import type { ParamValue } from '../../state/types';
import { scaleMask } from '../../midi/music';
import { num, str } from './descriptors';
import { Effect, setParam } from './base';
import { workletsReady } from '../worklets';

export interface PitchInfo {
  detected: number; // fractional MIDI note, -1 = unvoiced
  target: number; // MIDI note in scale, -1 = none
  shift: number; // semitones currently applied
  level: number;
  time: number;
}

/**
 * Pitch correction ("Goofy Tune"). The DSP runs in the goofy-pitch
 * AudioWorklet; the formant control is a spectral tilt approximation
 * (shelving filters) applied after the shifter.
 */
export class PitchEffect extends Effect {
  private node: AudioWorkletNode | null = null;
  private formantLow: BiquadFilterNode;
  private formantHigh: BiquadFilterNode;
  info: PitchInfo = { detected: -1, target: -1, shift: 0, level: 0, time: 0 };
  readonly available: boolean;

  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.formantLow = this.track(ctx.createBiquadFilter());
    this.formantLow.type = 'lowshelf';
    this.formantLow.frequency.value = 500;
    this.formantHigh = this.track(ctx.createBiquadFilter());
    this.formantHigh.type = 'highshelf';
    this.formantHigh.frequency.value = 2500;
    this.available = workletsReady(ctx);
    if (this.available) {
      this.node = this.track(new AudioWorkletNode(ctx, 'goofy-pitch', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] }));
      this.node.port.onmessage = (e) => {
        const m = e.data;
        if (m.type === 'pitch') this.info = { detected: m.detected, target: m.target, shift: m.shift, level: m.level, time: performance.now() };
      };
      this.input.connect(this.node).connect(this.formantLow);
    } else {
      this.input.connect(this.formantLow);
    }
    this.formantLow.connect(this.formantHigh).connect(this.output);
  }

  update(p: Record<string, ParamValue>) {
    const key = parseInt(str(p, 'key', '0'), 10) || 0;
    const mask = scaleMask(key, str(p, 'scale', 'Major'));
    this.node?.port.postMessage({
      type: 'params',
      mask,
      correction: num(p, 'correction', 1),
      retune: num(p, 'retune', 20),
      humanize: num(p, 'humanize', 0.2),
      mix: num(p, 'mix', 1),
      bypass: false,
    });
    const f = num(p, 'formant', 0);
    setParam(this.formantLow.gain, -f * 0.6, this.ctx);
    setParam(this.formantHigh.gain, f * 0.9, this.ctx);
  }
}
