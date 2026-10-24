// Effects for beats: clipper, kick-triggered sidechain pump, three-band
// "OTT" style compressor, transient shaper and lo-fi crusher.
import type { Channel, ParamValue } from '../../state/types';
import { num, str } from './descriptors';
import { Effect, mixGains, setParam } from './base';
import { workletsReady } from '../worklets';
import { clampFinite } from '../safety';
import { holdAt, noiseBuffer } from '../instruments/types';
import { proSound } from '../pro/catalog';

const dbToGain = (db: number) => Math.pow(10, db / 20);

// ---------------------------------------------------------------- clipper

/** Input range of the curve (±16 = +24 dB of headroom before the curve's end). */
const CLIP_RANGE = 16;

/** Clipping curve: linear up to the knee, then a smooth bend into the ceiling. */
export function clipCurve(ceiling: number, softness: number, size = 4096): Float32Array<ArrayBuffer> {
  const c = clampFinite(ceiling, 0.05, 1, 0.9);
  const k = clampFinite(softness, 0, 1, 0.5);
  const t = c * (1 - k * 0.9);
  const curve = new Float32Array(size);
  for (let i = 0; i < size; i++) {
    const x = ((i / (size - 1)) * 2 - 1) * CLIP_RANGE;
    const a = Math.abs(x);
    let y = a <= t ? a : t + (c - t) * Math.tanh((a - t) / Math.max(1e-6, c - t));
    if (y > c) y = c;
    curve[i] = x < 0 ? -y : y;
  }
  return curve;
}

/**
 * The trap mastering trick: drive the signal into a clipper instead of a
 * compressor – drums stay punchy and get much louder. 2× oversampled.
 */
export class ClipperEffect extends Effect {
  private pre: GainNode;
  private shaper: WaveShaperNode;
  private post: GainNode;
  private wet: GainNode;
  private dry: GainNode;
  private key = '';
  curve: Float32Array<ArrayBuffer> = clipCurve(0.9, 0.5);

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.pre = this.track(ctx.createGain());
    this.shaper = this.track(ctx.createWaveShaper());
    // 2× is enough against aliasing for a clipper and costs half of 4×
    this.shaper.oversample = '2x';
    this.post = this.track(ctx.createGain());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.input.connect(this.pre).connect(this.shaper).connect(this.wet).connect(this.post);
    this.input.connect(this.dry).connect(this.post);
    this.post.connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const ceiling = dbToGain(clampFinite(num(p, 'ceiling', -1), -18, 0, -1));
    const soft = clampFinite(num(p, 'softness', 0.4), 0, 1, 0.4);
    const key = `${ceiling.toFixed(4)}|${soft.toFixed(3)}`;
    if (key !== this.key) {
      this.key = key;
      this.curve = clipCurve(ceiling, soft);
      this.shaper.curve = this.curve;
    }
    // the curve covers ±16 (its input is the signal / 16, its output is in real units)
    setParam(this.pre.gain, dbToGain(clampFinite(num(p, 'drive', 6), 0, 24, 6)) / CLIP_RANGE, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
    setParam(this.post.gain, dbToGain(clampFinite(num(p, 'output', 0), -24, 6, 0)), this.ctx);
  }
}

// ---------------------------------------------------------------- sidechain pump

/** Pump effects per audio context, triggered by the notes of kick (or drum) channels. */
const pumps = new WeakMap<BaseAudioContext, Set<PumpEffect>>();

export function isKickChannel(ch: Channel) {
  if (ch.kind === 'pro') return proSound(ch.pro?.sound)?.group === 'Kicks';
  if (ch.kind === 'drum') return ch.drum?.type === 'kick';
  return /kick|\bbd\b|bassdrum/i.test(ch.name);
}

export function isDrumHit(ch: Channel) {
  if (ch.kind === 'pro') return !!proSound(ch.pro?.sound)?.oneShot;
  return ch.kind === 'drum' || /kick|snare|clap|hat|perc|drum/i.test(ch.name);
}

/** Called by the mix graph for every note: kicks make the pumps duck. */
export function triggerPumps(ctx: BaseAudioContext, ch: Channel, time: number, velocity: number) {
  const set = pumps.get(ctx);
  if (!set?.size) return;
  const kick = isKickChannel(ch);
  const drum = kick || isDrumHit(ch);
  for (const p of set) {
    if (p.from === 'kick' ? kick : drum) p.trigger(time, velocity);
  }
}

/**
 * Sidechain "pump": turns the signal down every time the kick plays and lets
 * it swell back – the 808 / pads make room for the kick. Triggered by the
 * kick's notes, so it is exactly in time (also when exporting).
 */
