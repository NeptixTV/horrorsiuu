import type {
  Channel, DrumParams, DrumType, EffectInstance, EffectType, MixerTrack, Note, Pattern, PlaylistTrack, Project, SamplerParams, SynthParams,
} from './types';
import { MASTER_ID, PPQ, TICKS_PER_STEP } from './types';
import { paletteColor, uid } from './utils';
import { defaultParams } from '../audio/effects/descriptors';

export function defaultSynth(): SynthParams {
  return {
    osc1: { wave: 'sawtooth', octave: 0, semi: 0, detune: -6, level: 0.8 },
    osc2: { wave: 'square', octave: -1, semi: 0, detune: 6, level: 0.4 },
    unisonDetune: 0,
    filterType: 'lowpass',
    cutoff: 2400,
    resonance: 2,
    filterEnv: 0.35,
    attack: 0.005,
    decay: 0.3,
    sustain: 0.6,
    release: 0.25,
    lfoWave: 'sine',
    lfoRate: 5,
    lfoDepth: 0,
    lfoTarget: 'pitch',
    glide: 0,
    volume: 0.8,
  };
}

export interface SynthPreset {
  name: string;
  category: 'Bass' | 'Lead' | 'Pad' | 'Pluck' | 'FX' | 'Keys';
  params: SynthParams;
}

const sp = (over: Partial<SynthParams>): SynthParams => ({ ...defaultSynth(), ...over });

export const SYNTH_PRESETS: SynthPreset[] = [
  { name: 'Init Saw', category: 'Lead', params: defaultSynth() },
  { name: 'Fat Bass', category: 'Bass', params: sp({
    osc1: { wave: 'sawtooth', octave: -1, semi: 0, detune: -8, level: 0.8 },
    osc2: { wave: 'square', octave: -2, semi: 0, detune: 0, level: 0.6 },
    cutoff: 650, resonance: 4, filterEnv: 0.5, decay: 0.25, sustain: 0.5, release: 0.12 }) },
  { name: 'Sub Bass', category: 'Bass', params: sp({
    osc1: { wave: 'sine', octave: -1, semi: 0, detune: 0, level: 1 },
    osc2: { wave: 'triangle', octave: -1, semi: 0, detune: 0, level: 0.25 },
    cutoff: 900, resonance: 0.7, filterEnv: 0, attack: 0.004, sustain: 1, release: 0.1 }) },
  { name: 'Acid Squelch', category: 'Bass', params: sp({
    osc1: { wave: 'sawtooth', octave: -1, semi: 0, detune: 0, level: 0.9 },
    osc2: { wave: 'square', octave: -1, semi: 0, detune: 0, level: 0 },
    cutoff: 400, resonance: 14, filterEnv: 0.8, decay: 0.18, sustain: 0.1, release: 0.08, glide: 0.06 }) },
  { name: 'Supersaw Lead', category: 'Lead', params: sp({
    osc1: { wave: 'sawtooth', octave: 0, semi: 0, detune: 0, level: 0.7 },
    osc2: { wave: 'sawtooth', octave: 1, semi: 0, detune: 9, level: 0.35 },
    unisonDetune: 18, cutoff: 5200, resonance: 1.2, filterEnv: 0.2, attack: 0.01, sustain: 0.8, release: 0.35 }) },
  { name: 'Square Lead', category: 'Lead', params: sp({
    osc1: { wave: 'square', octave: 0, semi: 0, detune: 0, level: 0.7 },
    osc2: { wave: 'square', octave: 0, semi: 7, detune: 4, level: 0.25 },
    cutoff: 3600, resonance: 3, lfoTarget: 'pitch', lfoDepth: 0.2, lfoRate: 5.5, release: 0.2, glide: 0.04 }) },
  { name: 'Soft Pad', category: 'Pad', params: sp({
    osc1: { wave: 'sawtooth', octave: 0, semi: 0, detune: -10, level: 0.5 },
    osc2: { wave: 'triangle', octave: 1, semi: 0, detune: 10, level: 0.5 },
    unisonDetune: 12, cutoff: 1800, resonance: 1, filterEnv: 0.1, attack: 0.6, decay: 1, sustain: 0.8, release: 1.4,
    lfoTarget: 'filter', lfoDepth: 0.25, lfoRate: 0.4, volume: 0.7 }) },
  { name: 'Glass Pluck', category: 'Pluck', params: sp({
    osc1: { wave: 'triangle', octave: 0, semi: 0, detune: 0, level: 0.8 },
    osc2: { wave: 'square', octave: 1, semi: 0, detune: 3, level: 0.2 },
    cutoff: 900, resonance: 5, filterEnv: 0.75, attack: 0.002, decay: 0.22, sustain: 0, release: 0.25 }) },
  { name: 'Bell Keys', category: 'Keys', params: sp({
    osc1: { wave: 'sine', octave: 0, semi: 0, detune: 0, level: 0.8 },
    osc2: { wave: 'sine', octave: 2, semi: 7, detune: 0, level: 0.3 },
    cutoff: 8000, resonance: 0.7, filterEnv: 0, attack: 0.002, decay: 1.2, sustain: 0.1, release: 0.9 }) },
  { name: 'Wobble Bass', category: 'Bass', params: sp({
    osc1: { wave: 'sawtooth', octave: -1, semi: 0, detune: -5, level: 0.8 },
    osc2: { wave: 'square', octave: -1, semi: 0, detune: 5, level: 0.6 },
    cutoff: 500, resonance: 9, filterEnv: 0.1, sustain: 1, lfoTarget: 'filter', lfoDepth: 0.85, lfoRate: 4 }) },
  { name: 'Goofy Boing', category: 'FX', params: sp({
    osc1: { wave: 'triangle', octave: 0, semi: 0, detune: 0, level: 0.9 },
    osc2: { wave: 'sine', octave: 1, semi: 0, detune: 0, level: 0.3 },
    cutoff: 6000, resonance: 1, filterEnv: 0, attack: 0.003, decay: 0.4, sustain: 0.3, release: 0.3,
    lfoTarget: 'pitch', lfoDepth: 1, lfoRate: 9, glide: 0.12 }) },
];

