// Project doctor: turns anything that claims to be a project (an old save, an
// imported file, a half-written autosave) into a valid Project. Missing
// fields get defaults, numbers that are not finite or out of range are
// replaced, references to things that no longer exist are dropped and mixer
// feedback loops are cut. Every repair is counted so the user can be told.
//
// The audio engine relies on this: it never has to deal with `undefined`,
// NaN or dangling ids, which used to throw inside the scheduler and stop
// playback of older projects completely.
import type {
  AutomationData, AutomationTarget, Channel, Clip, DreamParams, DrumParams, EffectInstance, Marker, MixerTrack, Note, OscParams,
  ParamValue, Pattern, PlaylistTrack, Project, SamplerParams, Send, SynthParams,
} from './types';
import { MASTER_ID, PPQ } from './types';
import { EFFECTS } from '../audio/effects/descriptors';
import { defaultDream, defaultDrum, defaultSampler, defaultSynth, makeMaster, makePattern, makeTrack } from './defaults';
import { uid } from './utils';

export interface RepairReport {
  project: Project;
  /** human readable list of what was repaired (empty = nothing to do) */
  fixes: string[];
  /** total number of individual repairs */
  count: number;
}

type Obj = Record<string, unknown>;

const BAD_KEYS = new Set(['__proto__', 'constructor', 'prototype']);
const isObj = (v: unknown): v is Obj => typeof v === 'object' && v !== null && !Array.isArray(v);

class Fixer {
  private counts = new Map<string, number>();
  note(what: string, n = 1) {
    this.counts.set(what, (this.counts.get(what) ?? 0) + n);
  }
  get total() {
    let n = 0;
    for (const v of this.counts.values()) n += v;
    return n;
  }
  list() {
    return [...this.counts.entries()].map(([k, n]) => (n > 1 ? `${k} (${n}×)` : k));
  }
}

function num(v: unknown, min: number, max: number, def: number, f: Fixer, what: string): number {
  if (typeof v === 'number' && Number.isFinite(v)) {
    if (v < min || v > max) {
      f.note(what);
      return Math.min(max, Math.max(min, v));
    }
    return v;
  }
  if (v !== undefined) f.note(what);
  else f.note(`${what.split(' ')[0]} values missing (filled with defaults)`);
  return def;
}

/** Like num() but a missing value is silently filled (optional fields). */
function opt(v: unknown, min: number, max: number, def: number, f: Fixer, what: string): number {
  if (v === undefined) return def;
  return num(v, min, max, def, f, what);
}

function int(v: unknown, min: number, max: number, def: number, f: Fixer, what: string) {
  return Math.round(num(v, min, max, def, f, what));
}

function bool(v: unknown, def: boolean): boolean {
  return typeof v === 'boolean' ? v : def;
}

function str(v: unknown, def: string, max = 200): string {
  return typeof v === 'string' ? v.slice(0, max) : def;
}

function oneOf<T extends string>(v: unknown, options: readonly T[], def: T, f: Fixer, what: string): T {
  if (typeof v === 'string' && (options as readonly string[]).includes(v)) return v as T;
  if (v !== undefined) f.note(what);
  return def;
}

function color(v: unknown, def: string): string {
  return typeof v === 'string' && /^#[0-9a-fA-F]{3,8}$/.test(v) ? v : def;
}

function id(v: unknown, prefix: string, used: Set<string>, f: Fixer, what: string): string {
  let s = typeof v === 'string' && v.length > 0 && v.length < 120 && !BAD_KEYS.has(v) ? v : '';
  if (!s || used.has(s)) {
    if (s) f.note(what);
    s = uid(prefix);
  }
  used.add(s);
  return s;
}

const WAVES = ['sine', 'sawtooth', 'square', 'triangle'] as const;
const DREAM_WAVES = ['sine', 'triangle', 'sawtooth', 'square', 'glass', 'piano', 'organ', 'choir'] as const;
const FILTERS = ['lowpass', 'highpass', 'bandpass'] as const;

