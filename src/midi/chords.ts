// Chord theory for the DreamChord: the seven diatonic chords of a key (like
// the Nashville number system), joystick variations, chord names and
// voicings with smooth voice leading.

export type ScaleMode = 'major' | 'minor';
export type ChordMod = 'none' | 'seventh' | 'ninth' | 'add9' | 'six' | 'sus4' | 'sus2' | 'flip' | 'dom7';
/** Joystick directions (n = up). */
export type JoyDir = 'c' | 'n' | 'ne' | 'e' | 'se' | 's' | 'sw' | 'w' | 'nw';
export type Voicing = 'close' | 'open' | 'spread';

/** What each joystick direction does to the chord that is held. */
export const JOY_MODS: Record<JoyDir, ChordMod> = {
  c: 'none',
  n: 'seventh',
  ne: 'ninth',
  e: 'add9',
  se: 'six',
  s: 'sus4',
  sw: 'sus2',
  w: 'flip',
  nw: 'dom7',
};

export const MOD_LABELS: Record<ChordMod, string> = {
  none: 'Triad', seventh: '7th', ninth: '9th', add9: 'add9', six: '6th', sus4: 'sus4', sus2: 'sus2', flip: 'Maj↔Min', dom7: 'Dom 7',
};

const SCALES: Record<ScaleMode, number[]> = {
  major: [0, 2, 4, 5, 7, 9, 11],
  minor: [0, 2, 3, 5, 7, 8, 10],
};

const SHARPS = ['C', 'C♯', 'D', 'D♯', 'E', 'F', 'F♯', 'G', 'G♯', 'A', 'A♯', 'B'];
const FLATS = ['C', 'D♭', 'D', 'E♭', 'E', 'F', 'G♭', 'G', 'A♭', 'A', 'B♭', 'B'];
const FLAT_MAJOR_KEYS = new Set([1, 3, 5, 6, 8, 10]);

export function usesFlats(key: number, mode: ScaleMode) {
  const major = mode === 'major' ? key : (key + 3) % 12;
  return FLAT_MAJOR_KEYS.has(((major % 12) + 12) % 12);
}

export function noteName(pc: number, flats = false) {
  return (flats ? FLATS : SHARPS)[((pc % 12) + 12) % 12];
}

export function keyName(key: number, mode: ScaleMode) {
  return `${noteName(key, usesFlats(key, mode))} ${mode === 'major' ? 'Major' : 'Minor'}`;
}

const SUFFIX: [number[], string][] = [
  [[0, 4, 7], ''], [[0, 3, 7], 'm'], [[0, 3, 6], '°'], [[0, 4, 8], '+'], [[0, 5, 7], 'sus4'], [[0, 2, 7], 'sus2'],
  [[0, 4, 7, 11], 'maj7'], [[0, 3, 7, 10], 'm7'], [[0, 4, 7, 10], '7'], [[0, 3, 6, 10], 'm7♭5'], [[0, 3, 6, 9], '°7'],
  [[0, 3, 7, 11], 'm(maj7)'], [[0, 4, 8, 11], 'maj7♯5'], [[0, 4, 8, 10], '7♯5'], [[0, 5, 7, 10], '7sus4'],
  [[0, 4, 7, 14], 'add9'], [[0, 3, 7, 14], 'm(add9)'], [[0, 3, 6, 14], '°(add9)'], [[0, 4, 8, 14], '+(add9)'],
  [[0, 4, 7, 9], '6'], [[0, 3, 7, 9], 'm6'], [[0, 3, 6, 9], '°7'], [[0, 4, 8, 9], '+6'],
  [[0, 4, 7, 11, 14], 'maj9'], [[0, 3, 7, 10, 14], 'm9'], [[0, 4, 7, 10, 14], '9'], [[0, 3, 6, 10, 14], 'm9♭5'],
  [[0, 3, 7, 11, 14], 'm(maj9)'], [[0, 4, 8, 11, 14], 'maj9♯5'], [[0, 5, 7, 14], 'sus4(add9)'], [[0, 2, 7, 14], 'sus2'],
  [[0, 5, 6, 14], 'sus4♭5'], [[0, 2, 6], 'sus2♭5'], [[0, 5, 6], 'sus4♭5'], [[0, 5, 8], 'sus4♯5'], [[0, 2, 8], 'sus2♯5'],
];

