// Domain operations on the project. Every function is a thin, named wrapper
// around `update` so it produces a labelled undo step.
import type {
  AssetMeta, Channel, ChannelKind, Clip, DrumParams, EffectInstance, EffectType, MixerTrack, Note, ParamValue, Pattern,
  PlaylistTrack, SamplerParams, SynthParams,
} from './types';
import { MASTER_ID, TICKS_PER_STEP } from './types';
import { getProject, update } from './store';
import { defaultDrum, defaultSampler, DRUM_LABELS, makeChannel, makeEffect, makeMixerTrack, makePattern, makeTrack, SYNTH_PRESETS } from './defaults';
import { clamp, paletteColor, uid } from './utils';
import { wouldCycle } from './routing';
import type { DrumType } from './types';

// ---------- project ----------
export const setBpm = (bpm: number) => update('Tempo', (d) => { d.bpm = clamp(Math.round(bpm * 1000) / 1000, 20, 400); });
export const setTimeSig = (num: number, den: number) => update('Time signature', (d) => { d.timeSig = [clamp(num, 1, 16), den]; });
export const setSwing = (v: number) => update('Swing', (d) => { d.swing = clamp(v, 0, 1); });
export const renameProject = (name: string) => update('Rename project', (d) => { d.name = name.trim() || 'Untitled'; });
export const setKeyScale = (key: number, scale: string) => update('Key/scale', (d) => { d.key = key; d.scale = scale; });

// ---------- channels ----------
function freeMixerTrack(): string {
  const p = getProject();
  const used = new Set(p.channels.map((c) => c.mixerTrackId));
  const free = p.mixer.find((m) => m.id !== MASTER_ID && !used.has(m.id) && m.inserts.length === 0 && m.sends.length === 0 && /^Insert \d+$/.test(m.name));
  return free?.id ?? MASTER_ID;
}

export interface AddChannelOpts {
  name?: string;
  drum?: DrumType;
  synthPreset?: string;
  assetId?: string;
  rootNote?: number;
}

export function addChannel(kind: ChannelKind, opts: AddChannelOpts = {}): string {
  const p = getProject();
  const mixerId = freeMixerTrack();
  const extra: Partial<Channel> = {};
  let name = opts.name ?? 'Goofy Synth';
  if (kind === 'drum') {
    const type = opts.drum ?? 'kick';
    extra.drum = defaultDrum(type);
    name = opts.name ?? DRUM_LABELS[type];
  } else if (kind === 'synth') {
    const preset = SYNTH_PRESETS.find((x) => x.name === opts.synthPreset);
    if (preset) {
      extra.synth = structuredClone(preset.params);
      name = opts.name ?? preset.name;
    }
  } else if (kind === 'sampler') {
    extra.sampler = { ...defaultSampler(opts.assetId ?? null), rootNote: opts.rootNote ?? 60 };
    name = opts.name ?? 'Sampler';
  }
  const ch = makeChannel(kind, name, mixerId, p.channels.length, extra);
  update(`Add ${name}`, (d) => {
    d.channels.push(ch);
    const mx = d.mixer.find((m) => m.id === mixerId);
    if (mx && mx.id !== MASTER_ID) {
      mx.name = name;
      mx.color = ch.color;
    }
  });
  return ch.id;
}

export function removeChannel(id: string) {
  update('Delete channel', (d) => {
    d.channels = d.channels.filter((c) => c.id !== id);
    for (const pt of d.patterns) {
      delete pt.steps[id];
      delete pt.notes[id];
    }
  });
}

export function duplicateChannel(id: string): string | null {
  const src = getProject().channels.find((c) => c.id === id);
  if (!src) return null;
  const copy: Channel = { ...structuredClone(src), id: uid('ch'), name: `${src.name} #2`, solo: false };
  update('Duplicate channel', (d) => {
    const i = d.channels.findIndex((c) => c.id === id);
    d.channels.splice(i + 1, 0, copy);
    for (const pt of d.patterns) {
      if (pt.steps[id]) pt.steps[copy.id] = [...pt.steps[id]];
    }
  });
  return copy.id;
}

