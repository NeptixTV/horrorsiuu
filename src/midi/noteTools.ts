// Pure note transformations used by the piano roll tools menu. Every
// function returns new notes and never mutates its input.
import type { Note } from '../state/types';
import { uid } from '../state/utils';

export type ChordStamp = 'none' | 'triad' | 'seventh' | 'sus2' | 'sus4' | 'power' | 'octave';

export const CHORD_STAMPS: { value: ChordStamp; label: string }[] = [
  { value: 'none', label: 'Single notes' },
  { value: 'triad', label: 'Triads' },
  { value: 'seventh', label: '7th chords' },
  { value: 'sus2', label: 'sus2' },
  { value: 'sus4', label: 'sus4' },
  { value: 'power', label: 'Power (1-5-8)' },
  { value: 'octave', label: 'Octaves' },
];

const clampPitch = (p: number) => Math.max(0, Math.min(127, Math.round(p)));

// `mask` is indexed by absolute pitch class (0 = C … 11 = B), as returned by
// scaleMask(key, scale).
const inMask = (mask: boolean[], p: number) => mask[((p % 12) + 12) % 12];

/** Nearest pitch that is in the scale (ties go up). */
export function snapToScale(pitch: number, mask: boolean[]): number {
  if (!mask.some(Boolean) || inMask(mask, pitch)) return pitch;
  for (let d = 1; d < 12; d++) {
    if (inMask(mask, pitch + d)) return clampPitch(pitch + d);
    if (inMask(mask, pitch - d)) return clampPitch(pitch - d);
  }
  return pitch;
}

/** Pitches of a diatonic chord on `root` (snapped into the scale) by stacking scale thirds. */
export function chordAt(root: number, stamp: ChordStamp, mask: boolean[]): number[] {
  if (stamp === 'none') return [root];
  if (stamp === 'power') return [root, root + 7, root + 12].map(clampPitch);
  if (stamp === 'octave') return [root, root + 12].map(clampPitch);
  const hasScale = mask.some(Boolean) && mask.filter(Boolean).length >= 5;
  const r = hasScale ? snapToScale(root, mask) : root;
  const scale: number[] = [];
  if (hasScale) for (let p = r; p < r + 36 && scale.length < 9; p++) if (inMask(mask, p)) scale.push(p);
  // without a usable scale: major-scale steps above the root
  const step = (i: number) => scale[i] ?? r + [0, 2, 4, 5, 7, 9, 11, 12, 14][i];
  if (stamp === 'sus2') return [r, step(1), step(4)].map(clampPitch);
  if (stamp === 'sus4') return [r, step(3), step(4)].map(clampPitch);
  const tones = [step(0), step(2), step(4)];
  if (stamp === 'seventh') tones.push(step(6));
  return tones.map(clampPitch);
}

export function transpose(notes: Note[], semis: number): Note[] {
  return notes.map((n) => ({ ...n, pitch: clampPitch(n.pitch + semis) }));
}

export function shift(notes: Note[], ticks: number): Note[] {
  return notes.map((n) => ({ ...n, start: Math.max(0, n.start + ticks) }));
}

export function setLength(notes: Note[], length: number): Note[] {
  return notes.map((n) => ({ ...n, length: Math.max(1, length) }));
}

/** Small random timing and velocity changes (deterministic with a seed for tests). */
export function humanize(notes: Note[], timingTicks: number, velocityAmount: number, seed = Date.now()): Note[] {
  let s = seed >>> 0 || 1;
  const rnd = () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 4294967296 * 2 - 1;
  };
  return notes.map((n) => ({
    ...n,
    start: Math.max(0, Math.round(n.start + rnd() * timingTicks)),
    velocity: Math.max(0.05, Math.min(1, n.velocity + rnd() * velocityAmount)),
  }));
}

