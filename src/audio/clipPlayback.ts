// Schedules one audio clip (or the remaining part of it) on a context.
// Shared by the live transport and the offline renderer so both sound the same.
import type { Project } from '../state/types';
import { assets } from './assets';
import { clampFinite, finite } from './safety';
import type { MixGraph } from './mixgraph';
import type { AudioEvent } from './sequencer';

export interface ScheduledAudio {
  src: AudioBufferSourceNode;
  gain: GainNode;
  end: number;
}

const MICRO_FADE = 0.003;

export function scheduleAudioClip(
  ctx: BaseAudioContext, graph: MixGraph, project: Project, ev: AudioEvent, time: number, spt: number,
): ScheduledAudio | null {
  const clip = ev.clip;
  if (!clip.assetId) return null;
  const buffer = clip.reverse ? assets.getReversed(clip.assetId) : assets.get(clip.assetId);
  if (!buffer) return null;
  if (!Number.isFinite(time) || !(spt > 0)) return null;
  const speed = clampFinite(clip.speed, 0.05, 8, 1);
  const offset = Math.max(0, finite(ev.sourceOffset, 0));
  if (offset >= buffer.duration) return null;
  // timeline duration (seconds) left in the clip and how much of the source that consumes
  const timeline = Math.min((buffer.duration - offset) / speed, finite(ev.remaining, 0) * spt);
  if (!(timeline > 0.001)) return null;

  const src = ctx.createBufferSource();
  src.buffer = buffer;
  src.playbackRate.value = speed;
  const g = ctx.createGain();
  const level = clampFinite(clip.gain, 0, 8, 1);
  const clipLen = clip.length * spt;
  const pos0 = Math.max(0, (ev.tick - clip.start) * spt); // where in the clip playback begins
  const fadeIn = Math.max(MICRO_FADE, finite(clip.fadeIn, 0));
  const fadeOut = Math.max(MICRO_FADE, finite(clip.fadeOut, 0));
  const env = (p: number) => {
    let v = level;
    if (p < fadeIn) v *= Math.max(0, p / fadeIn);
    if (p > clipLen - fadeOut) v *= Math.max(0, (clipLen - p) / fadeOut);
    return v;
  };
  const end = time + timeline;
  const playEnd = pos0 + timeline;
  // envelope breakpoints (clip-relative seconds): micro fades at both ends, user fades in between
  const pts = new Set<number>([pos0, pos0 + MICRO_FADE, playEnd - MICRO_FADE, playEnd]);
  if (fadeIn > pos0 && fadeIn < playEnd) pts.add(fadeIn);
  const foStart = clipLen - fadeOut;
  if (foStart > pos0 && foStart < playEnd) pts.add(foStart);
  const sorted = [...pts].filter((p) => p >= pos0 && p <= playEnd).sort((x, y) => x - y);
  sorted.forEach((p, i) => {
    const v = i === 0 || i === sorted.length - 1 ? 0 : env(p);
    const t = time + (p - pos0);
    if (i === 0) g.gain.setValueAtTime(v, t);
    else g.gain.linearRampToValueAtTime(v, t);
  });
  src.connect(g).connect(graph.clipInput(clip, project));
  src.start(time, offset, timeline * speed);
  return { src, gain: g, end };
}
