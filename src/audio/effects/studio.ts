// Tape machine, stereo width and noise gate.
import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, mixGains, setParam } from './base';
import { workletsReady } from '../worklets';
import { clampFinite } from '../safety';

/** tanh soft clipping, normalised so ±1 stays ±1. */
export function tapeCurve(drive: number, size = 1024): Float32Array<ArrayBuffer> {
  const k = 1 + clampFinite(drive, 0, 1, 0.35) * 4;
  const norm = Math.tanh(k);
  const c = new Float32Array(size);
  for (let i = 0; i < size; i++) {
    const x = (i / (size - 1)) * 2 - 1;
    c[i] = Math.tanh(k * x) / norm;
  }
  return c;
}

/**
 * Cassette / reel-to-reel character: pitch wobble (slow "wow" from two
 * detuned LFOs plus fast "flutter"), soft tape saturation with a little head
 * bump in the lows, a high cut and optional hiss.
 */
export class TapeEffect extends Effect {
  private delay: DelayNode;
  private wowA: OscillatorNode;
  private wowB: OscillatorNode;
  private flutter: OscillatorNode;
  private wowGain: GainNode;
  private flutterGain: GainNode;
  private shaper: WaveShaperNode;
  private post: GainNode;
  private bump: BiquadFilterNode;
  private tone: BiquadFilterNode;
  private hissGain: GainNode;
  private wet: GainNode;
  private dry: GainNode;
  private drive = -1;
  curve: Float32Array<ArrayBuffer> = tapeCurve(0.35);

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.delay = this.track(ctx.createDelay(0.05));
    this.delay.delayTime.value = 0.012;
    this.wowA = this.source(ctx.createOscillator());
    this.wowA.frequency.value = 0.55;
    this.wowB = this.source(ctx.createOscillator());
    this.wowB.frequency.value = 0.83;
    this.flutter = this.source(ctx.createOscillator());
    this.flutter.type = 'triangle';
    this.flutter.frequency.value = 7.3;
    this.wowGain = this.track(ctx.createGain());
    this.flutterGain = this.track(ctx.createGain());
    this.wowA.connect(this.wowGain);
    this.wowB.connect(this.wowGain);
    this.flutter.connect(this.flutterGain);
    this.wowGain.connect(this.delay.delayTime);
    this.flutterGain.connect(this.delay.delayTime);

