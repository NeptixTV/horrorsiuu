// Music theory helpers: note names, scales and frequency conversion.

export const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

export function noteName(midi: number): string {
  const octave = Math.floor(midi / 12) - 1;
  return `${NOTE_NAMES[((midi % 12) + 12) % 12]}${octave}`;
}

export function isBlackKey(midi: number): boolean {
  const n = ((midi % 12) + 12) % 12;
  return n === 1 || n === 3 || n === 6 || n === 8 || n === 10;
}

export function midiToFreq(midi: number): number {
  return 440 * Math.pow(2, (midi - 69) / 12);
}

export function freqToMidi(freq: number): number {
  return 69 + 12 * Math.log2(freq / 440);
}

export const SCALES: Record<string, number[]> = {
  Chromatic: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  Major: [0, 2, 4, 5, 7, 9, 11],
  Minor: [0, 2, 3, 5, 7, 8, 10],
  'Harmonic Minor': [0, 2, 3, 5, 7, 8, 11],
  'Melodic Minor': [0, 2, 3, 5, 7, 9, 11],
  Dorian: [0, 2, 3, 5, 7, 9, 10],
  Phrygian: [0, 1, 3, 5, 7, 8, 10],
  Lydian: [0, 2, 4, 6, 7, 9, 11],
  Mixolydian: [0, 2, 4, 5, 7, 9, 10],
  'Pentatonic Major': [0, 2, 4, 7, 9],
  'Pentatonic Minor': [0, 3, 5, 7, 10],
  Blues: [0, 3, 5, 6, 7, 10],
};

export const SCALE_NAMES = Object.keys(SCALES);

/** 12 element mask, true where the pitch class is in the scale. */
export function scaleMask(key: number, scale: string): boolean[] {
  const intervals = SCALES[scale] ?? SCALES.Chromatic;
  const mask = new Array<boolean>(12).fill(false);
  for (const i of intervals) mask[(key + i) % 12] = true;
  return mask;
}

/** Nearest MIDI note (fractional input) that lies in the scale. */
export function nearestScaleNote(midi: number, mask: boolean[]): number {
  const base = Math.round(midi);
  for (let d = 0; d <= 6; d++) {
    const candidates = d === 0 ? [base] : [base - d, base + d];
    let best = -1;
    let bestDist = Infinity;
    for (const c of candidates) {
      if (mask[((c % 12) + 12) % 12]) {
        const dist = Math.abs(c - midi);
        if (dist < bestDist) {
          bestDist = dist;
          best = c;
        }
      }
    }
    if (best >= 0) return best;
  }
  return base;
}

/** Computer keyboard → semitone offset (two rows, FL/Ableton style). */
export const TYPING_KEYS: Record<string, number> = {
  z: 0, s: 1, x: 2, d: 3, c: 4, v: 5, g: 6, b: 7, h: 8, n: 9, j: 10, m: 11,
  ',': 12, l: 13, '.': 14,
  q: 12, '2': 13, w: 14, '3': 15, e: 16, r: 17, '5': 18, t: 19, '6': 20, y: 21, '7': 22, u: 23,
  i: 24, '9': 25, o: 26, '0': 27, p: 28,
};