export function chordSuffix(intervals: number[]): string {
  const key = [...new Set(intervals)].sort((a, b) => a - b).join(',');
  for (const [iv, s] of SUFFIX) if (iv.join(',') === key) return s;
  return `(${intervals.join(' ')})`;
}

export interface Chord {
  /** pitch class of the root */
  root: number;
  /** semitones above the root, ascending (may exceed 12 for 9ths) */
  intervals: number[];
  name: string;
  /** Nashville number 1..7 */
  degree: number;
  /** roman numeral of the diatonic chord on this degree */
  roman: string;
}

const ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII'];

/** Third and fifth (semitones) of the diatonic triad on a degree. */
function triad(mode: ScaleMode, d: number): [number, number] {
  const scale = SCALES[mode];
  const at = (i: number) => scale[i % 7] + 12 * Math.floor(i / 7);
  return [at(d + 2) - at(d), at(d + 4) - at(d)];
}

function romanOf(mode: ScaleMode, d: number): string {
  const [third, fifth] = triad(mode, d);
  const r = ROMAN[d];
  if (third === 3 && fifth === 6) return `${r.toLowerCase()}°`;
  return third === 3 ? r.toLowerCase() : r;
}

/** Roman numerals of the seven diatonic triads of a mode (e.g. I ii iii IV V vi vii°). */
export function romanNumerals(mode: ScaleMode): string[] {
  return [0, 1, 2, 3, 4, 5, 6].map((d) => romanOf(mode, d));
}

/** The chord on scale degree `degree` (0..6) of key/mode, modified by a joystick direction. */
export function diatonicChord(key: number, mode: ScaleMode, degree: number, mod: ChordMod = 'none'): Chord {
  const scale = SCALES[mode];
  const d = ((degree % 7) + 7) % 7;
  const at = (i: number) => scale[i % 7] + 12 * Math.floor(i / 7);
  const base = at(d);
  const tone = (step: number) => at(d + step) - base;
  const third = tone(2);
  const fifth = tone(4);
  const seventh = tone(6);
  let iv: number[];
  switch (mod) {
    case 'seventh': iv = [0, third, fifth, seventh]; break;
    case 'ninth': iv = [0, third, fifth, seventh, 14]; break; // always a major 9th (♭9 sounds harsh in a pad)
    case 'add9': iv = [0, third, fifth, 14]; break;
    case 'six': iv = [0, third, fifth, 9]; break;
    case 'sus4': iv = [0, 5, fifth]; break;
    case 'sus2': iv = [0, 2, fifth]; break;
    case 'flip': iv = [0, third === 4 ? 3 : 4, 7]; break;
    case 'dom7': iv = [0, 4, 7, 10]; break;
    default: iv = [0, third, fifth];
  }
  iv = [...new Set(iv)].sort((a, b) => a - b);
  const root = (((key + base) % 12) + 12) % 12;
  const flats = usesFlats(key, mode);
  return { root, intervals: iv, name: noteName(root, flats) + chordSuffix(iv), degree: d + 1, roman: romanOf(mode, d) };
}

/** Maps a joystick vector (-1..1, y up = positive) to one of 9 directions. */
export function joyDirection(x: number, y: number, deadzone = 0.38): JoyDir {
  const r = Math.hypot(x, y);
  if (r < deadzone) return 'c';
  const a = (Math.atan2(y, x) * 180) / Math.PI; // 0 = right, 90 = up
  const dirs: JoyDir[] = ['e', 'ne', 'n', 'nw', 'w', 'sw', 's', 'se'];
  const i = Math.round(((a + 360) % 360) / 45) % 8;
  return dirs[i];
}

const sum = (a: number[]) => a.reduce((s, v) => s + v, 0);

