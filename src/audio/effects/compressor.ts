import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, setParam } from './base';

export class CompressorEffect extends Effect {
  readonly comp: DynamicsCompressorNode;
  private makeup: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.comp = this.track(ctx.createDynamicsCompressor());
    this.makeup = this.track(ctx.createGain());
    this.input.connect(this.comp);
    this.comp.connect(this.makeup);
    this.makeup.connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    setParam(this.comp.threshold, num(p, 'threshold', -18), this.ctx);
    setParam(this.comp.ratio, num(p, 'ratio', 4), this.ctx);
    setParam(this.comp.attack, num(p, 'attack', 10) / 1000, this.ctx);
    setParam(this.comp.release, num(p, 'release', 160) / 1000, this.ctx);
    setParam(this.comp.knee, num(p, 'knee', 6), this.ctx);
    setParam(this.makeup.gain, Math.pow(10, num(p, 'makeup') / 20), this.ctx);
  }

  /** Current gain reduction in dB (negative or 0). */
  get reduction(): number {
    return this.comp.reduction;
  }
}