function osc(v: unknown, def: OscParams, f: Fixer): OscParams {
  const o = isObj(v) ? v : {};
  if (!isObj(v)) f.note('Synth values missing (filled with defaults)');
  const w = 'Synth setting out of range';
  return {
    wave: oneOf(o.wave, WAVES, def.wave, f, w),
    octave: int(o.octave, -3, 3, def.octave, f, w),
    semi: int(o.semi, -24, 24, def.semi, f, w),
    detune: num(o.detune, -100, 100, def.detune, f, w),
    level: num(o.level, 0, 1, def.level, f, w),
  };
}

export function normalizeSynth(v: unknown, f = new Fixer()): SynthParams {
  const d = defaultSynth();
  const s = isObj(v) ? v : {};
  const w = 'Synth setting out of range';
  return {
    osc1: osc(s.osc1, d.osc1, f),
    osc2: osc(s.osc2, d.osc2, f),
    unisonDetune: num(s.unisonDetune, 0, 100, d.unisonDetune, f, w),
    filterType: oneOf(s.filterType, FILTERS, d.filterType, f, w),
    cutoff: num(s.cutoff, 20, 20000, d.cutoff, f, w),
    resonance: num(s.resonance, 0, 30, d.resonance, f, w),
    filterEnv: num(s.filterEnv, -1, 1, d.filterEnv, f, w),
    attack: num(s.attack, 0, 20, d.attack, f, w),
    decay: num(s.decay, 0, 20, d.decay, f, w),
    sustain: num(s.sustain, 0, 1, d.sustain, f, w),
    release: num(s.release, 0, 30, d.release, f, w),
    lfoWave: oneOf(s.lfoWave, WAVES, d.lfoWave, f, w),
    lfoRate: num(s.lfoRate, 0, 50, d.lfoRate, f, w),
    lfoDepth: num(s.lfoDepth, 0, 1, d.lfoDepth, f, w),
    lfoTarget: oneOf(s.lfoTarget, ['pitch', 'filter', 'amp'] as const, d.lfoTarget, f, w),
    glide: num(s.glide, 0, 5, d.glide, f, w),
    volume: num(s.volume, 0, 1.5, d.volume, f, w),
  };
}

const DRUM_TYPES = ['kick', 'snare', 'clap', 'hihat', 'openhat', 'tom', 'perc', 'rim', 'cowbell', 'shaker', 'crash'] as const;

export function normalizeDrum(v: unknown, f = new Fixer()): DrumParams {
  const s = isObj(v) ? v : {};
  const type = oneOf(s.type, DRUM_TYPES, 'kick', f, 'Unknown drum type');
  const d = defaultDrum(type);
  const w = 'Drum setting out of range';
  return {
    type,
    tune: num(s.tune, -48, 48, d.tune, f, w),
    decay: num(s.decay, 0.05, 5, d.decay, f, w),
    tone: num(s.tone, 0, 1, d.tone, f, w),
    snap: num(s.snap, 0, 1, d.snap, f, w),
    level: num(s.level, 0, 1.5, d.level, f, w),
  };
}

export function normalizeSampler(v: unknown, f = new Fixer()): SamplerParams {
  const s = isObj(v) ? v : {};
  const d = defaultSampler();
  const w = 'Sampler setting out of range';
  return {
    assetId: typeof s.assetId === 'string' ? s.assetId : null,
    rootNote: int(s.rootNote, 0, 127, d.rootNote, f, w),
    tune: num(s.tune, -48, 48, d.tune, f, w),
    attack: num(s.attack, 0, 10, d.attack, f, w),
    release: num(s.release, 0, 20, d.release, f, w),
    reverse: bool(s.reverse, d.reverse),
    level: num(s.level, 0, 1.5, d.level, f, w),
  };
}

