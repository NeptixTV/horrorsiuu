import type { ParamValue } from '../../state/types';
import { num, str } from './descriptors';
import { Effect, mixGains, setParam, type EffectEnv } from './base';

const SYNC: Record<string, number> = { '1/4': 1, '1/8': 0.5, '1/8d': 0.75, '1/16': 0.25, '1/4t': 2 / 3 };

/** Stereo ping-pong delay with filtered feedback. */
export class DelayEffect extends Effect {
  private dl: DelayNode;
  private dr: DelayNode;
  private fb: GainNode;
  private fb2: GainNode;
  private tone: BiquadFilterNode;
  private panL: StereoPannerNode;
  private panR: StereoPannerNode;
  private wet: GainNode;
  private dry: GainNode;
  private params: Record<string, ParamValue> = {};
  private bpm = 120;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.dl = this.track(ctx.createDelay(4));
    this.dr = this.track(ctx.createDelay(4));
    this.fb = this.track(ctx.createGain());
    this.fb2 = this.track(ctx.createGain());
    this.tone = this.track(ctx.createBiquadFilter());
    this.tone.type = 'lowpass';
    this.panL = this.track(ctx.createStereoPanner());
    this.panR = this.track(ctx.createStereoPanner());
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());

    this.input.connect(this.dry).connect(this.output);
    this.input.connect(this.dl);
    // L → tone → fb → R → fb2 → L : every repeat is attenuated by `feedback`
    this.dl.connect(this.tone).connect(this.fb).connect(this.dr);
    this.dr.connect(this.fb2).connect(this.dl);
    this.dl.connect(this.panL).connect(this.wet);
    this.dr.connect(this.panR).connect(this.wet);
    this.wet.connect(this.output);
  }

  /** Effective delay time in seconds. */
  get time(): number {
    const sync = str(this.params, 'sync', 'off');
    if (sync !== 'off' && SYNC[sync]) return (60 / this.bpm) * SYNC[sync];
    return num(this.params, 'time', 375) / 1000;
  }

  get feedback(): number {
    return num(this.params, 'feedback', 0.4);
  }

  update(p: Record<string, ParamValue>, env: EffectEnv) {
    this.params = p;
    this.bpm = env.bpm;
    this.apply();
  }

  setTempo(bpm: number) {
    this.bpm = bpm;
    this.apply();
  }

  private apply() {
    const p = this.params;
    const t = Math.min(3.9, this.time);
    setParam(this.dl.delayTime, t, this.ctx, 0.05);
    setParam(this.dr.delayTime, t, this.ctx, 0.05);
    setParam(this.fb.gain, num(p, 'feedback', 0.4), this.ctx);
    setParam(this.fb2.gain, num(p, 'feedback', 0.4), this.ctx);
    setParam(this.tone.frequency, num(p, 'tone', 6000), this.ctx);
    const pp = num(p, 'pingpong', 0.5);
    setParam(this.panL.pan, -pp, this.ctx);
    setParam(this.panR.pan, pp, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 0.3));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}