export function moveChannel(id: string, dir: -1 | 1) {
  update('Move channel', (d) => {
    const i = d.channels.findIndex((c) => c.id === id);
    const j = i + dir;
    if (i < 0 || j < 0 || j >= d.channels.length) return;
    const [c] = d.channels.splice(i, 1);
    d.channels.splice(j, 0, c);
  });
}

export function updateChannel(id: string, patch: Partial<Channel>, label = 'Edit channel') {
  update(label, (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c) Object.assign(c, patch);
  });
}

export function updateSynth(id: string, patch: Partial<SynthParams>) {
  update('Synth parameter', (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c?.synth) Object.assign(c.synth, patch);
  });
}

export function updateDrum(id: string, patch: Partial<DrumParams>) {
  update('Drum parameter', (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c?.drum) Object.assign(c.drum, patch);
  });
}

export function updateSampler(id: string, patch: Partial<SamplerParams>) {
  update('Sampler parameter', (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c?.sampler) Object.assign(c.sampler, patch);
  });
}

export function toggleChannelMute(id: string) {
  update('Mute channel', (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c) c.mute = !c.mute;
  });
}

export function toggleChannelSolo(id: string) {
  update('Solo channel', (d) => {
    const c = d.channels.find((x) => x.id === id);
    if (c) c.solo = !c.solo;
  });
}

// ---------- patterns ----------
export function addPattern(): string {
  const p = getProject();
  const n = p.patterns.length + 1;
  const pt = makePattern(n);
  update('Add pattern', (d) => { d.patterns.push(pt); });
  return pt.id;
}

export function clonePattern(id: string): string | null {
  const src = getProject().patterns.find((p) => p.id === id);
  if (!src) return null;
  const copy: Pattern = { ...structuredClone(src), id: uid('pt'), name: `${src.name} (copy)`, color: paletteColor(getProject().patterns.length * 3) };
  for (const k of Object.keys(copy.notes)) copy.notes[k] = copy.notes[k].map((n) => ({ ...n, id: uid('n') }));
  update('Clone pattern', (d) => { d.patterns.push(copy); });
  return copy.id;
}

export function removePattern(id: string) {
  update('Delete pattern', (d) => {
    if (d.patterns.length <= 1) return;
    d.patterns = d.patterns.filter((p) => p.id !== id);
    d.clips = d.clips.filter((c) => c.patternId !== id);
  });
}

export function updatePattern(id: string, patch: Partial<Pick<Pattern, 'name' | 'color' | 'lengthSteps'>>) {
  update('Edit pattern', (d) => {
    const p = d.patterns.find((x) => x.id === id);
    if (p) Object.assign(p, patch);
  });
}

export function setStep(patternId: string, channelId: string, index: number, velocity: number) {
  update('Edit steps', (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    if (!p) return;
    const arr = p.steps[channelId] ?? [];
    while (arr.length < p.lengthSteps) arr.push(0);
    arr[index] = velocity;
    p.steps[channelId] = arr;
  });
}

export function fillSteps(patternId: string, channelId: string, every: number) {
  update('Fill steps', (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    if (!p) return;
    p.steps[channelId] = Array.from({ length: p.lengthSteps }, (_, i) => (every > 0 && i % every === 0 ? 1 : 0));
  });
}

export function shiftSteps(patternId: string, channelId: string, dir: number) {
  update('Shift steps', (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    const arr = p?.steps[channelId];
    if (!p || !arr) return;
    const n = p.lengthSteps;
    const src = Array.from({ length: n }, (_, i) => arr[i] ?? 0);
    p.steps[channelId] = src.map((_, i) => src[(i - dir + n) % n]);
  });
}

// ---------- notes ----------
export function addNote(patternId: string, channelId: string, note: Omit<Note, 'id'>): string {
  const id = uid('n');
  update('Add note', (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    if (!p) return;
    (p.notes[channelId] ??= []).push({ ...note, id });
  });
  return id;
}

export function updateNotes(patternId: string, channelId: string, fn: (n: Note) => void, ids?: Set<string>, opts: { history?: boolean } = {}) {
  update('Edit notes', (d) => {
    const list = d.patterns.find((x) => x.id === patternId)?.notes[channelId];
    if (!list) return;
    for (const n of list) if (!ids || ids.has(n.id)) fn(n);
  }, opts);
}