    this.shaper = this.track(ctx.createWaveShaper());
    this.shaper.oversample = '2x';
    this.post = this.track(ctx.createGain());
    this.bump = this.track(ctx.createBiquadFilter());
    this.bump.type = 'lowshelf';
    this.bump.frequency.value = 110;
    this.tone = this.track(ctx.createBiquadFilter());
    this.tone.type = 'lowpass';
    this.tone.Q.value = 0.5;
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.delay).connect(this.shaper).connect(this.post).connect(this.bump).connect(this.tone).connect(this.wet).connect(this.output);

    // hiss: 2 s of looped stereo noise, high-passed
    const len = Math.round(ctx.sampleRate * 2);
    const noise = ctx.createBuffer(2, len, ctx.sampleRate);
    let seed = 12345;
    for (let c = 0; c < 2; c++) {
      const d = noise.getChannelData(c);
      for (let i = 0; i < len; i++) {
        seed = (seed * 1664525 + 1013904223) >>> 0;
        d[i] = seed / 2147483648 - 1;
      }
    }
    const src = ctx.createBufferSource();
    src.buffer = noise;
    src.loop = true;
    this.source(src);
    const hp = this.track(ctx.createBiquadFilter());
    hp.type = 'highpass';
    hp.frequency.value = 2500;
    this.hissGain = this.track(ctx.createGain());
    this.hissGain.gain.value = 0;
    src.connect(hp).connect(this.hissGain).connect(this.wet);
  }

  update(p: Record<string, ParamValue>) {
    const drive = clampFinite(num(p, 'drive', 0.35), 0, 1, 0.35);
    if (drive !== this.drive) {
      this.drive = drive;
      this.curve = tapeCurve(drive);
      this.shaper.curve = this.curve;
    }
    // the curve's slope at 0 is k / tanh(k); compensate most of it so more
    // saturation means "warmer and denser", not just "louder"
    const k = 1 + drive * 4;
    setParam(this.post.gain, Math.pow(Math.tanh(k) / k, 0.9), this.ctx);
    setParam(this.bump.gain, drive * 3, this.ctx);
    setParam(this.wowGain.gain, clampFinite(num(p, 'wow', 0.25), 0, 1, 0.25) * 0.0012, this.ctx);
    setParam(this.flutterGain.gain, clampFinite(num(p, 'flutter', 0.2), 0, 1, 0.2) * 0.00018, this.ctx);
    setParam(this.tone.frequency, clampFinite(num(p, 'tone', 9000), 1000, 20000, 9000), this.ctx);
    const hiss = clampFinite(num(p, 'hiss', 0), 0, 1, 0);
    setParam(this.hissGain.gain, hiss * hiss * 0.03, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}

/**
 * Mid/side stereo width: 0 = mono, 1 = unchanged, 2 = twice the side signal.
 * "Mono bass" high-passes the side signal so the low end stays centred.
 */
export class WidthEffect extends Effect {
  private sideAmt: GainNode;
  private sideHp: BiquadFilterNode;
  private out: GainNode;
  readonly scopeL: AnalyserNode | null = null;
  readonly scopeR: AnalyserNode | null = null;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    // a mono input is spread to both sides first (speakers up-mix)
    const pre = this.track(ctx.createGain());
    pre.channelCount = 2;
    pre.channelCountMode = 'explicit';
    pre.channelInterpretation = 'speakers';
    const split = this.track(ctx.createChannelSplitter(2));
    const halfL = this.track(ctx.createGain());
    const halfR = this.track(ctx.createGain());
    halfL.gain.value = 0.5;
    halfR.gain.value = 0.5;
    const negR = this.track(ctx.createGain());
    negR.gain.value = -1;
    const mid = this.track(ctx.createGain());
    const side = this.track(ctx.createGain());
    this.sideHp = this.track(ctx.createBiquadFilter());
    this.sideHp.type = 'highpass';
    this.sideHp.Q.value = 0.7071;
    this.sideAmt = this.track(ctx.createGain());
    const inv = this.track(ctx.createGain());
    inv.gain.value = -1;
    const merger = this.track(ctx.createChannelMerger(2));
    this.out = this.track(ctx.createGain());
    this.input.connect(pre).connect(split);
    split.connect(halfL, 0);
    split.connect(halfR, 1);
    halfL.connect(mid);
    halfR.connect(mid);
    halfL.connect(side);
    halfR.connect(negR).connect(side);
    side.connect(this.sideHp).connect(this.sideAmt);
    mid.connect(merger, 0, 0);
    mid.connect(merger, 0, 1);
    this.sideAmt.connect(merger, 0, 0);
    this.sideAmt.connect(inv).connect(merger, 0, 1);
    merger.connect(this.out).connect(this.output);
    if (!(ctx instanceof OfflineAudioContext)) {
      const scope = this.track(ctx.createChannelSplitter(2));
      const l = this.track(ctx.createAnalyser());
      const r = this.track(ctx.createAnalyser());
      l.fftSize = 512;
      r.fftSize = 512;
      this.out.connect(scope);
      scope.connect(l, 0);
      scope.connect(r, 1);
      this.scopeL = l;
      this.scopeR = r;
    }
  }

  update(p: Record<string, ParamValue>) {
    setParam(this.sideAmt.gain, clampFinite(num(p, 'width', 1.4), 0, 2, 1), this.ctx);
    const mono = clampFinite(num(p, 'monoBass', 120), 0, 400, 0);
    setParam(this.sideHp.frequency, mono < 1 ? 10 : mono, this.ctx);
    setParam(this.out.gain, Math.pow(10, clampFinite(num(p, 'gain', 0), -24, 12, 0) / 20), this.ctx);
  }
}

export interface GateState {
  gain: number;
  minGain: number;
  peak: number;
  open: boolean;
  time: number;
}

/** Noise gate / expander (AudioWorklet). Passes the signal unchanged if worklets are unavailable. */
export class GateEffect extends Effect {
  private node: AudioWorkletNode | null = null;
  readonly available: boolean;
  state: GateState = { gain: 1, minGain: 1, peak: 0, open: true, time: 0 };

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.available = workletsReady(ctx);
    if (this.available) {
      this.node = this.track(new AudioWorkletNode(ctx, 'goofy-gate', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] }));
      this.node.port.onmessage = (e) => {
        const m = e.data;
        if (m?.type === 'state') this.state = { gain: m.gain, minGain: m.minGain, peak: m.peak, open: m.open, time: performance.now() };
      };
      this.input.connect(this.node).connect(this.output);
    } else {
      this.input.connect(this.output);
    }
  }

  update(p: Record<string, ParamValue>) {
    this.node?.port.postMessage({
      type: 'params',
      threshold: num(p, 'threshold', -45),
      range: num(p, 'range', -80),
      attack: num(p, 'attack', 1),
      hold: num(p, 'hold', 40),
      release: num(p, 'release', 120),
    });
  }

  dispose() {
    if (this.node) this.node.port.onmessage = null;
    super.dispose();
  }
}
