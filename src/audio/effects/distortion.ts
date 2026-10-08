import type { ParamValue } from '../../state/types';
import { num, str } from './descriptors';
import { Effect, mixGains, setParam } from './base';

export function makeCurve(shape: string, drive: number, size = 2048): Float32Array<ArrayBuffer> {
  const curve = new Float32Array(size);
  const k = 1 + drive * 60;
  for (let i = 0; i < size; i++) {
    const x = (i / (size - 1)) * 2 - 1;
    let y: number;
    switch (shape) {
      case 'hard':
        y = Math.max(-1, Math.min(1, x * k * 0.5));
        break;
      case 'fold': {
        let v = x * (1 + drive * 6);
        while (v > 1 || v < -1) v = v > 1 ? 2 - v : -2 - v;
        y = v;
        break;
      }
      case 'bit': {
        const steps = Math.max(2, Math.round(Math.pow(2, 8 - drive * 6.5)));
        y = Math.round(x * steps) / steps;
        break;
      }
      default:
        y = Math.tanh(x * k * 0.35) / Math.tanh(k * 0.35);
    }
    curve[i] = y;
  }
  return curve;
}

export class DistortionEffect extends Effect {
  private shaper: WaveShaperNode;
  private pre: GainNode;
  private tone: BiquadFilterNode;
  private post: GainNode;
  private wet: GainNode;
  private dry: GainNode;
  private curveKey = '';
  curve: Float32Array<ArrayBuffer> = makeCurve('soft', 0.4);

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.pre = this.track(ctx.createGain());
    this.shaper = this.track(ctx.createWaveShaper());
    this.shaper.oversample = '4x';
    this.tone = this.track(ctx.createBiquadFilter());
    this.tone.type = 'lowpass';
    this.post = this.track(ctx.createGain());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.input.connect(this.dry).connect(this.post);
    this.input.connect(this.pre).connect(this.shaper).connect(this.tone).connect(this.wet).connect(this.post);
    this.post.connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const shape = str(p, 'shape', 'soft');
    const drive = num(p, 'drive', 0.4);
    const key = `${shape}|${drive.toFixed(3)}`;
    if (key !== this.curveKey) {
      this.curveKey = key;
      this.curve = makeCurve(shape, drive);
      this.shaper.curve = this.curve;
    }
    setParam(this.pre.gain, 1 + drive * 2, this.ctx);
    setParam(this.tone.frequency, num(p, 'tone', 5000), this.ctx);
    setParam(this.post.gain, Math.pow(10, num(p, 'output', -3) / 20), this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}