export function defaultDrum(type: DrumType): DrumParams {
  const base: DrumParams = { type, tune: 0, decay: 1, tone: 0.5, snap: 0.5, level: 0.9 };
  if (type === 'kick') return { ...base, tone: 0.4, snap: 0.4, level: 1 };
  if (type === 'hihat' || type === 'openhat' || type === 'shaker') return { ...base, level: 0.6 };
  return base;
}

export function defaultSampler(assetId: string | null = null): SamplerParams {
  return { assetId, rootNote: 60, tune: 0, attack: 0.002, release: 0.1, reverse: false, level: 0.9 };
}

export const DRUM_LABELS: Record<DrumType, string> = {
  kick: 'Kick', snare: 'Snare', clap: 'Clap', hihat: 'Hi-Hat', openhat: 'Open Hat', tom: 'Tom', perc: 'Perc',
  rim: 'Rim', cowbell: 'Cowbell', shaker: 'Shaker', crash: 'Crash',
};

export function makeEffect(type: EffectType, params?: EffectInstance['params']): EffectInstance {
  return { id: uid('fx'), type, bypass: false, params: { ...defaultParams(type), ...(params ?? {}) } };
}

export function makeMixerTrack(index: number, name?: string): MixerTrack {
  return {
    id: uid('mx'), name: name ?? `Insert ${index}`, color: paletteColor(index + 3), volume: 0.8, pan: 0,
    mute: false, solo: false, inserts: [], sends: [], output: MASTER_ID,
  };
}

export function makeMaster(): MixerTrack {
  return { id: MASTER_ID, name: 'Master', color: '#f28c28', volume: 0.85, pan: 0, mute: false, solo: false, inserts: [], sends: [], output: null };
}

export function makeChannel(kind: Channel['kind'], name: string, mixerTrackId: string, colorIndex: number, extra: Partial<Channel> = {}): Channel {
  const ch: Channel = {
    id: uid('ch'), name, color: paletteColor(colorIndex), kind, volume: 0.8, pan: 0, mute: false, solo: false,
    mixerTrackId, rootNote: 60,
  };
  if (kind === 'synth') ch.synth = defaultSynth();
  if (kind === 'drum') ch.drum = defaultDrum('kick');
  if (kind === 'sampler') ch.sampler = defaultSampler();
  return { ...ch, ...extra };
}

export function makePattern(index: number, lengthSteps = 16): Pattern {
  return { id: uid('pt'), name: `Pattern ${index}`, color: paletteColor(index * 3), lengthSteps, steps: {}, notes: {} };
}

export function makeTrack(index: number, mixerTrackId = MASTER_ID): PlaylistTrack {
  return { id: uid('tr'), name: `Track ${index}`, color: '#56636e', mute: false, mixerTrackId };
}

const steps = (s: string, vel = 1) => s.replace(/\s/g, '').split('').map((c) => (c === 'x' ? vel : c === 'o' ? vel * 0.55 : 0));

function notes(list: [number, number, number, number?][]): Note[] {
  // [pitch, startStep, lengthSteps, velocity]
  return list.map(([pitch, start, len, vel]) => ({
    id: uid('n'), pitch, start: start * TICKS_PER_STEP, length: len * TICKS_PER_STEP, velocity: vel ?? 0.8,
  }));
}

