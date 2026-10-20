// Project lifecycle: new / open / save / save as / autosave / backups /
// import / export, plus audio asset persistence and audio file import.
import type { Project } from '../state/types';
import { getProject, loadProjectState, markSaved, useProject } from '../state/store';
import { createEmptyProject } from '../state/defaults';
import { normalizeProject } from '../state/normalize';
import { addAsset, addClip, ensureTrackInsert, updateTrack } from '../state/actions';
import { setUI, toast, ui } from '../state/ui';
import { secondsToTicks, uid } from '../state/utils';
import { assets } from '../audio/assets';
import { diagnostics } from '../audio/diagnostics';
import { recordToBuffer, storage, type BackupRecord } from './storage';
import { engine } from '../audio/engine';
import { downloadBlob } from '../audio/render';
import { isLibrarySample } from '../ui/library/libraryStore';

const LAST_KEY = 'goofy-studio-last-project';

function rememberLast(id: string) {
  try { localStorage.setItem(LAST_KEY, id); } catch { /* ignore */ }
}

/** Unsaved (in-memory only) assets that need to be written on save. */
const pendingAssets = new Set<string>();

export function markAssetPending(id: string) {
  pendingAssets.add(id);
}

/** Every audio asset a project needs (clips, samplers, registered assets). */
export function projectAssetIds(p: Project): Set<string> {
  const ids = new Set<string>(Object.keys(p.assets ?? {}));
  for (const c of p.clips) if (c.assetId) ids.add(c.assetId);
  for (const ch of p.channels) if (ch.sampler?.assetId) ids.add(ch.sampler.assetId);
  for (const id of [...ids]) if (id.startsWith('builtin:')) ids.delete(id);
  return ids;
}

async function persistAssets(project: Project) {
  for (const id of projectAssetIds(project)) {
    if (!pendingAssets.has(id) && (await storage.hasAsset(id))) continue;
    const buf = assets.get(id);
    if (buf) {
      await storage.saveAsset(id, project.assets[id]?.name ?? assets.name(id), buf, isLibrarySample(id));
      pendingAssets.delete(id);
    }
  }
}

async function loadAssets(project: Project) {
  for (const id of projectAssetIds(project)) {
    if (assets.has(id)) continue;
    const rec = await storage.loadAsset(id).catch(() => null);
    if (!rec) continue;
    try {
      assets.add(id, recordToBuffer(rec), rec.name);
    } catch (err) {
      diagnostics.log('error', 'asset', `Audio “${rec.name}” could not be loaded: ${err instanceof Error ? err.message : String(err)}`);
    }
  }
}

/**
 * Frees decoded audio that the open project no longer needs (previous
 * projects, deleted takes). Long sessions with many opened songs used to keep
 * every decoded file in memory, which caused heavy garbage collection pauses
 * – and those pauses made playback stutter.
 */
export function releaseUnusedAssets(keep: Project) {
  const needed = projectAssetIds(keep);
  let freed = 0;
  for (const id of assets.ids()) {
    if (id.startsWith('builtin:') || needed.has(id) || isLibrarySample(id)) continue;
    const b = assets.get(id);
    freed += b ? b.length * b.numberOfChannels * 4 : 0;
    assets.remove(id);
    pendingAssets.delete(id);
  }
  if (freed > 0) diagnostics.log('info', 'memory', `Released ${(freed / 1e6).toFixed(1)} MB of audio from closed projects`);
}

/** Repairs a loaded project and tells the user (and the log) what was fixed. */
function repair(raw: unknown, source: string): Project {
  const report = normalizeProject(raw);
  if (report.count > 0) {
    diagnostics.count('repairedProjects');
    diagnostics.log('warn', 'repair', `${source}: ${report.fixes.join(', ')}`);
    toast(`Project repaired: ${report.fixes.slice(0, 2).join(', ')}${report.fixes.length > 2 ? ' …' : ''}`, 'info', true);
  }
  return report.project;
}

const lastBackup = new Map<string, number>();

