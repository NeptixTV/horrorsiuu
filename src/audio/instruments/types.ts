import type { Channel } from '../../state/types';

/** Something that can be played with notes (synth, drum voice, sampler). */
export interface Instrument {
  readonly output: AudioNode;
  noteOn(pitch: number, velocity: number, time: number, duration?: number): void;
  noteOff(pitch: number, time: number): void;
  allNotesOff(time?: number): void;
  update(channel: Channel): void;
  activeVoices(): number;
  dispose(): void;
}

/** Ramps an AudioParam to hold its current value at `time` (cancelAndHold fallback). */
export function holdAt(param: AudioParam, time: number, fallbackValue: number) {
  const p = param as AudioParam & { cancelAndHoldAtTime?: (t: number) => AudioParam };
  if (typeof p.cancelAndHoldAtTime === 'function') {
    p.cancelAndHoldAtTime(time);
  } else {
    param.cancelScheduledValues(time);
    param.setValueAtTime(fallbackValue, time);
  }
}

const noiseCache = new WeakMap<BaseAudioContext, AudioBuffer>();
export function noiseBuffer(ctx: BaseAudioContext): AudioBuffer {
  let b = noiseCache.get(ctx);
  if (!b) {
    b = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate);
    const d = b.getChannelData(0);
    let seed = 22222;
    for (let i = 0; i < d.length; i++) {
      seed = (seed * 1664525 + 1013904223) >>> 0;
      d[i] = (seed / 4294967296) * 2 - 1;
    }
    noiseCache.set(ctx, b);
  }
  return b;
}