export function setNotes(patternId: string, channelId: string, notes: Note[], label = 'Edit notes') {
  update(label, (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    if (p) p.notes[channelId] = notes;
  });
}

export function deleteNotes(patternId: string, channelId: string, ids: Set<string>) {
  update('Delete notes', (d) => {
    const p = d.patterns.find((x) => x.id === patternId);
    if (p?.notes[channelId]) p.notes[channelId] = p.notes[channelId].filter((n) => !ids.has(n.id));
  });
}

export function quantizeNotes(patternId: string, channelId: string, grid: number, ids?: Set<string>) {
  updateNotes(patternId, channelId, (n) => {
    n.start = Math.round(n.start / grid) * grid;
    n.length = Math.max(grid, Math.round(n.length / grid) * grid);
  }, ids);
}

// ---------- playlist ----------
export function addClip(clip: Omit<Clip, 'id'>): string {
  const id = uid('c');
  update('Add clip', (d) => { d.clips.push({ ...clip, id }); });
  return id;
}

export function updateClips(ids: Set<string>, fn: (c: Clip) => void, label = 'Edit clips') {
  update(label, (d) => {
    for (const c of d.clips) if (ids.has(c.id)) fn(c);
  });
}

export function deleteClips(ids: Set<string>) {
  update('Delete clips', (d) => { d.clips = d.clips.filter((c) => !ids.has(c.id)); });
}

export function duplicateClips(ids: Set<string>): string[] {
  const p = getProject();
  const sel = p.clips.filter((c) => ids.has(c.id));
  if (!sel.length) return [];
  const start = Math.min(...sel.map((c) => c.start));
  const end = Math.max(...sel.map((c) => c.start + c.length));
  const copies = sel.map((c) => ({ ...c, id: uid('c'), start: c.start + (end - start) }));
  update('Duplicate clips', (d) => { d.clips.push(...copies); });
  return copies.map((c) => c.id);
}

export function sliceClip(id: string, at: number) {
  const p = getProject();
  const c = p.clips.find((x) => x.id === id);
  if (!c || at <= c.start || at >= c.start + c.length) return;
  const leftLen = at - c.start;
  const right: Clip = {
    ...c, id: uid('c'), start: at, length: c.length - leftLen,
    offset: c.kind === 'audio' ? c.offset + (leftLen / 96) * (60 / p.bpm) : c.offset + leftLen,
  };
  update('Slice clip', (d) => {
    const x = d.clips.find((k) => k.id === id);
    if (x) x.length = leftLen;
    d.clips.push(right);
  });
}

export function addTrack(): string {
  const t = makeTrack(getProject().tracks.length + 1);
  update('Add track', (d) => { d.tracks.push(t); });
  return t.id;
}

export function updateTrack(id: string, patch: Partial<PlaylistTrack>, label = 'Edit track') {
  update(label, (d) => {
    const t = d.tracks.find((x) => x.id === id);
    if (t) Object.assign(t, patch);
  });
}

export function removeTrack(id: string) {
  update('Delete track', (d) => {
    if (d.tracks.length <= 1) return;
    d.tracks = d.tracks.filter((t) => t.id !== id);
    d.clips = d.clips.filter((c) => c.trackId !== id);
  });
}

export function setLoop(patch: Partial<{ enabled: boolean; start: number; end: number }>) {
  update('Loop region', (d) => { Object.assign(d.loop, patch); });
}

export function addAsset(meta: AssetMeta) {
  update('Add audio', (d) => { d.assets[meta.id] = meta; });
}

// ---------- mixer ----------
export function updateMixerTrack(id: string, patch: Partial<MixerTrack>, label = 'Mixer') {
  update(label, (d) => {
    const t = d.mixer.find((x) => x.id === id);
    if (t) Object.assign(t, patch);
  });
}

export function toggleMixerMute(id: string) {
  update('Mute insert', (d) => {
    const t = d.mixer.find((x) => x.id === id);
    if (t) t.mute = !t.mute;
  });
}

export function toggleMixerSolo(id: string, exclusive = false) {
  update('Solo insert', (d) => {
    const t = d.mixer.find((x) => x.id === id);
    if (!t || t.id === MASTER_ID) return;
    const next = !t.solo;
    if (exclusive) for (const m of d.mixer) m.solo = false;
    t.solo = next;
  });
}