async function maybeBackup(project: Project, reason: BackupRecord['reason'], force = false) {
  const s = ui();
  if (!s.backups && !force) return;
  const last = lastBackup.get(project.id) ?? 0;
  if (!force && Date.now() - last < s.backupInterval * 60000) return;
  lastBackup.set(project.id, Date.now());
  try {
    await storage.saveBackup({ id: uid('bk'), projectId: project.id, name: project.name, time: Date.now(), reason, data: project }, s.backupCount);
  } catch (err) {
    diagnostics.log('warn', 'backup', `Backup failed: ${err instanceof Error ? err.message : String(err)}`);
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
    void maybeBackup(project, opts.autosave ? 'auto' : 'save');
    return true;
  } catch (err) {
    console.error(err);
    diagnostics.log('error', 'save', `Saving failed: ${err instanceof Error ? err.message : String(err)}`);
    toast(opts.autosave ? 'Autosave failed – is the disk full?' : 'Saving failed – storage unavailable or full?', 'error');
    return false;
  }
}

export async function saveProjectAs(name: string): Promise<boolean> {
  const p = getProject();
  const copy: Project = { ...p, id: uid('prj'), name: name.trim() || `${p.name} copy`, createdAt: Date.now(), updatedAt: Date.now() };
  for (const id of projectAssetIds(copy)) pendingAssets.add(id);
  loadProjectState(copy, { dirty: true });
  return saveProject();
}

async function activate(raw: unknown, source: string, opts: { dirty?: boolean } = {}): Promise<Project> {
  engine.transport?.stop();
  const project = repair(raw, source);
  await loadAssets(project);
  loadProjectState(project, opts);
  releaseUnusedAssets(project);
  resetSelection();
  return project;
}

