// Renders audio through an effect chain offline (sample editor "apply
// effects", saving clips with their effects to the library).
import type { EffectInstance } from '../state/types';
import { InsertChain } from './mixgraph';
import { loadWorklets, markWorkletsReady } from './worklets';

export interface RenderFxOptions {
  start?: number; // seconds into the source
  duration?: number; // seconds of source material
  gain?: number;
  speed?: number;
  reverse?: boolean;
  fadeIn?: number;
  fadeOut?: number;
  bpm?: number;
}

const TAIL_TYPES = new Set(['reverb', 'delay']);

export function reverseBuffer(b: AudioBuffer): AudioBuffer {
  const r = new AudioBuffer({ length: b.length, numberOfChannels: b.numberOfChannels, sampleRate: b.sampleRate });
  for (let c = 0; c < b.numberOfChannels; c++) {
    const s = b.getChannelData(c);
    const d = r.getChannelData(c);
    for (let i = 0, n = s.length; i < n; i++) d[i] = s[n - 1 - i];
  }
  return r;
}

/** Removes trailing near-silence (keeps at least `keep` seconds). */
export function trimTail(b: AudioBuffer, keep: number): AudioBuffer {
  const thr = 0.0003;
  let last = Math.floor(keep * b.sampleRate);
  for (let c = 0; c < b.numberOfChannels; c++) {
    const d = b.getChannelData(c);
    for (let i = d.length - 1; i > last; i--) {
      if (Math.abs(d[i]) > thr) { last = i; break; }
    }
  }
  const len = Math.min(b.length, last + Math.floor(0.02 * b.sampleRate));
  if (len >= b.length) return b;
  const out = new AudioBuffer({ length: Math.max(1, len), numberOfChannels: b.numberOfChannels, sampleRate: b.sampleRate });
  for (let c = 0; c < b.numberOfChannels; c++) out.copyToChannel(b.getChannelData(c).subarray(0, len), c);
  return out;
}

export async function renderWithEffects(source: AudioBuffer, effects: EffectInstance[], o: RenderFxOptions = {}): Promise<AudioBuffer> {
  const buffer = o.reverse ? reverseBuffer(source) : source;
  const sr = buffer.sampleRate;
  const speed = Math.max(0.05, o.speed ?? 1);
  const start = Math.max(0, o.start ?? 0);
  const srcDur = Math.max(0.001, Math.min(o.duration ?? buffer.duration - start, buffer.duration - start));
  const outDur = srcDur / speed;
  const active = effects.filter((e) => !e.bypass);
  const tail = active.some((e) => TAIL_TYPES.has(e.type)) ? 4 : 0.05;
  const ctx = new OfflineAudioContext(2, Math.ceil((outDur + tail) * sr), sr);
  if (await loadWorklets(ctx)) markWorkletsReady(ctx);
  const chain = new InsertChain(ctx);
  chain.apply(effects, { bpm: o.bpm ?? 120 });
  chain.output.connect(ctx.destination);
  const src = ctx.createBufferSource();
  src.buffer = buffer;
  src.playbackRate.value = speed;
  const g = ctx.createGain();
  const level = o.gain ?? 1;
  const fi = Math.max(0.002, o.fadeIn ?? 0);
  const fo = Math.max(0.002, o.fadeOut ?? 0);
  g.gain.setValueAtTime(0, 0);
  g.gain.linearRampToValueAtTime(level, Math.min(fi, outDur / 2));
  g.gain.setValueAtTime(level, Math.max(Math.min(fi, outDur / 2), outDur - fo));
  g.gain.linearRampToValueAtTime(0, outDur);
  src.connect(g).connect(chain.input);
  src.start(0, start, srcDur);
  const out = await ctx.startRendering();
  return tail > 0.1 ? trimTail(out, outDur) : out;
}
