// Beat Starter: builds a complete, mixed beat project from a genre – Pro drum
// kit with a groove, an 808 that follows the kick and the chords, chords and
// a melody from the idea composer, mixer chains, and a short arrangement
// (intro, drop, break, drop) so pressing play in song mode sounds finished.
// Pure function: the same options (and seed) give the same project.
import type { Channel, Clip, EffectInstance, EffectType, MixerTrack, Note, ParamValue, Pattern, PlaylistTrack, Project } from '../state/types';
import { PPQ, TICKS_PER_STEP } from '../state/types';
import { makeChannel, makeEffect, makeMaster, makeMixerTrack, makePattern, makeTrack } from '../state/defaults';
import { defaultPro, PRO_KITS, proSound, type ProKit } from '../audio/pro/catalog';
import { FX_CHAINS } from '../audio/effects/fxPresets';
import { ideaChords, rollIdea, toNotes, type IdeaMood, type IdeaStyle } from '../midi/ideas';
import { uid } from '../state/utils';

export interface BeatGenre {
  id: string;
  name: string;
  emoji: string;
  description: string;
  kit: string;
  mood: IdeaMood;
  style: IdeaStyle;
  minor: boolean;
  chords?: { sound: string; name: string; chain: string; octave?: number; volume?: number };
  melody?: { sound: string; name: string; chain: string; octave?: number; volume?: number };
  /** the 808 / bass plays on every kick (trap style) or its own line */
  bass: 'kick' | 'line';
  bassChain: string;
  pump: number;
  master: [EffectType, Record<string, ParamValue>][];
  drumBus: string;
}

const trapMaster = FX_CHAINS.find((c) => c.name === 'Hard Trap Master')!.effects;

export const BEAT_GENRES: BeatGenre[] = [
  {
    id: 'trap', name: 'Trap', emoji: '🔥', description: 'Hard 808s, rolling hi-hats, dark piano and bells.', kit: 'trap', mood: 'dark', style: 'trap', minor: true,
    chords: { sound: 'keys.upright', name: 'Piano', chain: 'Sad Piano', volume: 0.7 },
    melody: { sound: 'mallet.glock', name: 'Bells', chain: 'Dark Bells', volume: 0.55 },
    bass: 'kick', bassChain: '808 Grit', pump: 0.25, master: trapMaster, drumBus: 'Drum Bus Punch',
  },
  {
    id: 'drill', name: 'Drill', emoji: '🥶', description: 'Sliding 808s, bouncy hats and dark staccato strings.', kit: 'drill', mood: 'dark', style: 'trap', minor: true,
    chords: { sound: 'strings.cellos.sus', name: 'Cellos', chain: 'Sad Piano', octave: -12, volume: 0.55 },
    melody: { sound: 'strings.violins.spic', name: 'Drill Strings', chain: 'Dark Bells', volume: 0.6 },
    bass: 'kick', bassChain: '808 Grit', pump: 0.2, master: trapMaster, drumBus: 'Drum Bus Punch',
  },
  {
    id: 'boombap', name: 'Boom Bap', emoji: '📻', description: '90s hip-hop: dusty drums, swing, warm keys.', kit: 'boombap', mood: 'chill', style: 'hiphop', minor: true,
    chords: { sound: 'keys.epiano', name: 'Keys', chain: 'Lo-Fi Keys', volume: 0.7 },
    melody: { sound: 'brass.horn.sus', name: 'Horns', chain: 'Sad Piano', octave: -12, volume: 0.4 },
    bass: 'line', bassChain: '808 Grit', pump: 0, drumBus: 'Drum Bus Punch',
    master: [['eq', { low: 1, lowFreq: 70, high: -1 }], ['compressor', { threshold: -16, ratio: 2.5, attack: 30, release: 150, knee: 8, makeup: 2 }], ['tape', { wow: 0.1, flutter: 0.1, drive: 0.35, tone: 12000 }], ['limiter', { drive: 2, ceiling: -0.5, mode: 'true' }]],
  },
  {
    id: 'lofi', name: 'Lo-Fi', emoji: '🌙', description: 'Chill study beats: soft drums, jazzy keys, vinyl crackle.', kit: 'lofi', mood: 'chill', style: 'lofi', minor: false,
    chords: { sound: 'keys.epiano', name: 'Rhodes', chain: 'Lo-Fi Keys', volume: 0.75 },
    melody: { sound: 'mallet.musicbox', name: 'Music Box', chain: 'Dark Bells', volume: 0.4 },
    bass: 'line', bassChain: '808 Grit', pump: 0.3, drumBus: 'Drum Bus Punch',
    master: [['lofi', { bits: 12, rate: 22050, tone: 9000, wobble: 0.2, crackle: 0.3, mix: 1 }], ['tape', { wow: 0.25, flutter: 0.15, drive: 0.3, tone: 9000 }], ['limiter', { drive: 2, ceiling: -0.5, mode: 'true' }]],
  },
  {
    id: 'house', name: 'House', emoji: '🪩', description: 'Four on the floor, pumping pads and plucks.', kit: 'house', mood: 'happy', style: 'house', minor: false,
    chords: { sound: 'synth.pad', name: 'Pad', chain: 'Wide Pad', volume: 0.55 },
    melody: { sound: 'synth.pluck', name: 'Pluck', chain: 'Dark Bells', volume: 0.5 },
    bass: 'line', bassChain: '808 Grit', pump: 0.65, drumBus: 'Drum Bus Punch',
    master: [['ott', { depth: 0.2 }], ['clipper', { drive: 2, ceiling: -1.2, softness: 0.4 }], ['limiter', { drive: 1, ceiling: -0.5, mode: 'true' }]],
  },
  {
    id: 'phonk', name: 'Phonk', emoji: '🏎️', description: 'Drift phonk: cowbell melody, distorted 808, dark organ.', kit: 'phonk', mood: 'dark', style: 'trap', minor: true,
    chords: { sound: 'keys.organ', name: 'Organ', chain: 'Sad Piano', octave: -12, volume: 0.45 },
    bass: 'kick', bassChain: '808 Grit', pump: 0.2, master: trapMaster, drumBus: 'Drum Bus Punch',
  },
  {
    id: 'afro', name: 'Afrobeats', emoji: '🌴', description: 'Bouncy percussion, marimba and warm sub bass.', kit: 'afro', mood: 'happy', style: 'house', minor: false,
    chords: { sound: 'keys.epiano', name: 'Keys', chain: 'Lo-Fi Keys', volume: 0.55 },
    melody: { sound: 'mallet.marimba', name: 'Marimba', chain: 'Dark Bells', volume: 0.6 },
    bass: 'line', bassChain: '808 Grit', pump: 0.35, drumBus: 'Drum Bus Punch',
    master: [['ott', { depth: 0.15 }], ['clipper', { drive: 2, ceiling: -1.2, softness: 0.5 }], ['limiter', { drive: 1, ceiling: -0.5, mode: 'true' }]],
  },
];