export function normalizeDream(v: unknown, f = new Fixer()): DreamParams {
  const s = isObj(v) ? v : {};
  const d = defaultDream();
  const w = 'DreamChord setting out of range';
  return {
    preset: str(s.preset, d.preset, 60),
    key: int(s.key, 0, 11, d.key, f, w),
    mode: oneOf(s.mode, ['major', 'minor'] as const, d.mode, f, w),
    octave: int(s.octave, -2, 2, d.octave, f, w),
    voicing: oneOf(s.voicing, ['close', 'open', 'spread'] as const, d.voicing, f, w),
    waveA: oneOf(s.waveA, DREAM_WAVES, d.waveA, f, w),
    waveB: oneOf(s.waveB, DREAM_WAVES, d.waveB, f, w),
    mixB: num(s.mixB, 0, 1, d.mixB, f, w),
    detune: num(s.detune, 0, 60, d.detune, f, w),
    sub: num(s.sub, 0, 1, d.sub, f, w),
    air: num(s.air, 0, 1, d.air, f, w),
    cutoff: num(s.cutoff, 60, 18000, d.cutoff, f, w),
    resonance: num(s.resonance, 0, 18, d.resonance, f, w),
    filterEnv: num(s.filterEnv, 0, 1, d.filterEnv, f, w),
    attack: num(s.attack, 0.001, 8, d.attack, f, w),
    decay: num(s.decay, 0.01, 10, d.decay, f, w),
    sustain: num(s.sustain, 0, 1, d.sustain, f, w),
    release: num(s.release, 0.01, 12, d.release, f, w),
    vibrato: num(s.vibrato, 0, 1, d.vibrato, f, w),
    tape: num(s.tape, 0, 1, d.tape, f, w),
    chorus: num(s.chorus, 0, 1, d.chorus, f, w),
    delay: num(s.delay, 0, 1, d.delay, f, w),
    delayTime: oneOf(s.delayTime, ['1/4', '1/8', '1/8d', '1/16'] as const, d.delayTime, f, w),
    reverb: num(s.reverb, 0, 1, d.reverb, f, w),
    space: num(s.space, 0, 1, d.space, f, w),
    volume: num(s.volume, 0, 1.5, d.volume, f, w),
    strum: num(s.strum, 0, 400, d.strum, f, w),
    strumDir: oneOf(s.strumDir, ['up', 'down'] as const, d.strumDir, f, w),
    arp: oneOf(s.arp, ['off', 'up', 'down', 'updown', 'random', 'chord'] as const, d.arp, f, w),
    arpRate: oneOf(s.arpRate, ['1/4', '1/8', '1/8t', '1/16', '1/16t', '1/32'] as const, d.arpRate, f, w),
    arpOctaves: int(s.arpOctaves, 1, 3, d.arpOctaves, f, w),
    arpGate: num(s.arpGate, 0.05, 1, d.arpGate, f, w),
    hold: bool(s.hold, d.hold),
    joystick: oneOf(s.joystick, ['chord', 'bend', 'filter'] as const, d.joystick, f, w),
  };
}

/** Effect instance with known type, every param present and in range. */
export function normalizeEffect(v: unknown, used: Set<string> = new Set(), f: Fixer = new Fixer()): EffectInstance | null {
  if (!isObj(v)) {
    f.note('Broken effect removed');
    return null;
  }
  const desc = typeof v.type === 'string' && Object.prototype.hasOwnProperty.call(EFFECTS, v.type) ? EFFECTS[v.type as keyof typeof EFFECTS] : null;
  if (!desc) {
    f.note('Unknown effect removed');
    return null;
  }
  const src = isObj(v.params) ? v.params : {};
  const params: Record<string, ParamValue> = {};
  for (const def of desc.params) {
    const raw = src[def.key];
    if (def.kind === 'number') {
      // number params sometimes arrive as strings from old saves
      const parsed = typeof raw === 'string' ? parseFloat(raw) : raw;
      params[def.key] = raw === undefined ? def.default : num(parsed, def.min, def.max, def.default, f, 'Effect setting out of range');
    } else {
      const val = raw === undefined || raw === null ? def.default : String(raw);
      if (def.options.some((o) => o.value === val)) params[def.key] = val;
      else {
        f.note('Effect setting out of range');
        params[def.key] = def.default;
      }
    }
  }
  return { id: id(v.id, 'fx', used, f, 'Duplicate id renamed'), type: desc.type, bypass: bool(v.bypass, false), params };
}

function effects(v: unknown, used: Set<string>, f: Fixer): EffectInstance[] {
  if (!Array.isArray(v)) return [];
  return v.map((e) => normalizeEffect(e, used, f)).filter((e): e is EffectInstance => !!e).slice(0, 32);
}