/** The demo project that greets users on first start. */
export function createDemoProject(): Project {
  const master = makeMaster();
  const mx = (i: number, name: string, color: string, inserts: EffectInstance[] = []) => ({ ...makeMixerTrack(i, name), color, inserts });
  const reverbBus = mx(9, 'Reverb Bus', '#8f6bff', [makeEffect('reverb', { wet: 1, dry: 0, size: 0.75, decay: 3.2 })]);
  const delayBus = mx(10, 'Delay Bus', '#4cd6a3', [makeEffect('delay', { mix: 1 })]);
  const kickMx = mx(1, 'Kick', '#ff5c5c', [makeEffect('eq', { low: 3, lowFreq: 60, lowMid: -3 })]);
  const snareMx = mx(2, 'Snare & Clap', '#ffbe3d', [makeEffect('compressor', { threshold: -16, ratio: 3, makeup: 2 })]);
  const hatMx = mx(3, 'Hats', '#c0c7cf');
  const percMx = mx(4, 'Perc', '#e3a75e');
  const bassMx = mx(5, 'Bass', '#5b8cff', [makeEffect('distortion', { drive: 0.2, mix: 0.4, output: -2 }), makeEffect('compressor', { threshold: -20, ratio: 4 })]);
  const leadMx = mx(6, 'Lead', '#3fc7e8', [makeEffect('chorus', { mix: 0.35 })]);
  const padMx = mx(7, 'Pad', '#d46bff', [makeEffect('filter', { cutoff: 3200, resonance: 1.2 })]);
  const vocalMx = mx(8, 'Vocals', '#f28c28', [
    makeEffect('pitch', { scale: 'Minor', key: '9', retune: 25 }),
    makeEffect('compressor', { threshold: -22, ratio: 4, attack: 5, makeup: 5 }),
    makeEffect('eq', { low: -8, lowFreq: 100, highMid: 2.5, high: 3 }),
  ]);
  hatMx.sends = [{ id: uid('s'), target: reverbBus.id, amount: 0.12 }];
  snareMx.sends = [{ id: uid('s'), target: reverbBus.id, amount: 0.25 }];
  leadMx.sends = [{ id: uid('s'), target: delayBus.id, amount: 0.3 }, { id: uid('s'), target: reverbBus.id, amount: 0.3 }];
  padMx.sends = [{ id: uid('s'), target: reverbBus.id, amount: 0.45 }];
  vocalMx.sends = [{ id: uid('s'), target: reverbBus.id, amount: 0.3 }];
  const mixer: MixerTrack[] = [master, kickMx, snareMx, hatMx, percMx, bassMx, leadMx, padMx, vocalMx, reverbBus, delayBus];
  for (let i = mixer.length; i <= 14; i++) mixer.push(makeMixerTrack(i));

  const drum = (type: DrumType, mxId: string, color: number, name?: string) =>
    makeChannel('drum', name ?? DRUM_LABELS[type], mxId, color, { drum: defaultDrum(type) });
  const kick = drum('kick', kickMx.id, 9);
  const clap = drum('clap', snareMx.id, 1);
  const snare = drum('snare', snareMx.id, 0);
  const hat = drum('hihat', hatMx.id, 10);
  const ohat = drum('openhat', hatMx.id, 11);
  const perc = drum('perc', percMx.id, 12);
  const tom = drum('tom', percMx.id, 4);
  const bass = makeChannel('synth', 'Fat Bass', bassMx.id, 5, { synth: SYNTH_PRESETS[1].params, volume: 0.75 });
  const lead = makeChannel('synth', 'Supersaw Lead', leadMx.id, 4, { synth: SYNTH_PRESETS[4].params, volume: 0.55 });
  const pad = makeChannel('synth', 'Soft Pad', padMx.id, 7, { synth: SYNTH_PRESETS[6].params, volume: 0.5 });
  const channels = [kick, clap, snare, hat, ohat, perc, tom, bass, lead, pad];

  // Pattern 1: beat
  const beat = makePattern(1);
  beat.name = 'Beat';
  beat.color = '#f28c28';
  beat.steps[kick.id] = steps('x... x... x... x..o');
  beat.steps[clap.id] = steps('.... x... .... x...');
  beat.steps[snare.id] = steps('.... .... .... ...o');
  beat.steps[hat.id] = steps('..x. ..x. ..x. ..x.');
  beat.steps[ohat.id] = steps('.... .... .... ..o.');
  beat.steps[perc.id] = steps('...o ..o. .o.. ....');

  // Pattern 2: bassline (A minor, 2 bars)
  const bassPat = makePattern(2, 32);
  bassPat.name = 'Bassline';
  bassPat.color = '#5b8cff';
  bassPat.notes[bass.id] = notes([
    [45, 0, 3], [45, 4, 2], [57, 6, 1, 0.6], [45, 8, 3], [48, 12, 2], [50, 14, 2],
    [41, 16, 3], [41, 20, 2], [53, 22, 1, 0.6], [43, 24, 3], [43, 28, 2], [47, 30, 2],
  ]);

  // Pattern 3: chords + lead (2 bars)
  const chords = makePattern(3, 32);
  chords.name = 'Chords & Lead';
  chords.color = '#d46bff';
  chords.notes[pad.id] = notes([
    [57, 0, 16, 0.6], [60, 0, 16, 0.6], [64, 0, 16, 0.6],
    [53, 16, 8, 0.6], [57, 16, 8, 0.6], [60, 16, 8, 0.6],
    [55, 24, 8, 0.6], [59, 24, 8, 0.6], [62, 24, 8, 0.6],
  ]);
  chords.notes[lead.id] = notes([
    [76, 0, 2], [72, 2, 2], [74, 4, 2], [76, 6, 4], [79, 10, 2], [76, 12, 4],
    [77, 16, 2], [76, 18, 2], [74, 20, 4], [72, 24, 2], [74, 26, 2], [71, 28, 4],
  ]);

  // Pattern 4: fill
  const fill = makePattern(4);
  fill.name = 'Drum Fill';
  fill.color = '#ff5c5c';
  fill.steps[kick.id] = steps('x... x... x.x. xxxx');
  fill.steps[snare.id] = steps('.... x... .x.x xxxx', 0.8);
  fill.steps[tom.id] = steps('..x. ..x. x.x. ....');
  fill.steps[hat.id] = steps('x.x. x.x. x.x. ....');

  const patterns = [beat, bassPat, chords, fill];

  const tracks = Array.from({ length: 12 }, (_, i) => makeTrack(i + 1));
  tracks[0].name = 'Drums';
  tracks[0].color = '#a0522d';
  tracks[1].name = 'Bass';
  tracks[2].name = 'Music';
  tracks[3].name = 'Vocals';
  tracks[3].mixerTrackId = vocalMx.id;
  tracks[4].name = 'FX';

  const bar = PPQ * 4;
  const clips: Project['clips'] = [];
  const place = (track: number, pattern: Pattern, startBar: number, bars: number) =>
    clips.push({ id: uid('c'), trackId: tracks[track].id, kind: 'pattern', start: startBar * bar, length: bars * bar, offset: 0, patternId: pattern.id });
  for (let b = 0; b < 7; b++) place(0, beat, b, 1);
  place(0, fill, 7, 1);
  for (let b = 2; b < 8; b += 2) place(1, bassPat, b, 2);
  for (let b = 4; b < 8; b += 2) place(2, chords, b, 2);

  return {
    version: 1,
    id: uid('prj'),
    name: 'Goofy Demo',
    bpm: 128,
    timeSig: [4, 4],
    swing: 0,
    key: 9,
    scale: 'Minor',
    channels,
    patterns,
    tracks,
    clips,
    loop: { enabled: false, start: 0, end: bar * 4 },
    mixer,
    assets: {},
    createdAt: Date.now(),
    updatedAt: Date.now(),
  };
}