/** Best inversion of `pcs` (pitch classes) around `center`, moving smoothly from `prev`. */
function voiceLead(tones: number[], low: number, high: number, prev: number[] | null, center: number): number[] {
  // candidate voicings: every rotation of the tones, at every octave that fits
  const cands: number[][] = [];
  const n = tones.length;
  for (let rot = 0; rot < n; rot++) {
    const r = tones.map((t, i) => (i < rot ? t + 12 : t)).sort((a, b) => a - b);
    for (let oct = -3; oct <= 3; oct++) {
      const v = r.map((t) => t + oct * 12);
      if (v[0] >= low && v[v.length - 1] <= high) cands.push(v);
    }
  }
  if (cands.length === 0) {
    // squeeze into range
    return tones.map((t) => {
      let x = t;
      while (x < low) x += 12;
      while (x > high) x -= 12;
      return x;
    }).sort((a, b) => a - b);
  }
  const target = prev && prev.length ? prev : null;
  let best = cands[0];
  let bestScore = Infinity;
  for (const c of cands) {
    let score: number;
    if (target) {
      // total movement: each voice to its nearest previous voice
      score = sum(c.map((x) => Math.min(...target.map((y) => Math.abs(x - y)))));
      score += Math.abs(sum(c) / c.length - sum(target) / target.length) * 0.5;
    } else {
      score = Math.abs(sum(c) / c.length - center);
    }
    // prefer compact voicings
    score += (c[c.length - 1] - c[0]) * 0.05;
    if (score < bestScore) {
      bestScore = score;
      best = c;
    }
  }
  return best;
}

/**
 * MIDI notes for a chord. `close` = smooth voice-led chord in the middle
 * register, `open` = bass note + spread upper voices, `spread` = wide pad.
 */
export function voiceChord(chord: Chord, octave = 0, voicing: Voicing = 'open', prev: number[] | null = null): number[] {
  const shift = Math.max(-2, Math.min(2, Math.round(octave))) * 12;
  const rootPc = chord.root;
  const upper = chord.intervals.map((i) => rootPc + i + 48);
  const prevUpper = prev ? prev.filter((n) => n >= 50 + shift) : null;
  let notes: number[];
  if (voicing === 'close') {
    notes = voiceLead(upper, 53 + shift, 79 + shift, prevUpper, 64 + shift);
  } else if (voicing === 'open') {
    const bass = 36 + rootPc + shift;
    const top = voiceLead(upper.filter((_, i) => i > 0 || chord.intervals.length <= 3), 55 + shift, 81 + shift, prevUpper, 66 + shift);
    notes = [bass, ...top];
  } else {
    const bass = 36 + rootPc + shift;
    const fifth = chord.intervals.find((i) => i === 7 || i === 6 || i === 8) ?? 7;
    const rest = chord.intervals.filter((i) => i !== 0 && i !== fifth);
    const wide = rest.map((i, k) => bass + 12 + i + (k % 2 === 1 ? 12 : 0));
    notes = [bass, bass + fifth, ...wide, bass + 24];
  }
  return [...new Set(notes.map((n) => Math.max(0, Math.min(127, n))))].sort((a, b) => a - b);
}

export type ArpPattern = 'up' | 'down' | 'updown' | 'random' | 'chord';

/** Order in which the arpeggiator walks through a chord (over `octaves` octaves). */
export function arpSequence(notes: number[], mode: ArpPattern, octaves = 1): number[] {
  const sorted = [...notes].sort((a, b) => a - b);
  const span: number[] = [];
  for (let o = 0; o < Math.max(1, Math.min(3, octaves)); o++) for (const n of sorted) span.push(Math.min(127, n + o * 12));
  if (mode === 'down') return [...span].reverse();
  if (mode === 'updown') return span.length > 2 ? [...span, ...span.slice(1, -1).reverse()] : span;
  return span; // 'random' and 'chord' are handled by the player
}

/** Arp rate in 16th-step ticks for a PPQ. */
export function arpStepTicks(rate: string, ppq: number): number {
  switch (rate) {
    case '1/4': return ppq;
    case '1/8': return ppq / 2;
    case '1/8t': return ppq / 3;
    case '1/16': return ppq / 4;
    case '1/16t': return ppq / 6;
    case '1/32': return ppq / 8;
    default: return ppq / 2;
  }
}