function mixerTrack(v: unknown, index: number, used: Set<string>, fxIds: Set<string>, f: Fixer): MixerTrack | null {
  if (!isObj(v)) {
    f.note('Broken mixer track removed');
    return null;
  }
  const isMaster = v.id === MASTER_ID;
  const w = 'Mixer value out of range';
  return {
    id: isMaster ? MASTER_ID : id(v.id, 'mx', used, f, 'Duplicate id renamed'),
    name: str(v.name, isMaster ? 'Master' : `Insert ${index}`, 60),
    color: color(v.color, '#56636e'),
    volume: num(v.volume, 0, 2, 0.8, f, w),
    pan: num(v.pan, -1, 1, 0, f, w),
    mute: bool(v.mute, false),
    solo: bool(v.solo, false),
    inserts: effects(v.inserts, fxIds, f),
    sends: Array.isArray(v.sends) ? (v.sends as unknown[]).filter(isObj).map((s) => ({
      id: typeof s.id === 'string' ? s.id : uid('s'),
      target: typeof s.target === 'string' ? s.target : '',
      amount: num(s.amount, 0, 1, 0.3, f, w),
    })) : [],
    output: isMaster ? null : typeof v.output === 'string' ? v.output : MASTER_ID,
  };
}

/** Drops outputs and sends that point nowhere or would create a feedback loop. */
function fixRouting(mixer: MixerTrack[], f: Fixer) {
  const ids = new Set(mixer.map((m) => m.id));
  const edges = new Map<string, Set<string>>(mixer.map((m) => [m.id, new Set<string>()]));
  const reaches = (from: string, to: string): boolean => {
    const stack = [from];
    const seen = new Set<string>();
    while (stack.length) {
      const n = stack.pop()!;
      if (n === to) return true;
      if (seen.has(n)) continue;
      seen.add(n);
      for (const e of edges.get(n) ?? []) stack.push(e);
    }
    return false;
  };
  const tryEdge = (from: string, to: string) => {
    if (from === to || !ids.has(to) || reaches(to, from)) return false;
    edges.get(from)!.add(to);
    return true;
  };
  for (const m of mixer) {
    if (m.id === MASTER_ID) {
      if (m.sends.length) f.note('Master sends removed');
      m.sends = [];
      m.output = null;
      continue;
    }
    if (!m.output || !tryEdge(m.id, m.output)) {
      if (m.output !== MASTER_ID) f.note('Mixer output repaired');
      m.output = MASTER_ID;
      edges.get(m.id)!.add(MASTER_ID);
    }
  }
  for (const m of mixer) {
    const seenTargets = new Set<string>();
    const sendIds = new Set<string>();
    const keep: Send[] = [];
    for (const s of m.sends) {
      if (s.target === MASTER_ID || seenTargets.has(s.target) || !tryEdge(m.id, s.target)) {
        f.note('Invalid or looping send removed');
        continue;
      }
      seenTargets.add(s.target);
      keep.push(sendIds.has(s.id) ? { ...s, id: uid('s') } : s);
      sendIds.add(s.id);
    }
    m.sends = keep;
  }
}

function channel(v: unknown, used: Set<string>, mixerIds: Set<string>, f: Fixer): Channel | null {
  if (!isObj(v)) {
    f.note('Broken channel removed');
    return null;
  }
  const kind = v.kind;
  if (kind !== 'synth' && kind !== 'drum' && kind !== 'sampler' && kind !== 'dream') {
    f.note('Unknown instrument removed');
    return null;
  }
  const w = 'Channel value out of range';
  const mixerTrackId = typeof v.mixerTrackId === 'string' && mixerIds.has(v.mixerTrackId) ? v.mixerTrackId : MASTER_ID;
  if (mixerTrackId !== v.mixerTrackId) f.note('Channel routing repaired');
  const ch: Channel = {
    id: id(v.id, 'ch', used, f, 'Duplicate id renamed'),
    name: str(v.name, 'Channel', 60),
    color: color(v.color, '#56636e'),
    kind,
    volume: num(v.volume, 0, 1.5, 0.8, f, w),
    pan: num(v.pan, -1, 1, 0, f, w),
    mute: bool(v.mute, false),
    solo: bool(v.solo, false),
    mixerTrackId,
    rootNote: int(v.rootNote, 0, 127, 60, f, w),
  };
  if (kind === 'synth') ch.synth = normalizeSynth(v.synth, f);
  if (kind === 'drum') ch.drum = normalizeDrum(v.drum, f);
  if (kind === 'sampler') ch.sampler = normalizeSampler(v.sampler, f);
  if (kind === 'dream') ch.dream = normalizeDream(v.dream, f);
  return ch;
}

