// "Idea dice": a small composer that rolls a musical starting point in the
// project's key – chord progression, bass line, melody and a drum groove that
// fit together. Every part has its own seed, so one part can be re-rolled
// while the others stay (lock). Pure functions: same options → same idea.
import type { DrumType, Note } from '../state/types';
import { TICKS_PER_STEP } from '../state/types';
import { SCALES } from './music';
import { diatonicChord, voiceChord, type Chord, type ChordMod, type ScaleMode } from './chords';

export type IdeaMood = 'dreamy' | 'happy' | 'sad' | 'dark' | 'epic' | 'chill';
export type IdeaStyle = 'lofi' | 'pop' | 'house' | 'hiphop' | 'trap' | 'ambient';
export type IdeaPart = 'chords' | 'bass' | 'melody' | 'drums';
export const IDEA_PARTS: IdeaPart[] = ['chords', 'bass', 'melody', 'drums'];

export const MOODS: { id: IdeaMood; label: string }[] = [
  { id: 'dreamy', label: 'Dreamy' }, { id: 'happy', label: 'Happy' }, { id: 'sad', label: 'Sad' },
  { id: 'dark', label: 'Dark' }, { id: 'epic', label: 'Epic' }, { id: 'chill', label: 'Chill' },
];
export const STYLES: { id: IdeaStyle; label: string }[] = [
  { id: 'lofi', label: 'Lo-fi' }, { id: 'pop', label: 'Pop' }, { id: 'house', label: 'House' },
  { id: 'hiphop', label: 'Hip-hop' }, { id: 'trap', label: 'Trap' }, { id: 'ambient', label: 'Ambient' },
];

export interface IdeaOptions {
  key: number;
  mode: ScaleMode;
  mood: IdeaMood;
  style: IdeaStyle;
  bars: 2 | 4 | 8;
  seeds: Record<IdeaPart, number>;
}

export interface IdeaNote {
  pitch: number;
  start: number; // ticks
  length: number;
  velocity: number;
}

export interface Idea {
  progression: string;
  lengthSteps: number;
  chords: IdeaNote[];
  bass: IdeaNote[];
  melody: IdeaNote[];
  drums: Partial<Record<DrumType, number[]>>;
}

/** Small, fast, seedable PRNG (mulberry32). */
export function rng(seed: number) {
  let a = seed >>> 0;
  const next = () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
  return {
    next,
    int: (n: number) => Math.floor(next() * n),
    pick: <T,>(list: readonly T[]): T => list[Math.floor(next() * list.length)],
    chance: (p: number) => next() < p,
  };
}

export const newSeed = () => (Math.random() * 0xffffffff) >>> 0;

// progressions as 0-based scale degrees, per mode and mood
const PROGRESSIONS: Record<ScaleMode, Record<IdeaMood, number[][]>> = {
  major: {
    happy: [[0, 4, 5, 3], [0, 3, 4, 3], [0, 5, 3, 4], [3, 4, 0, 0], [0, 3, 0, 4]],
    dreamy: [[0, 3, 5, 4], [3, 0, 4, 5], [0, 2, 3, 3], [0, 5, 3, 3], [3, 4, 2, 5]],
    sad: [[5, 3, 0, 4], [5, 4, 3, 3], [0, 2, 5, 3]],
    dark: [[5, 4, 3, 4], [5, 3, 4, 2], [2, 5, 3, 4]],
    epic: [[5, 3, 0, 4], [0, 4, 5, 3], [3, 4, 5, 5]],
    chill: [[1, 4, 0, 0], [3, 2, 1, 0], [0, 5, 1, 4], [3, 3, 0, 5]],
  },
  minor: {
    happy: [[2, 6, 0, 5], [5, 6, 2, 2], [2, 5, 6, 0]],
    dreamy: [[0, 5, 2, 6], [5, 6, 0, 0], [0, 3, 5, 6], [5, 2, 6, 3]],
    sad: [[0, 5, 2, 6], [0, 3, 5, 4], [0, 6, 5, 6], [0, 5, 3, 4]],
    dark: [[0, 5, 6, 4], [0, 1, 4, 0], [0, 0, 5, 4], [0, 3, 1, 4]],
    epic: [[0, 5, 2, 6], [0, 6, 5, 6], [5, 6, 0, 0], [0, 3, 6, 2]],
    chill: [[0, 3, 6, 2], [3, 6, 2, 5], [0, 6, 5, 3]],
  },
};