/** An empty project (File → New). */
export function createEmptyProject(): Project {
  const master = makeMaster();
  const mixer: MixerTrack[] = [master];
  for (let i = 1; i <= 12; i++) mixer.push(makeMixerTrack(i));
  const kick = makeChannel('drum', 'Kick', mixer[1].id, 9, { drum: defaultDrum('kick') });
  const clap = makeChannel('drum', 'Clap', mixer[2].id, 1, { drum: defaultDrum('clap') });
  const hat = makeChannel('drum', 'Hi-Hat', mixer[3].id, 10, { drum: defaultDrum('hihat') });
  const snare = makeChannel('drum', 'Snare', mixer[2].id, 0, { drum: defaultDrum('snare') });
  const synth = makeChannel('synth', 'Goofy Synth', mixer[4].id, 4);
  const p1 = makePattern(1);
  const tracks = Array.from({ length: 12 }, (_, i) => makeTrack(i + 1));
  return {
    version: 1, id: uid('prj'), name: 'Untitled', bpm: 130, timeSig: [4, 4], swing: 0, key: 0, scale: 'Major',
    channels: [kick, clap, hat, snare, synth], patterns: [p1], tracks, clips: [],
    loop: { enabled: false, start: 0, end: PPQ * 16 }, mixer, assets: {}, createdAt: Date.now(), updatedAt: Date.now(),
  };
}
