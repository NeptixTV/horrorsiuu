import type { ParamValue } from '../../state/types';
import { num, str } from './descriptors';
import { Effect, mixGains, setParam } from './base';

/** Resonant multimode filter with an optional LFO sweep. */
export class FilterEffect extends Effect {
  readonly filter: BiquadFilterNode;
  private lfo: OscillatorNode;
  private lfoGain: GainNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.filter = this.track(ctx.createBiquadFilter());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.lfo = this.source(ctx.createOscillator());
    this.lfoGain = this.track(ctx.createGain());
    this.lfo.connect(this.lfoGain).connect(this.filter.detune);
    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.filter).connect(this.wet).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    this.filter.type = str(p, 'mode', 'lowpass') as BiquadFilterType;
    setParam(this.filter.frequency, num(p, 'cutoff', 2000), this.ctx);
    setParam(this.filter.Q, num(p, 'resonance', 1), this.ctx);
    setParam(this.lfo.frequency, num(p, 'lfoRate', 0.5), this.ctx);
    setParam(this.lfoGain.gain, num(p, 'lfoDepth', 0) * 2400, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }

  response(freqs: Float32Array<ArrayBuffer>): Float32Array {
    const mag = new Float32Array(freqs.length);
    const phase = new Float32Array(freqs.length);
    this.filter.getFrequencyResponse(freqs, mag, phase);
    return mag.map((m) => 20 * Math.log10(Math.max(1e-6, m)));
  }
}
