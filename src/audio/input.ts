// Microphone / line input: device selection, input gain, metering, monitoring
// through a mixer insert and sample-accurate capture via the recorder worklet.
import { releaseWorklet, workletsReady } from './worklets';
import { diagnostics } from './diagnostics';

/** Labels Windows / macOS use for Bluetooth headsets in low-quality call mode. */
const HFP = /hands[- ]?free|freisprech|headset|hfp|\bag audio\b/i;
export function isBluetoothCallDevice(label: string) {
  return HFP.test(label);
}

export interface DeviceInfo {
  deviceId: string;
  label: string;
}

export interface CaptureState {
  startTime: number;
  peaks: number[];
  samples: number;
}

const PEAK_BLOCK = 1024;

export class AudioInput {
  stream: MediaStream | null = null;
  deviceId = '';
  private source: MediaStreamAudioSourceNode | null = null;
  readonly gain: GainNode;
  readonly analyser: AnalyserNode;
  readonly monitor: GainNode;
  private recorder: AudioWorkletNode | null = null;
  private sink: GainNode;
  private monitorDest: AudioNode | null = null;
  private chunksL: Float32Array[] = [];
  private chunksR: Float32Array[] = [];
  capture: CaptureState | null = null;
  private stopResolve: (() => void) | null = null;
  monitoring = false;
  /** opened on purpose by the user (stays open); temporary opens are released after recording */
  pinned = false;
  /** label of the opened device (shown locally only, never logged) */
  label = '';
  gainValue = 1;
  error: string | null = null;
  /** called when the device disappears (unplugged) */
  onLost: (() => void) | null = null;
  private buf = new Float32Array(1024);

  constructor(private ctx: AudioContext) {
    this.gain = ctx.createGain();
    this.analyser = ctx.createAnalyser();
    this.analyser.fftSize = 1024;
    this.monitor = ctx.createGain();
    this.monitor.gain.value = 0;
    this.sink = ctx.createGain();
    this.sink.gain.value = 0;
    this.sink.connect(ctx.destination);
    this.gain.connect(this.analyser);
    this.gain.connect(this.monitor);
  }

  get isOpen() {
    return !!this.stream;
  }

  static async listDevices(): Promise<{ inputs: DeviceInfo[]; outputs: DeviceInfo[] }> {
    if (!navigator.mediaDevices?.enumerateDevices) return { inputs: [], outputs: [] };
    const all = await navigator.mediaDevices.enumerateDevices();
    const map = (kind: MediaDeviceKind) =>
      all.filter((d) => d.kind === kind).map((d, i) => ({ deviceId: d.deviceId, label: d.label || `${kind === 'audioinput' ? 'Input' : 'Output'} ${i + 1}` }));
    return { inputs: map('audioinput'), outputs: map('audiooutput') };
  }