function chordMod(mood: IdeaMood, mode: ScaleMode, degree: number, r: ReturnType<typeof rng>): ChordMod {
  // the minor v is usually played as major V (leading tone) in dark / epic moods
  if (mode === 'minor' && degree === 4 && (mood === 'dark' || mood === 'epic') && r.chance(0.7)) return 'flip';
  switch (mood) {
    case 'dreamy': return r.pick(['seventh', 'add9', 'add9', 'none'] as ChordMod[]);
    case 'chill': return r.pick(['seventh', 'ninth', 'seventh'] as ChordMod[]);
    case 'sad': return r.pick(['none', 'none', 'seventh', 'add9'] as ChordMod[]);
    case 'happy': return r.pick(['none', 'none', 'add9', 'sus2'] as ChordMod[]);
    case 'epic': return r.pick(['none', 'none', 'sus2'] as ChordMod[]);
    default: return 'none';
  }
}

const STEP = TICKS_PER_STEP;

function progressionFor(o: IdeaOptions) {
  const r = rng(o.seeds.chords);
  const base = r.pick(PROGRESSIONS[o.mode][o.mood]);
  const perBar = o.bars === 2 ? 2 : 1;
  const count = o.bars * perBar;
  const degrees: number[] = [];
  for (let i = 0; i < count; i++) degrees.push(base[i % base.length]);
  // 8 bars: the second time round, the last chord may resolve differently
  if (count === 8 && r.chance(0.5)) degrees[7] = r.pick(PROGRESSIONS[o.mode][o.mood])[3];
  const chords: Chord[] = degrees.map((d) => diatonicChord(o.key, o.mode, d, chordMod(o.mood, o.mode, d, r)));
  const span = (16 * STEP) / perBar;
  return { chords, span, r };
}

function chordPart(o: IdeaOptions, chords: Chord[], span: number, r: ReturnType<typeof rng>): IdeaNote[] {
  const out: IdeaNote[] = [];
  let prev: number[] | null = null;
  chords.forEach((c, i) => {
    const voiced = voiceChord(c, 0, 'close', prev);
    prev = voiced;
    const at = i * span;
    const hit = (start: number, len: number, vel: number, strum = 0) =>
      voiced.forEach((p, k) => out.push({ pitch: p, start: at + start + k * strum, length: Math.max(STEP / 2, len - k * strum), velocity: vel }));
    switch (o.style) {
      case 'house': // off-beat stabs
        for (let s = 2; s < span / STEP; s += 4) hit(s * STEP, STEP * 1.5, 0.62 + r.next() * 0.1);
        break;
      case 'pop': // half notes with a push into the next chord
        hit(0, span / 2, 0.7);
        hit(span / 2, span / 2 - STEP, 0.62);
        break;
      case 'hiphop':
        hit(0, span * 0.6, 0.66, 4);
        if (span >= 16 * STEP) hit(span * 0.625, span * 0.375, 0.55, 4);
        break;
      case 'lofi': // lazy, slightly strummed
        hit(r.int(3) * 4, span - STEP, 0.6 + r.next() * 0.08, 6);
        break;
      default: // ambient, trap: long sustained pads
        hit(0, span, 0.6);
    }
  });
  return out;
}

function bassPart(o: IdeaOptions, chords: Chord[], span: number): IdeaNote[] {
  const r = rng(o.seeds.bass);
  const out: IdeaNote[] = [];
  const steps = span / STEP;
  const low = (pc: number) => 40 + ((pc - 4 + 12) % 12); // E2 … D#3: deep, but still audible on small speakers
  chords.forEach((c, i) => {
    const root = low(c.root);
    const fifth = root + 7;
    const next = chords[(i + 1) % chords.length];
    const nextRoot = low(next.root);
    const at = i * span;
    const n = (s: number, len: number, p: number, vel = 0.8) => out.push({ pitch: p, start: at + s * STEP, length: len * STEP, velocity: vel });
    switch (o.style) {
      case 'house':
        for (let s = 2; s < steps; s += 4) n(s, 1.5, r.chance(0.15) ? root + 12 : root, 0.85);
        break;
      case 'pop':
        for (let s = 0; s < steps; s += 2) n(s, 1.6, s === steps - 2 && r.chance(0.5) ? fifth : r.chance(0.12) ? root + 12 : root, s % 4 === 0 ? 0.85 : 0.7);
        break;
      case 'trap':
        n(0, Math.min(steps, 10), root, 0.95);
        if (steps >= 16) {
          n(10, 2, r.chance(0.5) ? root + 12 : root, 0.8);
          if (r.chance(0.6)) n(13, 3, root, 0.85);
        }
        break;
      case 'ambient':
        n(0, steps, root, 0.7);
        break;
      default: { // lo-fi / hip-hop: root, a syncopated re-hit and a walk into the next chord
        n(0, Math.min(steps, 6), root, 0.85);
        if (steps >= 16) {
          n(7, 3, r.chance(0.4) ? fifth : root, 0.7);
          const approach = nextRoot + (nextRoot > root ? -1 : 1) * (r.chance(0.5) ? 2 : 1);
          if (r.chance(0.7)) n(14, 2, Math.max(28, approach), 0.65);
        } else n(steps / 2, steps / 2 - 1, root, 0.7);
      }
    }
  });
  return out;
}

