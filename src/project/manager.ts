// Project lifecycle: new / open / save / save as / autosave / import / export,
// plus audio asset persistence and audio file import.
import type { Project } from '../state/types';
import { PPQ } from '../state/types';
import { getProject, loadProjectState, markSaved, useProject } from '../state/store';
import { createEmptyProject } from '../state/defaults';
import { addAsset, addClip, updateTrack } from '../state/actions';
import { setUI, toast, ui } from '../state/ui';
import { secondsToTicks, uid } from '../state/utils';
import { assets } from '../audio/assets';
import { recordToBuffer, storage } from './storage';
import { engine } from '../audio/engine';
import { downloadBlob } from '../audio/render';

const LAST_KEY = 'goofy-studio-last-project';

function rememberLast(id: string) {
  try { localStorage.setItem(LAST_KEY, id); } catch { /* ignore */ }
}

/** Unsaved (in-memory only) assets that need to be written on save. */
const pendingAssets = new Set<string>();

export function markAssetPending(id: string) {
  pendingAssets.add(id);
}

async function persistAssets(project: Project) {
  for (const id of Object.keys(project.assets)) {
    if (id.startsWith('builtin:')) continue;
    if (!pendingAssets.has(id) && (await storage.hasAsset(id))) continue;
    const buf = assets.get(id);
    if (buf) {
      await storage.saveAsset(id, project.assets[id].name, buf);
      pendingAssets.delete(id);
    }
  }
}

async function loadAssets(project: Project) {
  for (const id of Object.keys(project.assets)) {
    if (assets.has(id)) continue;
    const rec = await storage.loadAsset(id).catch(() => null);
    if (rec) assets.add(id, recordToBuffer(rec), rec.name);
  }
}

export async function saveProject(opts: { autosave?: boolean } = {}): Promise<boolean> {
  try {
    const project = { ...getProject(), updatedAt: Date.now() };
    await persistAssets(project);
    await storage.saveProject(project);
    rememberLast(project.id);
    // keep updatedAt without creating an undo step
    useProject.setState((s) => ({ project: s.project.id === project.id ? { ...s.project, updatedAt: project.updatedAt } : s.project }));
    markSaved(opts.autosave);
    if (!opts.autosave) toast(`Saved “${project.name}”`, 'ok');
    return true;
  } catch (err) {
    console.error(err);
    if (!opts.autosave) toast('Saving failed – storage unavailable?', 'error');
    return false;
  }
}

export async function saveProjectAs(name: string): Promise<boolean> {
  const p = getProject();
  const copy: Project = { ...p, id: uid('prj'), name: name.trim() || `${p.name} copy`, createdAt: Date.now(), updatedAt: Date.now() };
  for (const id of Object.keys(copy.assets)) pendingAssets.add(id);
  loadProjectState(copy, { dirty: true });
  return saveProject();
}

export async function openProject(id: string): Promise<boolean> {
  try {
    const p = await storage.loadProject(id);
    if (!p) {
      toast('Project not found', 'error');
      return false;
    }
    engine.transport?.stop();
    await loadAssets(p);
    loadProjectState(migrate(p));
    rememberLast(p.id);
    resetSelection();
    toast(`Opened “${p.name}”`, 'ok');
    return true;
  } catch (err) {
    console.error(err);
    toast('Could not open project', 'error');
    return false;
  }
}

export function newProject() {
  engine.transport?.stop();
  const p = createEmptyProject();
  loadProjectState(p, { dirty: true });
  resetSelection();
  toast('New project', 'info');
}

function resetSelection() {
  const p = getProject();
  setUI({
    selectedChannelId: p.channels[0]?.id ?? null,
    selectedPatternId: p.patterns[0]?.id ?? null,
    pianoChannelId: p.channels.find((c) => c.kind === 'synth')?.id ?? p.channels[0]?.id ?? null,
    selectedClipIds: [],
    selectedNoteIds: [],
    selectedMixerId: 'master',
    selectedTrackId: (p.tracks.find((t) => t.name.toLowerCase().includes('vocal')) ?? p.tracks[0])?.id ?? null,
  });
  if (engine.transport) engine.transport.patternId = getProject().patterns[0]?.id ?? null;
}

/** Restores the last project on start-up. Returns false if none was found. */
export async function restoreLastProject(): Promise<boolean> {
  let id: string | null = null;
  try { id = localStorage.getItem(LAST_KEY); } catch { /* ignore */ }
  if (!id) {
    resetSelection();
    return false;
  }
  try {
    const p = await storage.loadProject(id);
    if (!p) {
      resetSelection();
      return false;
    }
    await loadAssets(p);
    loadProjectState(migrate(p));
    resetSelection();
    return true;
  } catch {
    resetSelection();
    return false;
  }
}

function migrate(p: Project): Project {
  return { ...p, loop: p.loop ?? { enabled: false, start: 0, end: PPQ * 16 }, assets: p.assets ?? {}, swing: p.swing ?? 0 };
}

