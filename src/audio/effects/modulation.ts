import type { ParamValue } from '../../state/types';
import { num, str } from './descriptors';
import { Effect, mixGains, setParam } from './base';

/** 4 stage all-pass phaser with feedback. */
export class PhaserEffect extends Effect {
  private stages: BiquadFilterNode[];
  private lfo: OscillatorNode;
  private lfoGain: GainNode;
  private fb: GainNode;
  private fbDelay: DelayNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.stages = Array.from({ length: 4 }, () => {
      const f = this.track(ctx.createBiquadFilter());
      f.type = 'allpass';
      f.Q.value = 0.7;
      return f;
    });
    this.lfo = this.source(ctx.createOscillator());
    this.lfoGain = this.track(ctx.createGain());
    this.fb = this.track(ctx.createGain());
    this.fbDelay = this.track(ctx.createDelay(0.01));
    this.fbDelay.delayTime.value = 0.0005;
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.lfo.connect(this.lfoGain);
    for (const s of this.stages) this.lfoGain.connect(s.detune);
    this.input.connect(this.dry).connect(this.output);
    let prev: AudioNode = this.input;
    for (const s of this.stages) {
      prev.connect(s);
      prev = s;
    }
    prev.connect(this.wet).connect(this.output);
    prev.connect(this.fb).connect(this.fbDelay).connect(this.stages[0]);
  }

  update(p: Record<string, ParamValue>) {
    for (const s of this.stages) setParam(s.frequency, num(p, 'freq', 900), this.ctx);
    setParam(this.lfo.frequency, num(p, 'rate', 0.5), this.ctx);
    setParam(this.lfoGain.gain, num(p, 'depth', 0.7) * 1800, this.ctx);
    setParam(this.fb.gain, num(p, 'feedback', 0.5), this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 0.5));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}

/** Short modulated delay with feedback (jet / comb sound). */
export class FlangerEffect extends Effect {
  private delay: DelayNode;
  private lfo: OscillatorNode;
  private lfoGain: GainNode;
  private fb: GainNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.delay = this.track(ctx.createDelay(0.05));
    this.lfo = this.source(ctx.createOscillator());
    this.lfo.type = 'triangle';
    this.lfoGain = this.track(ctx.createGain());
    this.fb = this.track(ctx.createGain());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.lfo.connect(this.lfoGain).connect(this.delay.delayTime);
    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.delay).connect(this.wet).connect(this.output);
    this.delay.connect(this.fb).connect(this.delay);
  }

  update(p: Record<string, ParamValue>) {
    const base = num(p, 'delay', 3) / 1000;
    setParam(this.delay.delayTime, base, this.ctx);
    setParam(this.lfoGain.gain, base * 0.95 * num(p, 'depth', 0.7), this.ctx);
    setParam(this.lfo.frequency, num(p, 'rate', 0.3), this.ctx);
    setParam(this.fb.gain, num(p, 'feedback', 0.6), this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 0.5));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}

/** Amplitude modulation. */
export class TremoloEffect extends Effect {
  private amp: GainNode;
  private lfo: OscillatorNode;
  private lfoGain: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.amp = this.track(ctx.createGain());
    this.lfo = this.source(ctx.createOscillator());
    this.lfoGain = this.track(ctx.createGain());
    this.lfo.connect(this.lfoGain).connect(this.amp.gain);
    this.input.connect(this.amp).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const depth = num(p, 'depth', 0.6);
    this.lfo.type = str(p, 'shape', 'sine') as OscillatorType;
    setParam(this.lfo.frequency, num(p, 'rate', 5), this.ctx);
    setParam(this.amp.gain, 1 - depth / 2, this.ctx);
    setParam(this.lfoGain.gain, depth / 2, this.ctx);
  }
}

/** "8D audio": the sound circles around the listener (pan + front/back cue). */
export class AutoPanEffect extends Effect {
  private panner: StereoPannerNode;
  private filter: BiquadFilterNode;
  private amp: GainNode;
  private lfo: OscillatorNode;
  private cos: OscillatorNode;
  private panDepth: GainNode;
  private filterDepth: GainNode;
  private ampDepth: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.panner = this.track(ctx.createStereoPanner());
    this.filter = this.track(ctx.createBiquadFilter());
    this.filter.type = 'lowpass';
    this.filter.frequency.value = 16000;
    this.amp = this.track(ctx.createGain());
    this.lfo = this.source(ctx.createOscillator());
    // cosine = same LFO shifted by 90° → "behind the head" when the pan crosses the centre
    this.cos = ctx.createOscillator();
    this.cos.setPeriodicWave(ctx.createPeriodicWave(new Float32Array([0, 1]), new Float32Array([0, 0])));
    this.sources.push(this.cos);
    this.nodes.push(this.cos);
    this.cos.start();
    this.panDepth = this.track(ctx.createGain());
    this.filterDepth = this.track(ctx.createGain());
    this.ampDepth = this.track(ctx.createGain());
    this.lfo.connect(this.panDepth).connect(this.panner.pan);
    this.cos.connect(this.filterDepth).connect(this.filter.detune);
    this.cos.connect(this.ampDepth).connect(this.amp.gain);
    this.input.connect(this.filter).connect(this.amp).connect(this.panner).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const rate = num(p, 'rate', 0.15);
    const motion = num(p, 'motion', 0.4);
    setParam(this.lfo.frequency, rate, this.ctx);
    setParam(this.cos.frequency, rate, this.ctx);
    setParam(this.panDepth.gain, num(p, 'depth', 1), this.ctx);
    setParam(this.filterDepth.gain, -1500 * motion, this.ctx);
    setParam(this.amp.gain, 1 - motion * 0.15, this.ctx);
    setParam(this.ampDepth.gain, -0.15 * motion, this.ctx);
  }
}

/** Loudness maximiser: drive into a fast, hard-knee compressor. */
export class LimiterEffect extends Effect {
  private drive: GainNode;
  readonly comp: DynamicsCompressorNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.drive = this.track(ctx.createGain());
    this.comp = this.track(ctx.createDynamicsCompressor());
    this.comp.ratio.value = 20;
    this.comp.knee.value = 0;
    this.comp.attack.value = 0.001;
    this.input.connect(this.drive).connect(this.comp).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    setParam(this.drive.gain, Math.pow(10, num(p, 'drive', 6) / 20), this.ctx);
    setParam(this.comp.threshold, num(p, 'ceiling', -0.5) - 1, this.ctx);
    setParam(this.comp.release, num(p, 'release', 80) / 1000, this.ctx);
  }

  get reduction() {
    return this.comp.reduction;
  }
}