  async open(deviceId = this.deviceId): Promise<boolean> {
    this.close();
    this.error = null;
    if (!navigator.mediaDevices?.getUserMedia) {
      this.error = 'This browser does not support audio input.';
      return false;
    }
    const constraints = (id: string): MediaStreamConstraints => ({
      audio: {
        deviceId: id ? { exact: id } : undefined,
        echoCancellation: false,
        noiseSuppression: false,
        autoGainControl: false,
        channelCount: { ideal: 2 },
      },
    });
    try {
      try {
        this.stream = await navigator.mediaDevices.getUserMedia(constraints(deviceId));
      } catch (err) {
        // the saved device is gone (unplugged) – use the default microphone instead
        if (deviceId && err instanceof Error && (err.name === 'OverconstrainedError' || err.name === 'NotFoundError')) {
          diagnostics.log('warn', 'input', 'Saved microphone not found – using the default input');
          this.stream = await navigator.mediaDevices.getUserMedia(constraints(''));
        } else {
          throw err;
        }
      }
    } catch (err) {
      this.error = err instanceof Error ? `${err.name}: ${err.message}` : String(err);
      this.stream = null;
      return false;
    }
    const track = this.stream.getAudioTracks()[0];
    this.deviceId = track?.getSettings().deviceId ?? deviceId;
    this.label = track?.label ?? '';
    if (track) {
      track.onended = () => {
        if (this.stream?.getAudioTracks()[0] !== track) return;
        diagnostics.log('warn', 'input', 'Microphone disconnected');
        this.close();
        this.onLost?.();
      };
    }
    this.source = this.ctx.createMediaStreamSource(this.stream);
    this.source.connect(this.gain);
    if (workletsReady(this.ctx) && !this.recorder) {
      this.recorder = new AudioWorkletNode(this.ctx, 'goofy-recorder', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [1] });
      this.recorder.port.onmessage = (e) => this.onRecorderMessage(e.data);
      this.gain.connect(this.recorder);
      this.recorder.connect(this.sink);
    }
    return true;
  }

  close() {
    if (this.capture) this.capture = null;
    try { this.source?.disconnect(); } catch { /* ignore */ }
    this.source = null;
    this.stream?.getTracks().forEach((t) => { t.onended = null; t.stop(); });
    this.stream = null;
    this.label = '';
    this.pinned = false;
  }

  /** Releases everything (engine restart). */
  dispose() {
    this.close();
    for (const n of [this.gain, this.analyser, this.monitor, this.sink, this.recorder]) {
      try { n?.disconnect(); } catch { /* ignore */ }
    }
    releaseWorklet(this.recorder);
    this.recorder = null;
  }

  setGain(v: number) {
    if (!Number.isFinite(v)) return;
    this.gainValue = v;
    this.gain.gain.setTargetAtTime(v, this.ctx.currentTime, 0.01);
  }

  setMonitorDestination(node: AudioNode) {
    if (this.monitorDest === node) return;
    if (this.monitorDest) {
      try { this.monitor.disconnect(this.monitorDest); } catch { /* ignore */ }
    }
    this.monitor.connect(node);
    this.monitorDest = node;
  }

  setMonitoring(on: boolean) {
    this.monitoring = on;
    if (on) this.pinned = true;
    this.monitor.gain.setTargetAtTime(on ? 1 : 0, this.ctx.currentTime, 0.01);
  }

  level(): number {
    this.analyser.getFloatTimeDomainData(this.buf);
    let p = 0;
    for (let i = 0; i < this.buf.length; i++) {
      const a = Math.abs(this.buf[i]);
      if (a > p) p = a;
    }
    return p;
  }

  private onRecorderMessage(m: { type: string; l?: Float32Array; r?: Float32Array }) {
    if (m.type === 'chunk' && m.l && m.r && this.capture) {
      this.chunksL.push(m.l);
      this.chunksR.push(m.r);
      for (let i = 0; i < m.l.length; i += PEAK_BLOCK) {
        let p = 0;
        const end = Math.min(m.l.length, i + PEAK_BLOCK);
        for (let s = i; s < end; s++) {
          const a = Math.max(Math.abs(m.l[s]), Math.abs(m.r[s]));
          if (a > p) p = a;
        }
        this.capture.peaks.push(p);
      }
      this.capture.samples += m.l.length;
    } else if (m.type === 'stopped') {
      this.stopResolve?.();
      this.stopResolve = null;
    }
  }

  get peakBlock() {
    return PEAK_BLOCK;
  }

  startCapture(at: number): boolean {
    if (!this.recorder || !this.stream) return false;
    this.chunksL = [];
    this.chunksR = [];
    this.capture = { startTime: at, peaks: [], samples: 0 };
    this.recorder.port.postMessage({ type: 'start', at });
    return true;
  }

  /** Stops capturing and returns the recording, trimming `trimSeconds` of latency. */
  async stopCapture(trimSeconds = 0): Promise<AudioBuffer | null> {
    if (!this.recorder || !this.capture) return null;
    const done = new Promise<void>((resolve) => {
      this.stopResolve = resolve;
      setTimeout(resolve, 1000);
    });
    this.recorder.port.postMessage({ type: 'stop' });
    await done;
    this.capture = null;
    const total = this.chunksL.reduce((n, c) => n + c.length, 0);
    const trim = Math.min(total, Math.max(0, Math.round(trimSeconds * this.ctx.sampleRate)));
    const length = total - trim;
    if (length < 128) return null;
    const buffer = this.ctx.createBuffer(2, length, this.ctx.sampleRate);
    const L = buffer.getChannelData(0);
    const R = buffer.getChannelData(1);
    let pos = -trim;
    for (let i = 0; i < this.chunksL.length; i++) {
      const cl = this.chunksL[i];
      const cr = this.chunksR[i];
      for (let s = 0; s < cl.length; s++, pos++) {
        if (pos >= 0) {
          L[pos] = cl[s];
          R[pos] = cr[s];
        }
      }
    }
    this.chunksL = [];
    this.chunksR = [];
    return buffer;
  }
}