// rhythmic cells for one bar (16th positions); the melody repeats and varies them
const RHYTHMS: number[][] = [
  [0, 3, 6, 8, 10], [0, 2, 4, 7, 10, 12], [2, 4, 6, 10, 12], [0, 6, 8, 14], [0, 3, 4, 8, 11, 12, 14], [4, 6, 8, 12], [0, 2, 3, 8, 10],
];

function melodyPart(o: IdeaOptions, chords: Chord[], span: number): IdeaNote[] {
  const r = rng(o.seeds.melody);
  const scale = SCALES[o.mode === 'minor' ? 'Minor' : 'Major'];
  // scale tones from G4 to E6
  const tones: number[] = [];
  for (let p = 64; p <= 88; p++) if (scale.includes((((p - o.key) % 12) + 12) % 12)) tones.push(p);
  const nearestIdx = (p: number) => tones.reduce((b, t, i) => (Math.abs(t - p) < Math.abs(tones[b] - p) ? i : b), 0);
  const chordAt = (tick: number) => chords[Math.min(chords.length - 1, Math.floor(tick / span))];
  const isChordTone = (p: number, c: Chord) => c.intervals.some((iv) => (c.root + iv) % 12 === ((p % 12) + 12) % 12);
  const toChordTone = (idx: number, c: Chord) => {
    for (let d = 0; d < 4; d++) {
      if (tones[idx + d] !== undefined && isChordTone(tones[idx + d], c)) return idx + d;
      if (tones[idx - d] !== undefined && isChordTone(tones[idx - d], c)) return idx - d;
    }
    return idx;
  };
  const sparse = o.style === 'ambient' || o.mood === 'dreamy';
  const callRhythm = r.pick(RHYTHMS).filter((s) => !sparse || s % 4 === 0 || r.chance(0.4));
  const answerRhythm = r.pick(RHYTHMS).filter((s) => !sparse || s % 4 === 0 || r.chance(0.4));
  const out: IdeaNote[] = [];
  let idx = nearestIdx(67 + ((o.key + 5) % 12)); // start on the tonic between G4 and F#5
  const bars = o.bars;
  for (let bar = 0; bar < bars; bar++) {
    const call = bar % 2 === 0;
    const rhythm = call ? callRhythm : answerRhythm;
    const last = bar === bars - 1;
    for (let k = 0; k < rhythm.length; k++) {
      const s = rhythm[k];
      const tick = bar * 16 * STEP + s * STEP;
      const c = chordAt(tick);
      // contour: mostly steps, sometimes a leap; strong beats land on chord tones
      const move = r.pick([-1, -1, 1, 1, 0, 2, -2, 1, -1, 3, -3]);
      idx = Math.max(1, Math.min(tones.length - 2, idx + move));
      if (s % 8 === 0 || k === 0) idx = toChordTone(idx, c);
      const nextS = rhythm[k + 1] ?? 16;
      let len = Math.max(1, Math.min(nextS - s, sparse ? 6 : 3));
      if (k === rhythm.length - 1) len = Math.min(16 - s, call ? 2 : 4);
      out.push({ pitch: tones[idx], start: tick, length: len * STEP, velocity: (s % 4 === 0 ? 0.82 : 0.68) + r.next() * 0.08 });
    }
    // the very last note comes home to the tonic
    if (last && out.length) {
      const tonic = out[out.length - 1];
      const home = tones.filter((t) => ((t - o.key) % 12 + 12) % 12 === 0);
      if (home.length) tonic.pitch = home.reduce((b, t) => (Math.abs(t - tonic.pitch) < Math.abs(b - tonic.pitch) ? t : b));
    }
  }
  return out;
}