export interface BeatOptions {
  genre: string;
  /** 0..11, or undefined for a random key */
  key?: number;
  seed?: number;
}

const BAR = 16 * TICKS_PER_STEP;

function chain(name: string): EffectInstance[] {
  const c = FX_CHAINS.find((x) => x.name === name);
  return (c?.effects ?? []).map(([type, params]) => makeEffect(type, params));
}

/**
 * A chain without its reverb: the beat uses one shared reverb bus instead
 * (much less CPU than a reverb on every insert). Returns the reverb's wet
 * amount as the send level.
 */
function dryChain(name: string): { inserts: EffectInstance[]; send: number } {
  const all = chain(name);
  const rev = all.find((e) => e.type === 'reverb');
  const wet = typeof rev?.params.wet === 'number' ? rev.params.wet : 0;
  return { inserts: all.filter((e) => e.type !== 'reverb'), send: Math.min(1, wet * 1.2) };
}

const stepValue = (c: string) => (c === 'x' ? 1 : c === 'o' ? 0.55 : 0);

/** 808 notes: one on every kick, at the root of the chord that is playing, held until the next kick. */
export function kick808(kit: ProKit, roots: number[], span: number, bars: number): Omit<Note, 'id'>[] {
  const kick = kit.slots.find((s) => proSound(s.sound)?.group === 'Kicks');
  const pattern = kick?.steps ?? 'x.......x.......';
  const len = pattern.length;
  const total = bars * 16;
  const hits: number[] = [];
  for (let s = 0; s < total; s++) if (stepValue(pattern[s % len]) > 0) hits.push(s);
  const out: Omit<Note, 'id'>[] = [];
  const low = (pc: number) => 28 + ((pc - 4 + 12) % 12); // E1 … D#2
  hits.forEach((s, i) => {
    const next = hits[i + 1] ?? total;
    const start = s * TICKS_PER_STEP;
    const root = roots[Math.min(roots.length - 1, Math.floor(start / span))];
    out.push({ pitch: low(root), start, length: Math.max(1, next - s) * TICKS_PER_STEP, velocity: 0.9 });
  });
  // an 808 slide up an octave at the end of every second bar (overlapping notes glide)
  for (let bar = 1; bar < bars; bar += 2) {
    const end = (bar + 1) * BAR;
    const last = [...out].reverse().find((n) => n.start < end - 2 * TICKS_PER_STEP && n.start + n.length >= end - TICKS_PER_STEP);
    if (last) out.push({ pitch: last.pitch + 12, start: end - 2 * TICKS_PER_STEP, length: 2 * TICKS_PER_STEP, velocity: 0.85 });
  }
  return out.sort((a, b) => a.start - b.start);
}