export async function openProject(id: string): Promise<boolean> {
  try {
    const p = await storage.loadProject(id);
    if (!p) {
      toast('Project not found', 'error');
      return false;
    }
    const project = await activate(p, `Opened “${p.name}”`);
    rememberLast(project.id);
    toast(`Opened “${project.name}”`, 'ok');
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
  releaseUnusedAssets(p);
  resetSelection();
  toast('New project', 'info');
}

function resetSelection() {
  const p = getProject();
  setUI({
    selectedChannelId: p.channels[0]?.id ?? null,
    selectedPatternId: p.patterns[0]?.id ?? null,
    pianoChannelId: p.channels.find((c) => c.kind === 'synth' || c.kind === 'dream')?.id ?? p.channels[0]?.id ?? null,
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
  if (!id || !ui().openLastProject) {
    resetSelection();
    return false;
  }
  try {
    const p = await storage.loadProject(id);
    if (!p) {
      resetSelection();
      return false;
    }
    await activate(p, `Restored “${p.name}”`);
    return true;
  } catch (err) {
    diagnostics.log('error', 'restore', `Last project could not be restored: ${err instanceof Error ? err.message : String(err)}`);
    resetSelection();
    return false;
  }
}

// ---------- autosave ----------

let autosaveTimer: ReturnType<typeof setInterval> | null = null;
let deferredSince = 0;
let stopWatch: (() => void) | null = null;

function autosaveTick() {
  const s = useProject.getState();
  if (!ui().autosave || !s.dirty || s.gestureBase || engine.recording) return;
  // Saving copies a lot of data on the UI thread; while the song plays that can
  // delay the scheduler. Wait for the stop (but never longer than 10 minutes).
  if (engine.transport?.playing) {
    if (!deferredSince) deferredSince = Date.now();
    if (Date.now() - deferredSince < 10 * 60000) return;
  }
  deferredSince = 0;
  void saveProject({ autosave: true });
}

export function startAutosave() {
  if (autosaveTimer) clearInterval(autosaveTimer);
  const secs = Math.max(15, Math.min(1800, ui().autosaveInterval || 45));
  autosaveTimer = setInterval(autosaveTick, secs * 1000);
  stopWatch?.();
  stopWatch = engine.onChange(() => {
    if (deferredSince && !engine.transport?.playing) setTimeout(autosaveTick, 500);
  });
}

// ---------- backups ----------

export async function listBackups() {
  return storage.listBackups(getProject().id);
}

export async function backupNow() {
  await maybeBackup({ ...getProject(), updatedAt: Date.now() }, 'save', true);
  toast('Backup created', 'ok');
}

/** Replaces the open project with a backup (the current state is backed up first). */
export async function restoreBackup(id: string): Promise<boolean> {
  const b = await storage.loadBackup(id);
  if (!b) {
    toast('Backup not found', 'error');
    return false;
  }
  await maybeBackup({ ...getProject(), updatedAt: Date.now() }, 'before-restore', true);
  await activate({ ...b.data, id: b.projectId }, 'Backup', { dirty: true });
  toast(`Restored backup from ${new Date(b.time).toLocaleString()} – save to keep it`, 'ok', true);
  return true;
}

// ---------- storage housekeeping ----------

export interface StorageInfo {
  usage: number;
  quota: number;
  persisted: boolean;
  projects: number;
  audioFiles: number;
  audioBytes: number;
  orphanFiles: number;
  orphanBytes: number;
  backups: number;
}

async function referencedAssets(): Promise<Set<string>> {
  const used = new Set<string>();
  const add = (p: Project) => { for (const id of projectAssetIds(p)) used.add(id); };
  for (const p of await storage.allProjects().catch(() => [])) add(p);
  for (const p of await storage.allBackupProjects().catch(() => [])) add(p);
  add(getProject());
  return used;
}

export async function storageInfo(): Promise<StorageInfo> {
  const est = await navigator.storage?.estimate?.().catch(() => undefined);
  const persisted = (await navigator.storage?.persisted?.().catch(() => false)) ?? false;
  const meta = await storage.listAssetMeta().catch(() => []);
  const used = await referencedAssets();
  const orphans = meta.filter((m) => !m.library && !used.has(m.id) && !pendingAssets.has(m.id));
  return {
    usage: est?.usage ?? 0,
    quota: est?.quota ?? 0,
    persisted,
    projects: (await storage.listProjects().catch(() => [])).length,
    audioFiles: meta.length,
    audioBytes: meta.reduce((n, m) => n + m.bytes, 0),
    orphanFiles: orphans.length,
    orphanBytes: orphans.reduce((n, m) => n + m.bytes, 0),
    backups: (await storage.listBackups().catch(() => [])).length,
  };
}

/** Deletes stored audio that no project, backup or the library uses any more. */
export async function cleanupStorage(): Promise<number> {
  const meta = await storage.listAssetMeta();
  const used = await referencedAssets();
  let freed = 0;
  for (const m of meta) {
    if (m.library || used.has(m.id) || pendingAssets.has(m.id)) continue;
    await storage.deleteAsset(m.id);
    freed += m.bytes;
  }
  return freed;
}

/** Asks the browser not to evict the app's data when disk space runs low. */
export async function requestPersistentStorage(): Promise<boolean> {
  try {
    return (await navigator.storage?.persist?.()) ?? false;
  } catch {
    return false;
  }
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
  const usable = bin.length - (bin.length % 4);
  const bytes = new Uint8Array(usable);
  for (let i = 0; i < usable; i++) bytes[i] = bin.charCodeAt(i);
  return new Float32Array(bytes.buffer);
}

export function exportProjectFile() {
  const p = getProject();
  const audio: Record<string, { sampleRate: number; channels: string[] }> = {};
  for (const id of projectAssetIds(p)) {
    const b = assets.get(id);
    if (b) audio[id] = { sampleRate: b.sampleRate, channels: Array.from({ length: b.numberOfChannels }, (_, c) => b64(b.getChannelData(c))) };
  }
  const blob = new Blob([JSON.stringify({ format: 'goofy-studio-project', version: 1, project: p, audio })], { type: 'application/json' });
  downloadBlob(blob, `${safeFileName(p.name, 'project')}.goofy.json`);
}

/** File names from project names: no path characters, no reserved names, short. */
export function safeFileName(name: string, fallback: string): string {
  const cleaned = name.replace(/[^\w\- ]+/g, '_').replace(/^[.\s]+|[.\s]+$/g, '').slice(0, 80);
  return /^(con|prn|aux|nul|com\d|lpt\d)$/i.test(cleaned) || !cleaned ? fallback : cleaned;
}

const MAX_IMPORT_BYTES = 600 * 1024 * 1024;

export async function importProjectFile(file: File): Promise<boolean> {
  try {
    if (file.size > MAX_IMPORT_BYTES) throw new Error('The file is too large (max. 600 MB)');
    let json: unknown;
    try {
      json = JSON.parse(await file.text());
    } catch {
      throw new Error('This is not a Goofy Studio project file');
    }
    const j = json as { format?: unknown; project?: unknown; audio?: unknown };
    if (!j || j.format !== 'goofy-studio-project' || !j.project) throw new Error('Not a Goofy Studio project');
    const audio = j.audio && typeof j.audio === 'object' ? (j.audio as Record<string, unknown>) : {};
    const decoded: [string, AudioBuffer][] = [];
    for (const [id, a] of Object.entries(audio)) {
      if (!/^[\w:-]{1,120}$/.test(id) || id.startsWith('builtin:')) continue;
      const entry = a as { sampleRate?: unknown; channels?: unknown };
      const sr = typeof entry.sampleRate === 'number' && entry.sampleRate >= 3000 && entry.sampleRate <= 768000 ? entry.sampleRate : 0;
      if (!sr || !Array.isArray(entry.channels) || entry.channels.length < 1 || entry.channels.length > 32) {
        diagnostics.log('warn', 'import', `Skipped damaged audio “${id}”`);
        continue;
      }
      try {
        const chans = (entry.channels as unknown[]).map((c) => (typeof c === 'string' ? unb64(c) : new Float32Array(0)));
        const len = Math.max(...chans.map((c) => c.length));
        if (!(len > 0)) continue;
        const buf = new AudioBuffer({ length: len, numberOfChannels: chans.length, sampleRate: sr });
        chans.forEach((d, c) => {
          const copy = new Float32Array(len);
          copy.set(d.subarray(0, len));
          for (let i = 0; i < len; i++) if (!Number.isFinite(copy[i])) copy[i] = 0;
          buf.copyToChannel(copy, c);
        });
        decoded.push([id, buf]);
      } catch {
        diagnostics.log('warn', 'import', `Skipped damaged audio “${id}”`);
      }
    }
    const raw = j.project as Record<string, unknown>;
    const names = (raw.assets && typeof raw.assets === 'object' ? raw.assets : {}) as Record<string, { name?: string }>;
    for (const [id, buf] of decoded) {
      assets.add(id, buf, typeof names[id]?.name === 'string' ? names[id].name : 'Audio');
      pendingAssets.add(id);
    }
    const project = await activate(raw, `Imported “${file.name}”`, { dirty: true });
    toast(`Imported “${project.name}”`, 'ok');
    return true;
  } catch (err) {
    toast(`Import failed: ${err instanceof Error ? err.message : err}`, 'error');
    return false;
  }
}

// ---------- audio import / recording ----------

/** Largest audio file accepted for decoding (decoded audio needs ~10× the file size in memory). */
const MAX_AUDIO_BYTES = 400 * 1024 * 1024;

export async function decodeAudioFile(file: File): Promise<{ id: string; buffer: AudioBuffer; name: string } | null> {
  try {
    if (file.size > MAX_AUDIO_BYTES) {
      toast(`“${file.name}” is too large (max. 400 MB)`, 'error');
      return null;
    }
    const data = await file.arrayBuffer();
    const buffer = await engine.ctx.decodeAudioData(data);
    const id = uid('au');
    const name = file.name.replace(/\.[^.]+$/, '').slice(0, 120) || 'Audio';
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
  ensureTrackInsert(trackId);
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
    if (track.mixerTrackId === 'master') {
      if (target !== 'master') updateTrack(track.id, { mixerTrackId: target }, 'Route track');
      else ensureTrackInsert(track.id);
    }
    toast(`Recorded ${name} (${buffer.duration.toFixed(1)} s)`, 'ok');
  };
}
