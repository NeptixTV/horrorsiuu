// Looks at an imported sound so it lands in the right place on its own:
// silence before / after it is trimmed, the kind of sound is guessed from
// the file name (sample packs name their files well) or from the audio, and
// melodic sounds get their root note. Pure functions, unit tested.

export type SoundCategory =
  | 'Kicks' | '808s' | 'Snares' | 'Claps' | 'Hi-Hats' | 'Open Hats' | 'Percussion' | 'Cymbals'
  | 'FX' | 'Vocals' | 'Loops' | 'Melodic' | 'Other';

export const SOUND_CATEGORIES: SoundCategory[] = [
  'Kicks', '808s', 'Snares', 'Claps', 'Hi-Hats', 'Open Hats', 'Percussion', 'Cymbals', 'FX', 'Vocals', 'Loops', 'Melodic', 'Other',
];

/** Categories that are played like drums (one-shots on a pad). */
export const DRUM_CATEGORIES = new Set<SoundCategory>(['Kicks', 'Snares', 'Claps', 'Hi-Hats', 'Open Hats', 'Percussion', 'Cymbals']);

export interface SoundInfo {
  category: SoundCategory;
  /** MIDI root note of melodic sounds */
  root?: number;
  /** tempo of loops, from the file name */
  bpm?: number;
}

const RULES: [RegExp, SoundCategory][] = [
  [/open.?(hi.?)?hat|\bohh?\b|\bo.?hat/i, 'Open Hats'],
  [/h(i|y)?.?hats?|\bhh\b|\bchh?\b|hihat/i, 'Hi-Hats'],
  [/kick|\bkik\b|\bbd\b|bass.?drum/i, 'Kicks'],
  [/snare|\bsd\b|\bsnr\b|rimshot/i, 'Snares'],
  [/clap|\bclp\b|snap/i, 'Claps'],
  [/crash|ride|cymbal|china|splash/i, 'Cymbals'],
  [/perc|\brim\b|\btom\b|conga|bongo|shaker|cowbell|tamb|clave|wood|block|triangle|guiro/i, 'Percussion'],
  // "808" alone is the bass (an "808 kick" was caught above)
  [/808|\bsub\b|reese/i, '808s'],
  [/vox|vocal|voice|chant|adlib|\bhey\b|choir|acapella/i, 'Vocals'],
  [/\bloop\b|_loop|loop_|break(beat)?s?\b|\bbpm\b|\d{2,3}\s?bpm/i, 'Loops'],
  [/\bfx\b|riser|impact|sweep|whoosh|downlifter|uplifter|transition|noise|reverse|\brev\b|zap|laser/i, 'FX'],
  [/piano|keys|synth|pad|lead|pluck|melody|guitar|bell|chord|string|flute|organ|brass|horn|bass|arp|stab/i, 'Melodic'],
];

export function categoryFromName(name: string): SoundCategory | null {
  const n = name.replace(/[_\-.]+/g, ' ');
  for (const [re, cat] of RULES) if (re.test(n) || re.test(name)) return cat;
  return null;
}

/** "Dark Loop 140 BPM", "loop_92bpm", "trap_150_Cmin" → 140 / 92 / 150 */
export function bpmFromName(name: string): number | undefined {
  const m = /(\d{2,3})\s?-?bpm/i.exec(name) ?? /(?:^|[_\s-])(\d{2,3})(?:[_\s-]|$)/.exec(name);
  if (!m) return undefined;
  const v = Number(m[1]);
  return v >= 60 && v <= 200 ? v : undefined;
}

const NOTE_PC: Record<string, number> = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 };