type Groove = Partial<Record<DrumType, string>>;
const GROOVES: Record<IdeaStyle, Groove[]> = {
  house: [{ kick: 'x...x...x...x...', clap: '....x.......x...', openhat: '..x...x...x...x.', hihat: 'x.x.x.x.x.x.x.x.' }],
  pop: [
    { kick: 'x.....x.x.......', snare: '....x.......x...', hihat: 'x.x.x.x.x.x.x.x.' },
    { kick: 'x.......x.x.....', clap: '....x.......x...', hihat: 'x.xxx.x.x.xxx.x.' },
  ],
  hiphop: [
    { kick: 'x......x.xx.....', snare: '....x.......x...', hihat: 'x.x.x.x.x.x.x.x.' },
    { kick: 'x.x.......x..x..', snare: '....x.......x...', hihat: 'x.x.x.x.x.x.x.xx', perc: '.......o......o.' },
  ],
  lofi: [
    { kick: 'x.......x.x.....', snare: '....x.......x...', hihat: '..x...x...x...x.', perc: '.o.....o........' },
    { kick: 'x..x......x.....', snare: '....x.......x..o', hihat: 'x.x.x.x.x.x.x.x.' },
  ],
  trap: [
    { kick: 'x......x..x.....', clap: '........x.......', hihat: 'x.x.x.x.x.x.xxxx' },
    { kick: 'x.........x..x..', clap: '........x.......', hihat: 'x.x.x.xxx.x.x.x.', openhat: '..............o.' },
  ],
  ambient: [{ kick: 'x...........x...', hihat: '..o...o...o...o.', perc: '........o.......' }],
};

const VEL: Partial<Record<DrumType, number>> = { kick: 0.95, snare: 0.85, clap: 0.8, hihat: 0.55, openhat: 0.5, perc: 0.6 };

function drumPart(o: IdeaOptions): Partial<Record<DrumType, number[]>> {
  const r = rng(o.seeds.drums);
  const g = r.pick(GROOVES[o.style]);
  const out: Partial<Record<DrumType, number[]>> = {};
  const total = o.bars * 16;
  for (const [type, line] of Object.entries(g) as [DrumType, string][]) {
    const base = VEL[type] ?? 0.7;
    const arr = new Array(total).fill(0);
    for (let bar = 0; bar < o.bars; bar++) {
      for (let s = 0; s < 16; s++) {
        const c = line[s];
        let v = c === 'x' ? base : c === 'o' ? base * 0.55 : 0;
        // small variations: hat accents, the odd ghost kick, a pickup at the end of a phrase
        if (type === 'hihat' && v) v *= s % 4 === 0 ? 1 : 0.7 + r.next() * 0.25;
        if (type === 'kick' && !v && s === 14 && bar % 2 === 1 && r.chance(0.35)) v = base * 0.6;
        arr[bar * 16 + s] = Math.round(v * 100) / 100;
      }
    }
    out[type] = arr;
  }
  // fill on the last bar of longer ideas
  if (o.bars >= 4 && o.style !== 'ambient') {
    const sn = (out.snare ??= new Array(total).fill(0));
    const from = total - 4;
    for (let s = from; s < total; s++) sn[s] = Math.round((0.45 + ((s - from) / 4) * 0.45) * 100) / 100;
  }
  return out;
}

export function rollIdea(o: IdeaOptions): Idea {
  const { chords, span, r } = progressionFor(o);
  return {
    progression: chords.slice(0, o.bars === 2 ? 4 : Math.min(chords.length, 4)).map((c) => c.name).join(' – '),
    lengthSteps: o.bars * 16,
    chords: chordPart(o, chords, span, r),
    bass: bassPart(o, chords, span),
    melody: melodyPart(o, chords, span),
    drums: drumPart(o),
  };
}

export function toNotes(list: IdeaNote[], makeId: () => string): Note[] {
  return list.map((n) => ({ id: makeId(), pitch: Math.max(0, Math.min(127, Math.round(n.pitch))), start: Math.round(n.start), length: Math.max(1, Math.round(n.length)), velocity: Math.max(0.05, Math.min(1, n.velocity)) }));
}