export class PumpEffect extends Effect {
  readonly duck: GainNode;
  from: 'kick' | 'drums' = 'kick';
  private depth = 0.7;
  private attack = 0.003;
  private hold = 0.01;
  private release = 0.16;
  private last = -1;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.duck = this.track(ctx.createGain());
    this.input.connect(this.duck).connect(this.output);
    let set = pumps.get(ctx);
    if (!set) pumps.set(ctx, (set = new Set()));
    set.add(this);
  }

  update(p: Record<string, ParamValue>) {
    this.from = str(p, 'source', 'kick') === 'drums' ? 'drums' : 'kick';
    this.depth = clampFinite(num(p, 'depth', 0.7), 0, 1, 0.7);
    this.attack = clampFinite(num(p, 'attack', 3), 0.5, 50, 3) / 1000;
    this.hold = clampFinite(num(p, 'hold', 10), 0, 200, 10) / 1000;
    this.release = clampFinite(num(p, 'release', 160), 20, 1000, 160) / 1000;
  }

  trigger(time: number, velocity = 1) {
    const ctx = this.ctx;
    const t = Math.max(time, ctx.currentTime);
    // two kicks at the same moment (layered kicks) duck once
    if (Math.abs(t - this.last) < 0.004) return;
    this.last = t;
    const g = this.duck.gain;
    const floor = 1 - this.depth * clampFinite(0.55 + 0.45 * velocity, 0, 1, 1);
    holdAt(g, t, g.value);
    g.setTargetAtTime(floor, t, this.attack / 3);
    g.setTargetAtTime(1, t + this.attack + this.hold, this.release / 4);
  }

  dispose() {
    pumps.get(this.ctx)?.delete(this);
    super.dispose();
  }
}

// ---------------------------------------------------------------- 3-band "OTT"

/** Linkwitz-Riley (2 × Butterworth) filter pair. */
function lr4(ctx: BaseAudioContext, type: BiquadFilterType, f: number, track: <T extends AudioNode>(n: T) => T) {
  const a = track(ctx.createBiquadFilter());
  const b = track(ctx.createBiquadFilter());
  a.type = type; b.type = type;
  a.frequency.value = f; b.frequency.value = f;
  a.Q.value = Math.SQRT1_2; b.Q.value = Math.SQRT1_2;
  a.connect(b);
  return { input: a, output: b, set: (hz: number) => { a.frequency.value = hz; b.frequency.value = hz; } };
}

interface Band {
  comp: DynamicsCompressorNode;
  dry: GainNode;
  wet: GainNode;
  level: GainNode;
}

/**
 * Aggressive three-band compressor (the famous "OTT" sound): every band is
 * squashed hard and brought back up, which makes quiet details loud and
 * gives beats that bright, in-your-face sound. Depth blends it in. The dry
 * path runs through the same band split, so blending never smears the phase.
 */
export class MultibandEffect extends Effect {
  readonly bands: Band[] = [];
  private xLow: ReturnType<typeof lr4>[] = [];
  private xHigh: ReturnType<typeof lr4>[] = [];

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    const t = <T extends AudioNode>(n: T) => this.track(n);
    const lowLp = lr4(ctx, 'lowpass', 120, t);
    const midHp = lr4(ctx, 'highpass', 120, t);
    const midLp = lr4(ctx, 'lowpass', 2500, t);
    const highHp = lr4(ctx, 'highpass', 2500, t);
    this.xLow = [lowLp, midHp];
    this.xHigh = [midLp, highHp];
    this.input.connect(lowLp.input);
    this.input.connect(midHp.input);
    midHp.output.connect(midLp.input);
    this.input.connect(highHp.input);
    for (const src of [lowLp.output, midLp.output, highHp.output]) {
      const comp = t(ctx.createDynamicsCompressor());
      const dry = t(ctx.createGain());
      const wet = t(ctx.createGain());
      const level = t(ctx.createGain());
      src.connect(dry).connect(level);
      src.connect(comp).connect(wet).connect(level);
      level.connect(this.output);
      this.bands.push({ comp, dry, wet, level });
    }
  }

  update(p: Record<string, ParamValue>) {
    const depth = clampFinite(num(p, 'depth', 0.5), 0, 1, 0.5);
    const time = clampFinite(num(p, 'time', 1), 0.2, 3, 1);
    const ctx = this.ctx;
    const lowF = clampFinite(num(p, 'lowFreq', 120), 60, 400, 120);
    const highF = clampFinite(num(p, 'highFreq', 2500), 1000, 8000, 2500);
    for (const x of this.xLow) x.set(lowF);
    for (const x of this.xHigh) x.set(highF);
    const levels = [num(p, 'low', 0), num(p, 'mid', 0), num(p, 'high', 0)];
    // per band: threshold, ratio, attack (s), release (s), makeup (dB)
    const settings: [number, number, number, number, number][] = [
      [-36, 10, 0.006, 0.12, 14],
      [-34, 10, 0.004, 0.09, 13],
      [-38, 12, 0.002, 0.07, 15],
    ];
    this.bands.forEach((b, i) => {
      const [thr, ratio, atk, rel, makeup] = settings[i];
      setParam(b.comp.threshold, thr, ctx);
      setParam(b.comp.ratio, ratio, ctx);
      setParam(b.comp.knee, 6, ctx);
      setParam(b.comp.attack, atk * time, ctx);
      setParam(b.comp.release, rel * time, ctx);
      setParam(b.dry.gain, 1 - depth, ctx);
      setParam(b.wet.gain, depth * dbToGain(makeup * 0.75), ctx);
      setParam(b.level.gain, dbToGain(clampFinite(levels[i], -18, 18, 0)), ctx);
    });
    setParam(this.output.gain, dbToGain(clampFinite(num(p, 'output', 0), -24, 12, 0)), ctx);
  }
}