function pattern(v: unknown, used: Set<string>, channelIds: Set<string>, f: Fixer, index: number): Pattern | null {
  if (!isObj(v)) {
    f.note('Broken pattern removed');
    return null;
  }
  const p: Pattern = {
    id: id(v.id, 'pt', used, f, 'Duplicate id renamed'),
    name: str(v.name, `Pattern ${index + 1}`, 60),
    color: color(v.color, '#f28c28'),
    lengthSteps: int(v.lengthSteps, 1, 1024, 16, f, 'Pattern length repaired'),
    steps: {},
    notes: {},
  };
  const noteIds = new Set<string>();
  if (isObj(v.steps)) {
    for (const [chId, list] of Object.entries(v.steps)) {
      if (BAD_KEYS.has(chId) || !channelIds.has(chId) || !Array.isArray(list)) continue;
      p.steps[chId] = list.slice(0, 1024).map((x) => (typeof x === 'number' && Number.isFinite(x) ? Math.min(1, Math.max(0, x)) : 0));
    }
  }
  if (isObj(v.notes)) {
    for (const [chId, list] of Object.entries(v.notes)) {
      if (BAD_KEYS.has(chId) || !channelIds.has(chId) || !Array.isArray(list)) continue;
      const out: Note[] = [];
      for (const n of list.slice(0, 20000)) {
        if (!isObj(n) || typeof n.pitch !== 'number' || !Number.isFinite(n.pitch) || typeof n.start !== 'number' || !Number.isFinite(n.start)) {
          f.note('Broken note removed');
          continue;
        }
        out.push({
          id: id(n.id, 'n', noteIds, f, 'Duplicate id renamed'),
          pitch: int(n.pitch, 0, 127, 60, f, 'Note value out of range'),
          start: num(n.start, 0, 1e7, 0, f, 'Note value out of range'),
          length: num(n.length, 1, 1e6, PPQ / 4, f, 'Note value out of range'),
          velocity: num(n.velocity, 0, 1, 0.8, f, 'Note value out of range'),
        });
      }
      p.notes[chId] = out;
    }
  }
  return p;
}

function automationTarget(v: unknown): AutomationTarget | null {
  if (!isObj(v)) return null;
  if (v.kind === 'mixer' && typeof v.id === 'string' && (v.param === 'volume' || v.param === 'pan')) return { kind: 'mixer', id: v.id, param: v.param };
  if (v.kind === 'channel' && typeof v.id === 'string' && (v.param === 'volume' || v.param === 'pan' || v.param === 'cutoff')) return { kind: 'channel', id: v.id, param: v.param };
  if (v.kind === 'effect' && typeof v.trackId === 'string' && typeof v.effectId === 'string' && typeof v.param === 'string') {
    return { kind: 'effect', trackId: v.trackId, effectId: v.effectId, param: v.param.slice(0, 40) };
  }
  return null;
}

function automation(v: unknown, length: number, f: Fixer): AutomationData | null {
  if (!isObj(v)) return null;
  const target = automationTarget(v.target);
  if (!target) return null;
  const pts = Array.isArray(v.points) ? v.points : [];
  const points = pts
    .filter(isObj)
    .filter((pt) => typeof pt.t === 'number' && Number.isFinite(pt.t) && typeof pt.v === 'number' && Number.isFinite(pt.v))
    .map((pt) => ({ t: Math.min(length, Math.max(0, pt.t as number)), v: Math.min(1, Math.max(0, pt.v as number)) }))
    .sort((a, b) => a.t - b.t)
    .slice(0, 4000);
  if (points.length !== pts.length) f.note('Automation point repaired');
  if (points.length === 0) points.push({ t: 0, v: 0.5 });
  return { target, points };
}