/** Groups notes that start together (within `tolerance` ticks). */
function chordsOf(notes: Note[], tolerance = 2): Note[][] {
  const sorted = [...notes].sort((a, b) => a.start - b.start || a.pitch - b.pitch);
  const groups: Note[][] = [];
  for (const n of sorted) {
    const g = groups[groups.length - 1];
    if (g && Math.abs(g[0].start - n.start) <= tolerance) g.push(n);
    else groups.push([n]);
  }
  return groups;
}

/** Every note lasts until the next note (or chord) starts. */
export function legato(notes: Note[]): Note[] {
  const groups = chordsOf(notes);
  const out: Note[] = [];
  groups.forEach((g, i) => {
    const next = groups[i + 1];
    for (const n of g) out.push(next ? { ...n, length: Math.max(1, next[0].start - n.start) } : n);
  });
  return out;
}

/** Spreads the notes of each chord like a guitar strum. */
export function strum(notes: Note[], ticksPerNote: number, direction: 'up' | 'down' = 'up'): Note[] {
  const out: Note[] = [];
  for (const g of chordsOf(notes)) {
    const order = [...g].sort((a, b) => (direction === 'up' ? a.pitch - b.pitch : b.pitch - a.pitch));
    order.forEach((n, i) => {
      const delay = i * ticksPerNote;
      out.push({ ...n, start: n.start + delay, length: Math.max(1, n.length - delay) });
    });
  }
  return out;
}

/** Turns block chords into arpeggios (each chord keeps its length). */
export function arpeggiate(notes: Note[], stepTicks: number, pattern: 'up' | 'down' | 'updown' = 'up'): Note[] {
  const out: Note[] = [];
  for (const g of chordsOf(notes)) {
    if (g.length < 2) {
      out.push(...g);
      continue;
    }
    const asc = [...g].sort((a, b) => a.pitch - b.pitch);
    const seq = pattern === 'down' ? [...asc].reverse() : pattern === 'updown' ? [...asc, ...asc.slice(1, -1).reverse()] : asc;
    const start = g[0].start;
    const len = Math.max(...g.map((n) => n.length));
    for (let t = 0, i = 0; t < len; t += stepTicks, i++) {
      const src = seq[i % seq.length];
      out.push({ ...src, id: uid('n'), start: start + t, length: Math.max(1, Math.min(stepTicks, len - t)) });
    }
  }
  return out;
}

/** Plays the selection backwards in time (inside its own span). */
export function reverse(notes: Note[]): Note[] {
  if (!notes.length) return notes;
  const a = Math.min(...notes.map((n) => n.start));
  const b = Math.max(...notes.map((n) => n.start + n.length));
  // a note that ended at the end now starts at the beginning
  return notes.map((n) => ({ ...n, start: a + (b - (n.start + n.length)) }));
}

/** Velocities go smoothly from `from` to `to` across the selection (crescendo / fade). */
export function velocityRamp(notes: Note[], from: number, to: number): Note[] {
  if (notes.length < 2) return notes.map((n) => ({ ...n, velocity: from }));
  const a = Math.min(...notes.map((n) => n.start));
  const b = Math.max(...notes.map((n) => n.start));
  return notes.map((n) => ({ ...n, velocity: Math.max(0.02, Math.min(1, from + (to - from) * (b > a ? (n.start - a) / (b - a) : 0))) }));
}

/** Mirrors the pitches around the middle of the selection. */
export function invert(notes: Note[]): Note[] {
  if (!notes.length) return notes;
  const lo = Math.min(...notes.map((n) => n.pitch));
  const hi = Math.max(...notes.map((n) => n.pitch));
  return notes.map((n) => ({ ...n, pitch: clampPitch(hi + lo - n.pitch) }));
}

/** Copies placed right after the selection (new ids). */
export function duplicateAfter(notes: Note[], gridTicks: number): Note[] {
  if (!notes.length) return [];
  const a = Math.min(...notes.map((n) => n.start));
  const b = Math.max(...notes.map((n) => n.start + n.length));
  const span = Math.max(gridTicks, Math.ceil((b - a) / gridTicks) * gridTicks);
  return notes.map((n) => ({ ...n, id: uid('n'), start: n.start + span }));
}
