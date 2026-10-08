import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, mixGains, setParam } from './base';

/** Two modulated delay lines (LFOs in anti-phase) panned left and right. */
export class ChorusEffect extends Effect {
  private dl: DelayNode;
  private dr: DelayNode;
  private lfo: OscillatorNode;
  private depthL: GainNode;
  private depthR: GainNode;
  private panL: StereoPannerNode;
  private panR: StereoPannerNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.dl = this.track(ctx.createDelay(0.1));
    this.dr = this.track(ctx.createDelay(0.1));
    this.lfo = this.source(ctx.createOscillator());
    this.lfo.type = 'sine';
    this.depthL = this.track(ctx.createGain());
    this.depthR = this.track(ctx.createGain());
    this.panL = this.track(ctx.createStereoPanner());
    this.panR = this.track(ctx.createStereoPanner());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.lfo.connect(this.depthL).connect(this.dl.delayTime);
    this.lfo.connect(this.depthR).connect(this.dr.delayTime);
    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.dl).connect(this.panL).connect(this.wet);
    this.input.connect(this.dr).connect(this.panR).connect(this.wet);
    this.wet.connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const base = num(p, 'delay', 18) / 1000;
    const depth = num(p, 'depth', 0.5) * base * 0.8;
    setParam(this.dl.delayTime, base, this.ctx);
    setParam(this.dr.delayTime, base, this.ctx);
    setParam(this.depthL.gain, depth, this.ctx);
    setParam(this.depthR.gain, -depth, this.ctx);
    setParam(this.lfo.frequency, num(p, 'rate', 0.8), this.ctx);
    const spread = num(p, 'spread', 0.8);
    setParam(this.panL.pan, -spread, this.ctx);
    setParam(this.panR.pan, spread, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 0.5));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w * 0.8, this.ctx);
  }
}
