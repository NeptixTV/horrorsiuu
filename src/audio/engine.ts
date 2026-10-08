// The live audio engine singleton: owns the AudioContext, the project graph,
// the transport, the audio input and the master analysers.
import type { Project } from '../state/types';
import { MASTER_ID } from '../state/types';
import { loadWorklets, markWorkletsReady } from './worklets';
import { MixGraph } from './mixgraph';
import { Transport } from './transport';
import { AudioInput } from './input';
import { buildLibrary } from './library';

export type LatencyHint = 'interactive' | 'balanced' | 'playback';

export interface RecordingInfo {
  startTick: number;
  trackId: string;
  startTime: number;
}

export interface RecordingResult {
  buffer: AudioBuffer;
  startTick: number;
  trackId: string;
}

class Engine {
  ctx!: AudioContext;
  graph!: MixGraph;
  transport!: Transport;
  input!: AudioInput;
  masterOut!: GainNode;
  spectrum!: AnalyserNode;
  scope!: AnalyserNode;
  scopeR!: AnalyserNode;
  worklets = false;
  initialized = false;
  recording: RecordingInfo | null = null;
  latencyCompensation: number | null = null; // seconds; null = automatic
  private getProject: () => Project = () => { throw new Error('engine not attached'); };
  private listeners = new Set<() => void>();
  onRecordingComplete: ((r: RecordingResult) => void) | null = null;

  private initPromise: Promise<void> | null = null;

  init(latencyHint: LatencyHint, progress: (label: string, p: number) => void): Promise<void> {
    this.initPromise ??= this.doInit(latencyHint, progress);
    return this.initPromise;
  }

  private async doInit(latencyHint: LatencyHint, progress: (label: string, p: number) => void) {
    progress('Initializing audio engine…', 0.05);
    this.ctx = new AudioContext({ latencyHint });
    this.masterOut = this.ctx.createGain();
    this.masterOut.connect(this.ctx.destination);
    this.spectrum = this.ctx.createAnalyser();
    this.spectrum.fftSize = 4096;
    this.spectrum.smoothingTimeConstant = 0.78;
    this.scope = this.ctx.createAnalyser();
    this.scope.fftSize = 2048;
    this.scopeR = this.ctx.createAnalyser();
    this.scopeR.fftSize = 2048;
    const split = this.ctx.createChannelSplitter(2);
    this.masterOut.connect(this.spectrum);
    this.masterOut.connect(split);
    split.connect(this.scope, 0);
    split.connect(this.scopeR, 1);

    progress('Loading AudioWorklet processors…', 0.15);
    this.worklets = await loadWorklets(this.ctx);
    if (this.worklets) markWorkletsReady(this.ctx);

    progress('Rendering sample library…', 0.25);
    await buildLibrary((d, t) => progress(`Rendering sample library… (${d}/${t})`, 0.25 + 0.45 * (d / t)));

    this.graph = new MixGraph(this.ctx, this.masterOut);
    this.input = new AudioInput(this.ctx);
    this.initialized = true;
  }

  attach(getProject: () => Project) {
    this.getProject = getProject;
    this.transport = new Transport(this.ctx, this.graph, getProject);
    this.transport.onChange(() => this.emit());
    this.graph.sync(getProject());
  }

  sync(project: Project) {
    if (!this.initialized) return;
    this.graph.sync(project);
    if (this.input) this.input.setMonitorDestination(this.graph.stripInput(this.monitorTarget(project)));
  }

  monitorTarget(project: Project) {
    const vocals = project.mixer.find((m) => m.name.toLowerCase().includes('vocal'));
    return this.inputTrackId && project.mixer.some((m) => m.id === this.inputTrackId) ? this.inputTrackId : vocals?.id ?? MASTER_ID;
  }

  inputTrackId: string | null = null;

  onChange(l: () => void) {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }

  emit() {
    this.listeners.forEach((l) => l());
  }

  async resume() {
    if (this.ctx && this.ctx.state !== 'running') {
      try { await this.ctx.resume(); } catch { /* needs gesture */ }
    }
  }

  get running() {
    return this.ctx?.state === 'running';
  }

  previewNote(channelId: string, pitch: number, velocity = 0.8, duration?: number) {
    if (!this.initialized) return;
    void this.resume();
    this.graph.noteOn(channelId, pitch, velocity, this.ctx.currentTime + 0.005, duration);
  }

  previewNoteOff(channelId: string, pitch: number) {
    if (!this.initialized) return;
    this.graph.noteOff(channelId, pitch, this.ctx.currentTime);
  }

  /** Estimated round-trip latency compensation in seconds. */
  get roundTripLatency(): number {
    if (this.latencyCompensation !== null) return this.latencyCompensation;
    const c = this.ctx as AudioContext & { outputLatency?: number };
    return (c.baseLatency || 0) + (c.outputLatency || 0) + 0.01;
  }

  async startRecording(trackId: string): Promise<string | null> {
    if (this.recording) return null;
    await this.resume();
    if (!this.worklets) return 'AudioWorklet is not supported in this browser – recording is unavailable.';
    if (!this.input.isOpen) {
      const ok = await this.input.open();
      if (!ok) return this.input.error ?? 'Could not open the audio input.';
    }
    this.sync(this.getProject());
    let startTime: number;
    if (!this.transport.playing) {
      if (this.transport.mode !== 'song') this.transport.setMode('song');
      startTime = this.transport.play();
    } else {
      startTime = this.ctx.currentTime + 0.03;
    }
    const startTick = this.transport.tickAt(startTime);
    this.input.startCapture(startTime);
    this.recording = { startTick, trackId, startTime };
    this.emit();
    return null;
  }

  async stopRecording(): Promise<void> {
    const rec = this.recording;
    if (!rec) return;
    this.recording = null;
    const buffer = await this.input.stopCapture(this.roundTripLatency);
    this.emit();
    if (buffer && this.onRecordingComplete) this.onRecordingComplete({ buffer, startTick: rec.startTick, trackId: rec.trackId });
  }

  async setOutputDevice(deviceId: string): Promise<boolean> {
    const c = this.ctx as AudioContext & { setSinkId?: (id: string) => Promise<void> };
    if (!c.setSinkId) return false;
    try {
      await c.setSinkId(deviceId);
      return true;
    } catch {
      return false;
    }
  }

  get supportsOutputSelection() {
    return typeof (this.ctx as AudioContext & { setSinkId?: unknown })?.setSinkId === 'function';
  }

  get bufferSize(): number {
    return Math.round((this.ctx?.baseLatency || 128 / 44100) * (this.ctx?.sampleRate || 44100));
  }
}

export const engine = new Engine();
