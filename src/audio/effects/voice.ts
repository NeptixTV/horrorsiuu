import type { ParamValue } from '../../state/types';
import { num } from './descriptors';
import { Effect, mixGains, setParam } from './base';
import { workletsReady } from '../worklets';

/**
 * Voice changer: formant-ish pitch shift (goofy-pitch worklet in transpose
 * mode), ring modulator for robot / alien voices, tone filter.
 */
export class VoiceEffect extends Effect {
  private shifter: AudioWorkletNode | null = null;
  private ringDry: GainNode;
  private ringWet: GainNode;
  private ringOsc: OscillatorNode;
  private tone: BiquadFilterNode;
  private lowShelf: BiquadFilterNode;
  private highShelf: BiquadFilterNode;
  private wet: GainNode;
  private dry: GainNode;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.ringDry = this.track(ctx.createGain());
    this.ringWet = this.track(ctx.createGain());
    this.ringWet.gain.value = 0;
    const ringMod = this.track(ctx.createGain());
    ringMod.gain.value = 0;
    this.ringOsc = this.source(ctx.createOscillator());
    this.ringOsc.connect(ringMod.gain);
    this.tone = this.track(ctx.createBiquadFilter());
    this.tone.type = 'lowpass';
    this.lowShelf = this.track(ctx.createBiquadFilter());
    this.lowShelf.type = 'lowshelf';
    this.lowShelf.frequency.value = 500;
    this.highShelf = this.track(ctx.createBiquadFilter());
    this.highShelf.type = 'highshelf';
    this.highShelf.frequency.value = 2500;
    this.wet = this.track(ctx.createGain());
    this.dry = this.track(ctx.createGain());

    const sum = this.track(ctx.createGain());
    let head: AudioNode = this.input;
    if (workletsReady(ctx)) {
      this.shifter = this.track(new AudioWorkletNode(ctx, 'goofy-pitch', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] }));
      this.shifter.port.postMessage({ type: 'params', correction: 0, analysis: false, mix: 1, retune: 0, humanize: 0 });
      this.input.connect(this.shifter);
      head = this.shifter;
    }
    head.connect(this.ringDry).connect(sum);
    head.connect(ringMod).connect(this.ringWet).connect(sum);
    sum.connect(this.lowShelf).connect(this.highShelf).connect(this.tone).connect(this.wet).connect(this.output);
    this.input.connect(this.dry).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const transpose = num(p, 'transpose', 0) + num(p, 'fine', 0) / 100;
    this.shifter?.port.postMessage({ type: 'params', transpose, correction: 0, analysis: false, mix: 1 });
    const robot = num(p, 'robot', 0);
    setParam(this.ringDry.gain, 1 - robot, this.ctx);
    setParam(this.ringWet.gain, robot * 1.4, this.ctx);
    setParam(this.ringOsc.frequency, num(p, 'robotFreq', 90), this.ctx);
    setParam(this.tone.frequency, num(p, 'tone', 18000), this.ctx);
    const f = num(p, 'formant', 0);
    setParam(this.lowShelf.gain, -f * 0.6, this.ctx);
    setParam(this.highShelf.gain, f * 0.9, this.ctx);
    const [d, w] = mixGains(num(p, 'mix', 1));
    setParam(this.dry.gain, d, this.ctx);
    setParam(this.wet.gain, w, this.ctx);
  }
}