/** Builds a complete beat project (not loaded – see openBeatProject in the UI). */
export function createBeatProject(o: BeatOptions): Project {
  const genre = BEAT_GENRES.find((g) => g.id === o.genre) ?? BEAT_GENRES[0];
  const kit = PRO_KITS.find((k) => k.id === genre.kit) ?? PRO_KITS[0];
  const seed = o.seed ?? ((Math.random() * 0xffffffff) >>> 0);
  let s = seed;
  const rand = () => { s = (s * 1664525 + 1013904223) >>> 0; return s / 4294967296; };
  const minorKeys = [9, 0, 2, 4, 5, 7, 11, 1]; // A C D E F G B C# – popular beat keys
  const key = o.key ?? (genre.minor ? minorKeys[Math.floor(rand() * minorKeys.length)] : [0, 2, 5, 7, 9][Math.floor(rand() * 5)]);
  const mode = genre.minor ? 'minor' : 'major';

  // ---- mixer: master chain, drum bus, 808, chords, melody, spare inserts
  const master: MixerTrack = { ...makeMaster(), inserts: genre.master.map(([t, p]) => makeEffect(t, p)) };
  const mx = (i: number, name: string, color: string, inserts: EffectInstance[]): MixerTrack => ({ ...makeMixerTrack(i, name), color, inserts });
  const drumsMx = mx(1, 'Drums', kit.color, chain(genre.drumBus));
  const bassMx = mx(2, '808', '#ff5c5c', [...chain(genre.bassChain), ...(genre.pump > 0 ? [makeEffect('pump', { depth: genre.pump, release: 140 })] : [])]);
  const chordFx = genre.chords ? dryChain(genre.chords.chain) : { inserts: [], send: 0 };
  const melodyFx = genre.melody ? dryChain(genre.melody.chain) : { inserts: [], send: 0 };
  const chordsMx = mx(3, genre.chords?.name ?? 'Chords', '#5b8cff', chordFx.inserts);
  const melodyMx = mx(4, genre.melody?.name ?? 'Melody', '#9be15d', melodyFx.inserts);
  const reverbMx = mx(5, 'Reverb', '#8f6bff', [makeEffect('reverb', { size: 0.8, decay: 3, predelay: 20, damping: 6500, wet: 1, dry: 0 })]);
  const send = (m: MixerTrack, amount: number) => { if (amount > 0) m.sends.push({ id: uid('snd'), target: reverbMx.id, amount }); };
  send(chordsMx, chordFx.send);
  send(melodyMx, melodyFx.send);
  send(drumsMx, 0.06);
  const mixer: MixerTrack[] = [master, drumsMx, bassMx, chordsMx, melodyMx, reverbMx];
  for (let i = 6; i <= 12; i++) mixer.push(makeMixerTrack(i));

  // ---- channels
  const channels: Channel[] = [];
  const pro = (name: string, sound: string, mixerId: string, color: number, volume?: number) => {
    const ch = makeChannel('pro', name, mixerId, color, { pro: defaultPro(sound), volume: volume ?? proSound(sound)?.volume ?? 0.8 });
    channels.push(ch);
    return ch;
  };
  const drumCh = kit.slots.map((slot, i) => ({ slot, ch: pro(slot.name, slot.sound, drumsMx.id, i, slot.volume) }));
  const bassSound = kit.bass?.sound ?? 'bass.808.classic';
  const bassCh = pro(genre.bass === 'kick' ? '808' : 'Bass', bassSound, bassMx.id, 9);
  const chordCh = genre.chords ? pro(genre.chords.name, genre.chords.sound, chordsMx.id, 4, genre.chords.volume) : null;
  const melodyCh = genre.melody ? pro(genre.melody.name, genre.melody.sound, melodyMx.id, 6, genre.melody.volume) : null;

  // ---- music from the idea composer (4 bars)
  const seeds = { chords: (seed ^ 0x1234) >>> 0, bass: (seed ^ 0x5678) >>> 0, melody: (seed ^ 0x9abc) >>> 0, drums: (seed ^ 0xdef0) >>> 0 };
  const ideaOpts = { key, mode, mood: genre.mood, style: genre.style, bars: 4, seeds } as const;
  const idea = rollIdea(ideaOpts);
  const { chords, span } = ideaChords(ideaOpts);
  const nid = () => uid('n');

  // drums pattern (the kit groove) and a break variation (hats and percussion only)
  const drums = { ...makePattern(1, 32), name: 'Drums', color: kit.color };
  const brk = { ...makePattern(2, 32), name: 'Break', color: '#56636e' };
  for (const { slot, ch } of drumCh) {
    if (!slot.steps) continue;
    const steps = Array.from({ length: 32 }, (_, i) => stepValue(slot.steps![i] ?? '.'));
    drums.steps[ch.id] = steps;
    const group = proSound(slot.sound)?.group;
    if (group === 'Hi-Hats' || group === 'Percussion') brk.steps[ch.id] = steps.map((v, i) => (i % 2 === 0 ? v : 0));
    if (slot.rolls) {
      const r = Array.from({ length: 32 }, () => 1);
      for (const [i, n] of Object.entries(slot.rolls)) r[Number(i)] = n;
      drums.rolls = { ...(drums.rolls ?? {}), [ch.id]: r };
    }
  }
  const bass: Pattern = { ...makePattern(3, 64), name: genre.bass === 'kick' ? '808' : 'Bass', color: '#ff5c5c' };
  const roots = chords.map((c) => c.root);
  if (genre.bass === 'kick') {
    bass.notes[bassCh.id] = kick808(kit, roots, span, 4).map((n) => ({ ...n, id: nid() }));
  } else {
    // the idea's bass line, an octave lower for the sub / 808
    bass.notes[bassCh.id] = toNotes(idea.bass.map((n) => ({ ...n, pitch: n.pitch - 12 })), nid);
  }
  const chordPt: Pattern = { ...makePattern(4, 64), name: genre.chords?.name ?? 'Chords', color: '#5b8cff' };
  if (chordCh) chordPt.notes[chordCh.id] = toNotes(idea.chords.map((n) => ({ ...n, pitch: n.pitch + (genre.chords?.octave ?? 0), velocity: Math.min(1, n.velocity * 0.9) })), nid);
  const melodyPt: Pattern = { ...makePattern(5, 64), name: genre.melody?.name ?? 'Melody', color: '#9be15d' };
  if (melodyCh) melodyPt.notes[melodyCh.id] = toNotes(idea.melody.map((n) => ({ ...n, pitch: n.pitch + (genre.melody?.octave ?? 0) })), nid);
  const patterns = [drums, bass, chordPt, ...(melodyCh ? [melodyPt] : []), brk];

  // ---- arrangement: intro 4 · drop 8 · break 4 · drop 8 bars
  const tracks: PlaylistTrack[] = Array.from({ length: 8 }, (_, i) => makeTrack(i + 1));
  tracks[0] = { ...tracks[0], name: 'Drums', color: kit.color };
  tracks[1] = { ...tracks[1], name: genre.bass === 'kick' ? '808' : 'Bass', color: '#ff5c5c' };
  tracks[2] = { ...tracks[2], name: genre.chords?.name ?? 'Chords', color: '#5b8cff' };
  tracks[3] = { ...tracks[3], name: genre.melody?.name ?? 'Melody', color: '#9be15d' };
  const clips: Clip[] = [];
  const clip = (track: number, pattern: Pattern, bar: number, bars: number) =>
    clips.push({ id: uid('cl'), trackId: tracks[track].id, kind: 'pattern', patternId: pattern.id, start: bar * BAR, length: bars * BAR, offset: 0 });
  clip(2, chordPt, 0, 4);
  if (melodyCh) clip(3, melodyPt, 2, 2);
  for (const at of [4, 16]) {
    clip(0, drums, at, 8);
    clip(1, bass, at, 8);
    clip(2, chordPt, at, 8);
    if (melodyCh) clip(3, melodyPt, at, 8);
  }
  clip(0, brk, 12, 4);
  clip(2, chordPt, 12, 4);

  return {
    version: 1, id: uid('prj'), name: `${genre.name} Beat`, bpm: kit.bpm, timeSig: [4, 4], swing: kit.swing, key, scale: genre.minor ? 'Minor' : 'Major',
    channels, patterns, tracks, clips, loop: { enabled: false, start: 0, end: PPQ * 16 }, mixer, assets: {},
    notes: `${genre.emoji} ${genre.name} beat in ${['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'][key]} ${genre.minor ? 'minor' : 'major'} · ${idea.progression}\n\nIntro (bar 1) · Drop (bar 5) · Break (bar 13) · Drop (bar 17)`,
    markers: [
      { id: uid('mk'), tick: 0, name: 'Intro', color: '#56ccf2' },
      { id: uid('mk'), tick: 4 * BAR, name: 'Drop', color: '#ff5c5c' },
      { id: uid('mk'), tick: 12 * BAR, name: 'Break', color: '#9be15d' },
      { id: uid('mk'), tick: 16 * BAR, name: 'Drop 2', color: '#ff5c5c' },
    ],
    createdAt: Date.now(), updatedAt: Date.now(),
  };
}
