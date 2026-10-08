// Pure event collection: turns the project (patterns, playlist) into note and
// audio events for a tick range. Used by the live scheduler and the offline
// renderer, and unit tested without an AudioContext.
import type { Clip, Pattern, Project } from '../state/types';
import { PPQ, TICKS_PER_STEP } from '../state/types';
import { ticksPerBar } from '../state/utils';

export interface NoteEvent {
  tick: number; // absolute tick
  channelId: string;
  pitch: number;
  velocity: number;
  length: number; // ticks
  /** pattern-local step index for step sequencer events (UI highlighting) */
  step?: number;
}

export interface AudioEvent {
  tick: number; // absolute tick at which playback starts
  clip: Clip;
  /** seconds into the source at which playback starts */
  sourceOffset: number;
  /** ticks of the clip remaining from `tick` */
  remaining: number;
}

export type PlayMode = 'pattern' | 'song';

export function patternLength(pattern: Pattern, timeSig: [number, number] = [4, 4]): number {
  let len = pattern.lengthSteps * TICKS_PER_STEP;
  const bar = ticksPerBar(timeSig);
  for (const list of Object.values(pattern.notes)) {
    for (const n of list) {
      if (n.start + n.length > len) len = Math.ceil((n.start + n.length) / bar) * bar;
    }
  }
  return Math.max(TICKS_PER_STEP, len);
}

export function songLength(project: Project): number {
  let end = 0;
  for (const c of project.clips) end = Math.max(end, c.start + c.length);
  const bar = ticksPerBar(project.timeSig);
  return Math.max(bar * 4, Math.ceil(end / bar) * bar);
}

function activeChannels(project: Project): Set<string> {
  const anySolo = project.channels.some((c) => c.solo);
  const set = new Set<string>();
  for (const c of project.channels) {
    if (c.mute) continue;
    if (anySolo && !c.solo) continue;
    set.add(c.id);
  }
  return set;
}

/**
 * Events of one pattern in the pattern-local range [from, to).
 * `base` is added to produce absolute ticks; `limit` truncates note lengths.
 */
export function collectPatternEvents(
  project: Project, pattern: Pattern, from: number, to: number, base: number, out: NoteEvent[],
  active = activeChannels(project), limit = Infinity,
) {
  const swing = project.swing * TICKS_PER_STEP * 0.5;
  for (const ch of project.channels) {
    if (!active.has(ch.id)) continue;
    const steps = pattern.steps[ch.id];
    if (steps) {
      const n = Math.min(steps.length, pattern.lengthSteps);
      for (let i = 0; i < n; i++) {
        const v = steps[i];
        if (!v) continue;
        const t = i * TICKS_PER_STEP + (i % 2 === 1 ? swing : 0);
        if (t >= from && t < to) {
          out.push({ tick: base + t, channelId: ch.id, pitch: ch.rootNote, velocity: v, length: Math.min(TICKS_PER_STEP, limit - t), step: i });
        }
      }
    }
    const notes = pattern.notes[ch.id];
    if (notes) {
      for (const note of notes) {
        if (note.start >= from && note.start < to) {
          out.push({ tick: base + note.start, channelId: ch.id, pitch: note.pitch, velocity: note.velocity, length: Math.min(note.length, limit - note.start) });
        }
      }
    }
  }
}

/** Note events for absolute range [from, to) in the given mode. */
export function collectNoteEvents(project: Project, mode: PlayMode, patternId: string | null, from: number, to: number): NoteEvent[] {
  const out: NoteEvent[] = [];
  const active = activeChannels(project);
  if (mode === 'pattern') {
    const pattern = project.patterns.find((p) => p.id === patternId) ?? project.patterns[0];
    if (!pattern) return out;
    const len = patternLength(pattern, project.timeSig);
    // range may span several loops of the pattern
    let loopStart = Math.floor(from / len) * len;
    while (loopStart < to) {
      const a = Math.max(from, loopStart) - loopStart;
      const b = Math.min(to, loopStart + len) - loopStart;
      collectPatternEvents(project, pattern, a, b, loopStart, out, active, len);
      loopStart += len;
    }
    return out;
  }

  const mutedTracks = new Set(project.tracks.filter((t) => t.mute).map((t) => t.id));
  const patterns = new Map(project.patterns.map((p) => [p.id, p]));
  for (const clip of project.clips) {
    if (clip.kind !== 'pattern' || mutedTracks.has(clip.trackId)) continue;
    const clipEnd = clip.start + clip.length;
    if (clipEnd <= from || clip.start >= to) continue;
    const pattern = patterns.get(clip.patternId ?? '');
    if (!pattern) continue;
    const len = patternLength(pattern, project.timeSig);
    // content position 0 is at absolute tick (clip.start - clip.offset); content repeats every len
    const origin = clip.start - clip.offset;
    const a = Math.max(from, clip.start);
    const b = Math.min(to, clipEnd);
    let loopStart = origin + Math.floor((a - origin) / len) * len;
    while (loopStart < b) {
      const la = Math.max(a, loopStart) - loopStart;
      const lb = Math.min(b, loopStart + len) - loopStart;
      const limit = Math.min(len, clipEnd - loopStart);
      collectPatternEvents(project, pattern, la, lb, loopStart, out, active, limit);
      loopStart += len;
    }
  }
  out.sort((x, y) => x.tick - y.tick);
  return out;
}

/** Audio clips starting in [from, to). */
export function collectAudioStarts(project: Project, from: number, to: number): AudioEvent[] {
  const mutedTracks = new Set(project.tracks.filter((t) => t.mute).map((t) => t.id));
  const out: AudioEvent[] = [];
  for (const clip of project.clips) {
    if (clip.kind !== 'audio' || mutedTracks.has(clip.trackId)) continue;
    if (clip.start >= from && clip.start < to) out.push({ tick: clip.start, clip, sourceOffset: clip.offset, remaining: clip.length });
  }
  return out;
}

/** Audio clips that are already playing at `tick` (start < tick < end): used when starting mid-clip. */
export function collectAudioSpanning(project: Project, tick: number, bpm: number): AudioEvent[] {
  const mutedTracks = new Set(project.tracks.filter((t) => t.mute).map((t) => t.id));
  const out: AudioEvent[] = [];
  for (const clip of project.clips) {
    if (clip.kind !== 'audio' || mutedTracks.has(clip.trackId)) continue;
    if (clip.start < tick && clip.start + clip.length > tick) {
      const intoTicks = tick - clip.start;
      out.push({ tick, clip, sourceOffset: clip.offset + (intoTicks / PPQ) * (60 / bpm), remaining: clip.length - intoTicks });
    }
  }
  return out;
}