// ---------------------------------------------------------------- transient shaper

export class TransientEffect extends Effect {
  private node: AudioWorkletNode | null = null;
  readonly available: boolean;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.available = workletsReady(ctx);
    if (this.available) {
      this.node = this.track(new AudioWorkletNode(ctx, 'goofy-transient', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] }));
      this.input.connect(this.node).connect(this.output);
    } else {
      this.input.connect(this.output);
    }
  }

  update(p: Record<string, ParamValue>) {
    this.node?.port.postMessage({ type: 'params', attack: num(p, 'attack', 0.5), sustain: num(p, 'sustain', 0), output: num(p, 'output', 0) });
  }
}

// ---------------------------------------------------------------- lo-fi

/** Old sampler / tape sound: fewer bits, lower sample rate, dull tone, wobble and vinyl crackle. */
export class LofiEffect extends Effect {
  private crush: AudioWorkletNode | null = null;
  private tone: BiquadFilterNode;
  private hp: BiquadFilterNode;
  private wobble: DelayNode;
  private lfo: OscillatorNode;
  private lfoGain: GainNode;
  private noiseGain: GainNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.tone = this.track(ctx.createBiquadFilter());
    this.tone.type = 'lowpass';
    this.tone.Q.value = 0.6;
    this.hp = this.track(ctx.createBiquadFilter());
    this.hp.type = 'highpass';
    this.hp.frequency.value = 60;
    this.wobble = this.track(ctx.createDelay(0.05));
    this.wobble.delayTime.value = 0.008;
    this.lfo = this.source(ctx.createOscillator());
    this.lfo.frequency.value = 0.7;
    this.lfoGain = this.track(ctx.createGain());
    this.lfo.connect(this.lfoGain).connect(this.wobble.delayTime);
    let chain: AudioNode = this.input;
    if (workletsReady(ctx)) {
      this.crush = this.track(new AudioWorkletNode(ctx, 'goofy-lofi', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] }));
      chain = chain.connect(this.crush);
    }
    chain.connect(this.tone).connect(this.hp).connect(this.wobble).connect(this.wet).connect(this.output);
    this.input.connect(this.dry).connect(this.output);
    // vinyl crackle: sparse clicks on top of filtered noise
    const crackle = ctx.createBufferSource();
    crackle.buffer = crackleBuffer(ctx);
    crackle.loop = true;
    this.source(crackle);
    const nlp = this.track(ctx.createBiquadFilter());
    nlp.type = 'bandpass';
    nlp.frequency.value = 2500;
    nlp.Q.value = 0.5;
    this.noiseGain = this.track(ctx.createGain());
    this.noiseGain.gain.value = 0;
    crackle.connect(nlp).connect(this.noiseGain).connect(this.wet);
  }

  update(p: Record<string, ParamValue>) {
    const ctx = this.ctx;
    this.crush?.port.postMessage({ type: 'params', bits: num(p, 'bits', 10), rate: num(p, 'rate', 16000) });
    setParam(this.tone.frequency, clampFinite(num(p, 'tone', 6000), 800, 20000, 6000), ctx);
    setParam(this.lfoGain.gain, clampFinite(num(p, 'wobble', 0.3), 0, 1, 0.3) * 0.0016, ctx);
    const c = clampFinite(num(p, 'crackle', 0.3), 0, 1, 0.3);
    setParam(this.noiseGain.gain, c * c * 0.25, ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, ctx);
    setParam(this.wet.gain, w, ctx);
  }
}

const crackles = new WeakMap<BaseAudioContext, AudioBuffer>();
function crackleBuffer(ctx: BaseAudioContext): AudioBuffer {
  let b = crackles.get(ctx);
  if (!b) {
    const n = ctx.sampleRate * 3;
    b = ctx.createBuffer(2, n, ctx.sampleRate);
    const src = noiseBuffer(ctx).getChannelData(0);
    for (let c = 0; c < 2; c++) {
      const d = b.getChannelData(c);
      let seed = 777 + c * 31;
      for (let i = 0; i < n; i++) {
        seed = (seed * 1664525 + 1013904223) >>> 0;
        const r = seed / 4294967296;
        // a faint hiss plus a click now and then
        d[i] = src[(i * 7 + c * 13) % src.length] * 0.04 + (r < 0.0006 ? (r * 3000 - 0.9) * 0.9 : 0);
      }
    }
    crackles.set(ctx, b);
  }
  return b;
}