function clip(v: unknown, used: Set<string>, trackIds: Set<string>, patternIds: Set<string>, fxIds: Set<string>, f: Fixer): Clip | null {
  if (!isObj(v) || typeof v.trackId !== 'string' || !trackIds.has(v.trackId)) {
    f.note('Clip without track removed');
    return null;
  }
  const kind = v.kind;
  if (kind !== 'pattern' && kind !== 'audio' && kind !== 'automation') {
    f.note('Unknown clip removed');
    return null;
  }
  if (kind === 'pattern' && (typeof v.patternId !== 'string' || !patternIds.has(v.patternId))) {
    f.note('Clip of a deleted pattern removed');
    return null;
  }
  if (kind === 'audio' && typeof v.assetId !== 'string') {
    f.note('Audio clip without audio removed');
    return null;
  }
  const w = 'Clip value out of range';
  const c: Clip = {
    id: id(v.id, 'c', used, f, 'Duplicate id renamed'),
    trackId: v.trackId,
    kind,
    start: num(v.start, 0, 1e8, 0, f, w),
    length: num(v.length, 1, 1e8, PPQ * 4, f, w),
    offset: opt(v.offset, 0, 1e8, 0, f, w),
  };
  if (kind === 'pattern') c.patternId = v.patternId as string;
  if (kind === 'audio') c.assetId = v.assetId as string;
  if (v.gain !== undefined) c.gain = opt(v.gain, 0, 8, 1, f, w);
  if (v.pan !== undefined) c.pan = opt(v.pan, -1, 1, 0, f, w);
  if (v.transpose !== undefined) c.transpose = Math.round(opt(v.transpose, -48, 48, 0, f, w));
  if (v.speed !== undefined) c.speed = opt(v.speed, 0.05, 8, 1, f, w);
  if (v.reverse !== undefined) c.reverse = bool(v.reverse, false);
  if (v.fadeIn !== undefined) c.fadeIn = opt(v.fadeIn, 0, 600, 0, f, w);
  if (v.fadeOut !== undefined) c.fadeOut = opt(v.fadeOut, 0, 600, 0, f, w);
  if (v.fx !== undefined) c.fx = effects(v.fx, fxIds, f);
  if (typeof v.name === 'string') c.name = v.name.slice(0, 80);
  if (kind === 'automation') {
    const a = automation(v.automation, c.length, f);
    if (!a) {
      f.note('Broken automation clip removed');
      return null;
    }
    c.automation = a;
  }
  return c;
}

