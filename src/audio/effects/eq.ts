import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, setParam } from './base';

/** 5 band EQ: low shelf, three peaking bands and a high shelf. */
export class EqEffect extends Effect {
  readonly bands: BiquadFilterNode[];
  private gain: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    const types: BiquadFilterType[] = ['lowshelf', 'peaking', 'peaking', 'peaking', 'highshelf'];
    this.bands = types.map((t) => {
      const f = this.track(ctx.createBiquadFilter());
      f.type = t;
      return f;
    });
    this.gain = this.track(ctx.createGain());
    let prev: AudioNode = this.input;
    for (const b of this.bands) {
      prev.connect(b);
      prev = b;
    }
    prev.connect(this.gain);
    this.gain.connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const keys = [['low', 'lowFreq'], ['lowMid', 'lowMidFreq'], ['mid', 'midFreq'], ['highMid', 'highMidFreq'], ['high', 'highFreq']];
    const q = num(p, 'q', 0.9);
    keys.forEach(([g, f], i) => {
      const b = this.bands[i];
      setParam(b.gain, num(p, g), this.ctx);
      setParam(b.frequency, num(p, f, 1000), this.ctx);
      if (b.type === 'peaking') setParam(b.Q, q, this.ctx);
    });
    setParam(this.gain.gain, Math.pow(10, num(p, 'gain') / 20), this.ctx);
  }

  /** Combined magnitude response in dB for the given frequencies. */
  response(freqs: Float32Array<ArrayBuffer>): Float32Array {
    const total = new Float32Array(freqs.length);
    const mag = new Float32Array(freqs.length);
    const phase = new Float32Array(freqs.length);
    for (const b of this.bands) {
      b.getFrequencyResponse(freqs, mag, phase);
      for (let i = 0; i < freqs.length; i++) total[i] += 20 * Math.log10(Math.max(1e-6, mag[i]));
    }
    const g = 20 * Math.log10(Math.max(1e-6, this.gain.gain.value));
    for (let i = 0; i < freqs.length; i++) total[i] += g;
    return total;
  }
}
