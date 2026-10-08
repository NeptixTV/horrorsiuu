import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, setParam } from './base';

/** Generates a stereo impulse response: early reflections + exponential tail. */
export function makeImpulse(ctx: BaseAudioContext, size: number, decay: number): AudioBuffer {
  const sr = ctx.sampleRate;
  const len = Math.max(1, Math.floor(sr * Math.min(12, decay * (0.6 + size * 0.6))));
  const buf = ctx.createBuffer(2, len, sr);
  let seed = 1337;
  const rand = () => {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    return seed / 4294967296 * 2 - 1;
  };
  const early = Math.floor(sr * (0.005 + size * 0.06));
  for (let c = 0; c < 2; c++) {
    const d = buf.getChannelData(c);
    // diffuse tail
    for (let i = 0; i < len; i++) {
      const t = i / sr;
      const env = Math.exp((-6.9 * t) / decay);
      const fadeIn = i < early ? i / early : 1;
      d[i] = rand() * env * fadeIn * 0.6;
    }
    // early reflections
    const taps = 6 + Math.floor(size * 10);
    for (let k = 0; k < taps; k++) {
      const pos = Math.floor(early * (0.15 + 0.85 * Math.abs(rand())) * (1 + c * 0.07));
      if (pos < len) d[pos] += (0.8 - k / taps * 0.5) * (rand() > 0 ? 1 : -1);
    }
  }
  return buf;
}

export class ReverbEffect extends Effect {
  private convolver: ConvolverNode;
  private predelay: DelayNode;
  private damping: BiquadFilterNode;
  private wet: GainNode;
  private dry: GainNode;
  private irKey = '';
  private irTimer: ReturnType<typeof setTimeout> | null = null;
  decay = 2;
  size = 0.5;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.convolver = this.track(ctx.createConvolver());
    this.predelay = this.track(ctx.createDelay(1));
    this.damping = this.track(ctx.createBiquadFilter());
    this.damping.type = 'lowpass';
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());
    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.predelay).connect(this.convolver).connect(this.damping).connect(this.wet).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    this.size = num(p, 'size', 0.5);
    this.decay = num(p, 'decay', 2);
    setParam(this.predelay.delayTime, num(p, 'predelay', 10) / 1000, this.ctx);
    setParam(this.damping.frequency, num(p, 'damping', 7000), this.ctx);
    setParam(this.wet.gain, num(p, 'wet', 0.3), this.ctx);
    setParam(this.dry.gain, num(p, 'dry', 1), this.ctx);
    const key = `${this.size.toFixed(2)}|${this.decay.toFixed(2)}`;
    if (key !== this.irKey) {
      this.irKey = key;
      const build = () => {
        this.convolver.buffer = makeImpulse(this.ctx, this.size, this.decay);
      };
      // Rebuilding the IR is expensive: debounce while a knob is dragged.
      if (this.irTimer) clearTimeout(this.irTimer);
      if (!this.convolver.buffer || this.ctx instanceof OfflineAudioContext) build();
      else this.irTimer = setTimeout(build, 120);
    }
  }
}