/** Repairs anything shaped like a project. Throws only if it is not an object at all. */
export function normalizeProject(raw: unknown): RepairReport {
  if (!isObj(raw)) throw new Error('Not a Goofy Studio project');
  const f = new Fixer();
  const ts = Array.isArray(raw.timeSig) ? raw.timeSig : [];
  const timeSig: [number, number] = [
    int(ts[0], 1, 16, 4, f, 'Time signature repaired'),
    [2, 4, 8, 16].includes(ts[1] as number) ? (ts[1] as number) : 4,
  ];

  // mixer (master first, unique ids)
  const mixerIds = new Set<string>([MASTER_ID]);
  const fxIds = new Set<string>();
  const rawMixer = Array.isArray(raw.mixer) ? raw.mixer : [];
  let mixer = rawMixer.map((m, i) => mixerTrack(m, i, mixerIds, fxIds, f)).filter((m): m is MixerTrack => !!m);
  const masters = mixer.filter((m) => m.id === MASTER_ID);
  if (masters.length === 0) {
    f.note('Master track restored');
    mixer.unshift(makeMaster());
  } else if (mixer[0].id !== MASTER_ID || masters.length > 1) {
    f.note('Master track moved to the front');
    mixer = [masters[0], ...mixer.filter((m) => m.id !== MASTER_ID)];
  }
  fixRouting(mixer, f);
  const mixerSet = new Set(mixer.map((m) => m.id));

  const channelIds = new Set<string>();
  const channels = (Array.isArray(raw.channels) ? raw.channels : []).map((c) => channel(c, channelIds, mixerSet, f)).filter((c): c is Channel => !!c);

  const patternIds = new Set<string>();
  let patterns = (Array.isArray(raw.patterns) ? raw.patterns : []).map((p, i) => pattern(p, patternIds, channelIds, f, i)).filter((p): p is Pattern => !!p);
  if (patterns.length === 0) {
    f.note('Empty pattern added');
    patterns = [makePattern(1)];
    patternIds.add(patterns[0].id);
  }

  const trackIds = new Set<string>();
  let tracks: PlaylistTrack[] = (Array.isArray(raw.tracks) ? raw.tracks : []).filter(isObj).map((t, i) => {
    const mixerTrackId = typeof t.mixerTrackId === 'string' && mixerSet.has(t.mixerTrackId) ? t.mixerTrackId : MASTER_ID;
    if (mixerTrackId !== t.mixerTrackId) f.note('Track routing repaired');
    const tr: PlaylistTrack = {
      id: id(t.id, 'tr', trackIds, f, 'Duplicate id renamed'),
      name: str(t.name, `Track ${i + 1}`, 60),
      color: color(t.color, '#56636e'),
      mute: bool(t.mute, false),
      mixerTrackId,
    };
    if (typeof t.solo === 'boolean') tr.solo = t.solo;
    return tr;
  });
  if (tracks.length === 0) {
    f.note('Playlist track added');
    tracks = [makeTrack(1)];
    trackIds.add(tracks[0].id);
  }

  const clipIds = new Set<string>();
  const clips = (Array.isArray(raw.clips) ? raw.clips : []).map((c) => clip(c, clipIds, trackIds, patternIds, fxIds, f)).filter((c): c is Clip => !!c);

  const loopRaw = isObj(raw.loop) ? raw.loop : {};
  let loopStart = opt(loopRaw.start, 0, 1e8, 0, f, 'Loop repaired');
  let loopEnd = opt(loopRaw.end, 0, 1e8, PPQ * 16, f, 'Loop repaired');
  if (loopEnd <= loopStart) {
    if (raw.loop !== undefined) f.note('Loop repaired');
    loopStart = 0;
    loopEnd = PPQ * 16;
  }

  const assets: Project['assets'] = {};
  if (isObj(raw.assets)) {
    for (const [aid, a] of Object.entries(raw.assets)) {
      if (BAD_KEYS.has(aid) || !isObj(a)) continue;
      assets[aid] = {
        id: aid,
        name: str(a.name, 'Audio', 120),
        duration: opt(a.duration, 0, 1e6, 0, f, 'Audio info repaired'),
        sampleRate: opt(a.sampleRate, 3000, 768000, 44100, f, 'Audio info repaired'),
        channels: Math.round(opt(a.channels, 1, 32, 2, f, 'Audio info repaired')),
        ...(a.builtin === true || aid.startsWith('builtin:') ? { builtin: true } : {}),
      };
    }
  }

  const markers: Marker[] = (Array.isArray(raw.markers) ? raw.markers : []).filter(isObj)
    .filter((m) => typeof m.tick === 'number' && Number.isFinite(m.tick))
    .slice(0, 500)
    .map((m) => ({ id: typeof m.id === 'string' ? m.id : uid('mk'), tick: Math.max(0, m.tick as number), name: str(m.name, 'Marker', 40), color: color(m.color, '#f28c28') }));

  const now = Date.now();
  const project: Project = {
    version: 1,
    id: typeof raw.id === 'string' && raw.id ? raw.id.slice(0, 120) : uid('prj'),
    name: str(raw.name, 'Untitled', 120) || 'Untitled',
    bpm: num(raw.bpm, 20, 400, 120, f, 'Tempo repaired'),
    timeSig,
    swing: opt(raw.swing, 0, 1, 0, f, 'Swing repaired'),
    key: Math.round(opt(raw.key, 0, 11, 0, f, 'Key repaired')),
    scale: str(raw.scale, 'Major', 40),
    channels,
    patterns,
    tracks,
    clips,
    loop: { enabled: bool(loopRaw.enabled, false), start: loopStart, end: loopEnd },
    mixer,
    assets,
    createdAt: opt(raw.createdAt, 0, 1e15, now, f, 'Date repaired'),
    updatedAt: opt(raw.updatedAt, 0, 1e15, now, f, 'Date repaired'),
  };
  if (typeof raw.notes === 'string') project.notes = raw.notes.slice(0, 200000);
  if (markers.length) project.markers = markers;
  return { project, fixes: f.list(), count: f.total };
}