let autosaveTimer: ReturnType<typeof setInterval> | null = null;
export function startAutosave() {
  if (autosaveTimer) clearInterval(autosaveTimer);
  autosaveTimer = setInterval(() => {
    const s = useProject.getState();
    if (ui().autosave && s.dirty && !s.gestureBase && !engine.recording) void saveProject({ autosave: true });
  }, 45000);
}

// ---------- file export / import ----------

function b64(arr: Float32Array): string {
  const bytes = new Uint8Array(arr.buffer, arr.byteOffset, arr.byteLength);
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(s);
}

function unb64(s: string): Float32Array {
  const bin = atob(s);
  const bytes = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
  return new Float32Array(bytes.buffer);
}

export function exportProjectFile() {
  const p = getProject();
  const audio: Record<string, { sampleRate: number; channels: string[] }> = {};
  for (const id of Object.keys(p.assets)) {
    if (id.startsWith('builtin:')) continue;
    const b = assets.get(id);
    if (b) audio[id] = { sampleRate: b.sampleRate, channels: Array.from({ length: b.numberOfChannels }, (_, c) => b64(b.getChannelData(c))) };
  }
  const blob = new Blob([JSON.stringify({ format: 'goofy-studio-project', version: 1, project: p, audio })], { type: 'application/json' });
  downloadBlob(blob, `${p.name.replace(/[^\w\- ]+/g, '_') || 'project'}.goofy.json`);
}

export async function importProjectFile(file: File): Promise<boolean> {
  try {
    const json = JSON.parse(await file.text());
    if (json.format !== 'goofy-studio-project' || !json.project) throw new Error('Not a Goofy Studio project');
    const project = migrate(json.project as Project);
    for (const [id, a] of Object.entries(json.audio ?? {}) as [string, { sampleRate: number; channels: string[] }][]) {
      const chans = a.channels.map(unb64);
      const buf = new AudioBuffer({ length: chans[0].length, numberOfChannels: chans.length, sampleRate: a.sampleRate });
      chans.forEach((d, c) => buf.copyToChannel(new Float32Array(d), c));
      assets.add(id, buf, project.assets[id]?.name);
      pendingAssets.add(id);
    }
    engine.transport?.stop();
    loadProjectState(project, { dirty: true });
    resetSelection();
    toast(`Imported “${project.name}”`, 'ok');
    return true;
  } catch (err) {
    toast(`Import failed: ${err instanceof Error ? err.message : err}`, 'error');
    return false;
  }
}

// ---------- audio import / recording ----------

export async function decodeAudioFile(file: File): Promise<{ id: string; buffer: AudioBuffer; name: string } | null> {
  try {
    const data = await file.arrayBuffer();
    const buffer = await engine.ctx.decodeAudioData(data);
    const id = uid('au');
    const name = file.name.replace(/\.[^.]+$/, '');
    assets.add(id, buffer, name);
    pendingAssets.add(id);
    return { id, buffer, name };
  } catch {
    toast(`Could not decode “${file.name}”`, 'error');
    return null;
  }
}

/** Registers an asset with the project (meta) – needed before clips can reference it. */
export function registerAsset(id: string, name: string) {
  const p = getProject();
  if (p.assets[id]) return;
  const b = assets.get(id);
  if (!b) return;
  addAsset({ id, name, duration: b.duration, sampleRate: b.sampleRate, channels: b.numberOfChannels, builtin: id.startsWith('builtin:') });
}

export function placeAudioClip(assetId: string, trackId: string, startTick: number) {
  const p = getProject();
  const b = assets.get(assetId);
  if (!b) return;
  registerAsset(assetId, assets.name(assetId));
  addClip({ trackId, kind: 'audio', start: Math.max(0, startTick), length: Math.max(1, Math.round(secondsToTicks(b.duration, p.bpm))), offset: 0, assetId });
}

export async function importLibrarySample(file: File) {
  const res = await decodeAudioFile(file);
  if (!res) return null;
  await storage.saveAsset(res.id, res.name, res.buffer, true).catch(() => undefined);
  pendingAssets.delete(res.id);
  return res;
}

let takeCounter = 1;
export function installRecordingHandler() {
  engine.onRecordingComplete = ({ buffer, startTick, trackId }) => {
    const id = uid('rec');
    const name = `Take ${takeCounter++}`;
    assets.add(id, buffer, name);
    pendingAssets.add(id);
    const p = getProject();
    const track = p.tracks.find((t) => t.id === trackId) ?? p.tracks[0];
    registerAsset(id, name);
    addClip({ trackId: track.id, kind: 'audio', start: Math.max(0, Math.round(startTick)), length: Math.max(1, Math.round(secondsToTicks(buffer.duration, p.bpm))), offset: 0, assetId: id });
    // route the vocal track to the monitored insert so the recording plays through the same effects
    const target = engine.monitorTarget(p);
    if (track.mixerTrackId === 'master' && target !== 'master') updateTrack(track.id, { mixerTrackId: target }, 'Route track');
    toast(`Recorded ${name} (${buffer.duration.toFixed(1)} s)`, 'ok');
  };
}