/** "Piano C3", "808_F#1", "bell_Gb4" → MIDI note (C4 = 60); only an explicit note with octave counts. */
export function noteFromName(name: string): number | undefined {
  const m = /(?:^|[\s_\-(])([A-Ga-g])(#|b|♯|♭)?(-?\d)(?=$|[\s_\-).])/.exec(name);
  if (!m) return undefined;
  let pc = NOTE_PC[m[1].toLowerCase()];
  if (m[2] === '#' || m[2] === '♯') pc += 1;
  if (m[2] === 'b' || m[2] === '♭') pc -= 1;
  const midi = (Number(m[3]) + 1) * 12 + pc;
  return midi >= 12 && midi <= 120 ? midi : undefined;
}

/** Sample range without the silence before and after the sound (with a few ms of margin). */
export function trimRange(chans: Float32Array[], sampleRate: number, thresholdDb = -54): { start: number; end: number } {
  const n = chans[0]?.length ?? 0;
  let peak = 0;
  for (const c of chans) for (let i = 0; i < n; i++) { const a = Math.abs(c[i]); if (a > peak) peak = a; }
  if (peak === 0) return { start: 0, end: n };
  const t = peak * Math.pow(10, thresholdDb / 20);
  let start = 0;
  outer: for (; start < n; start++) for (const c of chans) if (Math.abs(c[start]) > t) break outer;
  let end = n - 1;
  outer2: for (; end > start; end--) for (const c of chans) if (Math.abs(c[end]) > t) break outer2;
  return { start: Math.max(0, start - Math.round(sampleRate * 0.002)), end: Math.min(n, end + Math.round(sampleRate * 0.02)) };
}

/**
 * Rough spectral balance of the first half second: the share of energy below
 * 150 Hz, and "brightness" from the first difference (white noise ≈ 0.5,
 * a hi-hat more, a kick almost 0).
 */
function balance(x: Float32Array, sr: number) {
  const aLow = 1 - Math.exp((-2 * Math.PI * 150) / sr);
  let low = 0, lowE = 0, all = 0, diff = 0, prev = 0;
  const n = Math.min(x.length, Math.round(sr * 0.5));
  for (let i = 0; i < n; i++) {
    const v = x[i];
    low += aLow * (v - low);
    lowE += low * low;
    all += v * v;
    diff += (v - prev) * (v - prev);
    prev = v;
  }
  return { low: lowE / (all + 1e-12), bright: diff / (4 * all + 1e-12) };
}

/** Root note of a (roughly) pitched sound – YIN on a steady part. null if it is not pitched. */
export function detectPitch(x: Float32Array, sr: number): number | null {
  const W = Math.min(4096, Math.floor(x.length / 3));
  if (W < 512) return null;
  // skip the attack: start a little into the sound
  const off = Math.min(x.length - 2 * W, Math.round(sr * 0.06));
  if (off < 0) return null;
  const maxLag = Math.min(W - 1, Math.floor(sr / 30));
  const minLag = Math.max(2, Math.floor(sr / 2000));
  const d = new Float32Array(maxLag + 1);
  for (let lag = minLag; lag <= maxLag; lag++) {
    let s = 0;
    for (let i = 0; i < W; i++) { const v = x[off + i] - x[off + i + lag]; s += v * v; }
    d[lag] = s;
  }
  // cumulative mean normalised difference
  let run = 0;
  let best = -1;
  for (let lag = minLag; lag <= maxLag; lag++) {
    run += d[lag];
    const cm = run > 0 ? (d[lag] * (lag - minLag + 1)) / run : 1;
    if (cm < 0.15) {
      // walk to the local minimum
      let l = lag;
      while (l + 1 <= maxLag && d[l + 1] < d[l]) l++;
      best = l;
      break;
    }
  }
  if (best < 0) return null;
  const f = sr / best;
  return Math.round(69 + 12 * Math.log2(f / 440));
}

/** Guesses what an imported sound is. */
export function analyzeSound(name: string, chans: Float32Array[], sr: number): SoundInfo {
  const mono = chans.length > 1 ? chans[0].map((v, i) => 0.5 * (v + chans[1][i])) : chans[0];
  const dur = mono.length / sr;
  let category = categoryFromName(name);
  const bpm = bpmFromName(name);
  if (!category) {
    if (bpm && dur > 1.5) category = 'Loops';
    else if (dur > 4) category = 'Loops';
    else {
      const b = balance(mono, sr);
      if (dur < 1.6 && b.low > 0.6) category = dur > 0.9 ? '808s' : 'Kicks';
      else if (dur < 0.5 && b.bright > 0.4) category = 'Hi-Hats';
      else if (dur < 2.5 && b.bright > 0.4) category = 'Open Hats';
      else if (dur < 0.8 && b.bright > 0.08) category = 'Snares';
      else category = 'Other';
    }
  }
  const info: SoundInfo = { category };
  if (bpm && (category === 'Loops' || dur > 2)) info.bpm = bpm;
  if (!DRUM_CATEGORIES.has(category) && category !== 'Loops' && category !== 'FX') {
    const root = noteFromName(name) ?? detectPitch(mono, sr) ?? undefined;
    if (root !== undefined && root >= 12 && root <= 108) info.root = root;
  }
  return info;
}