export function addMixerTrack(): string {
  const t = makeMixerTrack(getProject().mixer.length);
  update('Add insert', (d) => { d.mixer.push(t); });
  return t.id;
}

export function removeMixerTrack(id: string) {
  if (id === MASTER_ID) return;
  update('Delete insert', (d) => {
    d.mixer = d.mixer.filter((m) => m.id !== id);
    for (const m of d.mixer) {
      if (m.output === id) m.output = MASTER_ID;
      m.sends = m.sends.filter((s) => s.target !== id);
    }
    for (const c of d.channels) if (c.mixerTrackId === id) c.mixerTrackId = MASTER_ID;
    for (const t of d.tracks) if (t.mixerTrackId === id) t.mixerTrackId = MASTER_ID;
  });
}

export function addInsert(trackId: string, type: EffectType, params?: Record<string, ParamValue>, index?: number): string {
  const fx: EffectInstance = makeEffect(type, params);
  update('Add effect', (d) => {
    const t = d.mixer.find((x) => x.id === trackId);
    if (!t) return;
    if (index === undefined || index > t.inserts.length) t.inserts.push(fx);
    else t.inserts.splice(index, 0, fx);
  });
  return fx.id;
}

export function removeInsert(trackId: string, effectId: string) {
  update('Remove effect', (d) => {
    const t = d.mixer.find((x) => x.id === trackId);
    if (t) t.inserts = t.inserts.filter((e) => e.id !== effectId);
  });
}

export function moveInsert(trackId: string, effectId: string, dir: -1 | 1) {
  update('Reorder effects', (d) => {
    const t = d.mixer.find((x) => x.id === trackId);
    if (!t) return;
    const i = t.inserts.findIndex((e) => e.id === effectId);
    const j = i + dir;
    if (i < 0 || j < 0 || j >= t.inserts.length) return;
    const [e] = t.inserts.splice(i, 1);
    t.inserts.splice(j, 0, e);
  });
}

export function setEffectParam(trackId: string, effectId: string, key: string, value: ParamValue) {
  update('Effect parameter', (d) => {
    const e = d.mixer.find((x) => x.id === trackId)?.inserts.find((x) => x.id === effectId);
    if (e) e.params[key] = value;
  });
}

export function setEffectParams(trackId: string, effectId: string, params: Record<string, ParamValue>) {
  update('Effect preset', (d) => {
    const e = d.mixer.find((x) => x.id === trackId)?.inserts.find((x) => x.id === effectId);
    if (e) e.params = { ...e.params, ...params };
  });
}

export function toggleBypass(trackId: string, effectId: string) {
  update('Bypass effect', (d) => {
    const e = d.mixer.find((x) => x.id === trackId)?.inserts.find((x) => x.id === effectId);
    if (e) e.bypass = !e.bypass;
  });
}

export function addSend(trackId: string, target: string): boolean {
  const p = getProject();
  if (wouldCycle(p.mixer, trackId, target)) return false;
  update('Add send', (d) => {
    const t = d.mixer.find((x) => x.id === trackId);
    if (t && !t.sends.some((s) => s.target === target)) t.sends.push({ id: uid('s'), target, amount: 0.3 });
  });
  return true;
}

export function updateSend(trackId: string, sendId: string, amount: number) {
  update('Send level', (d) => {
    const s = d.mixer.find((x) => x.id === trackId)?.sends.find((x) => x.id === sendId);
    if (s) s.amount = clamp(amount, 0, 1);
  });
}

export function removeSend(trackId: string, sendId: string) {
  update('Remove send', (d) => {
    const t = d.mixer.find((x) => x.id === trackId);
    if (t) t.sends = t.sends.filter((s) => s.id !== sendId);
  });
}

export function setOutput(trackId: string, target: string): boolean {
  const p = getProject();
  const without = p.mixer.map((m) => (m.id === trackId ? { ...m, output: null } : m));
  if (target !== MASTER_ID && wouldCycle(without, trackId, target)) return false;
  updateMixerTrack(trackId, { output: target }, 'Routing');
  return true;
}

export const stepTicks = TICKS_PER_STEP;
