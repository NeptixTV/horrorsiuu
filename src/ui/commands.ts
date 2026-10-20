// User level commands shared by toolbar, menus and keyboard shortcuts.
import { engine } from '../audio/engine';
import { downloadBlob, encodeWav, normalizeBuffer, renderProject, renderStems } from '../audio/render';
import { makeZip, zipName } from '../project/zip';
import { getProject, redo, undo } from '../state/store';
import { setUI, toast, ui, openWindow } from '../state/ui';
import {
  addChannel, deleteClips, deleteNotes, duplicateClips, toggleChannelMute, toggleChannelSolo, toggleMixerMute, toggleMixerSolo,
} from '../state/actions';
import { safeFileName, saveProject } from '../project/manager';

export function syncTransportUI() {
  const t = engine.transport;
  if (!t) return;
  setUI({ playing: t.playing, recording: !!engine.recording, mode: t.mode });
}

export async function togglePlay() {
  await engine.resume();
  const t = engine.transport;
  if (t.playing) {
    if (engine.recording) await engine.stopRecording();
    t.pause();
  } else {
    t.patternId = ui().selectedPatternId;
    t.play();
  }
}

export async function stop() {
  if (engine.recording) await engine.stopRecording();
  engine.transport.stop();
}

export async function toggleRecord() {
  await engine.resume();
  if (engine.recording) {
    await engine.stopRecording();
    return;
  }
  const s = ui();
  const p = getProject();
  const trackId = s.selectedTrackId ?? p.tracks.find((t) => t.name.toLowerCase().includes('vocal'))?.id ?? p.tracks[0].id;
  engine.input.setGain(s.inputGain);
  const err = await engine.startRecording(trackId);
  if (err) toast(err, 'error');
  else toast('Recording… press R or Space to stop', 'info');
}

export function setMode(mode: 'pattern' | 'song') {
  engine.transport.patternId = ui().selectedPatternId;
  engine.transport.setMode(mode);
}

export function toggleMetronome() {
  const on = !ui().metronome;
  engine.transport.metronome = on;
  setUI({ metronome: on });
}

export function doUndo() {
  if (!undo()) toast('Nothing to undo');
}

export function doRedo() {
  if (!redo()) toast('Nothing to redo');
}

export async function doSave() {
  await saveProject();
}

export function selectAll() {
  const s = ui();
  const p = getProject();
  if (s.focus === 'piano') {
    const pat = p.patterns.find((x) => x.id === s.selectedPatternId);
    const notes = pat && s.pianoChannelId ? pat.notes[s.pianoChannelId] ?? [] : [];
    setUI({ selectedNoteIds: notes.map((n) => n.id) });
  } else {
    setUI({ selectedClipIds: p.clips.map((c) => c.id), focus: 'playlist' });
  }
}

export function deleteSelected() {
  const s = ui();
  if (s.focus === 'piano' && s.selectedNoteIds.length && s.selectedPatternId && s.pianoChannelId) {
    deleteNotes(s.selectedPatternId, s.pianoChannelId, new Set(s.selectedNoteIds));
    setUI({ selectedNoteIds: [] });
  } else if (s.selectedClipIds.length) {
    deleteClips(new Set(s.selectedClipIds));
    setUI({ selectedClipIds: [] });
  }
}

export function duplicateSelected() {
  const s = ui();
  if (s.selectedClipIds.length) setUI({ selectedClipIds: duplicateClips(new Set(s.selectedClipIds)) });
}

export function muteSelected() {
  const s = ui();
  if (s.focus === 'mixer') toggleMixerMute(s.selectedMixerId);
  else if (s.selectedChannelId) toggleChannelMute(s.selectedChannelId);
}

export function soloSelected() {
  const s = ui();
  if (s.focus === 'mixer') toggleMixerSolo(s.selectedMixerId);
  else if (s.selectedChannelId) toggleChannelSolo(s.selectedChannelId);
}

export interface ExportOptions {
  mode: 'song' | 'pattern';
  range?: 'auto' | 'song' | 'loop';
  bits?: 16 | 24 | 32;
  sampleRate?: number;
  normalize?: boolean;
  tail?: number;
  onProgress?: (p: number) => void;
}

function exportOpts(o: ExportOptions) {
  const s = ui();
  return {
    mode: o.mode, range: o.range ?? 'auto', patternId: s.selectedPatternId, sampleRate: o.sampleRate ?? s.exportRate,
    tail: o.tail ?? s.exportTail, onProgress: o.onProgress,
  } as const;
}

/** Renders and downloads the mix as WAV. */
export async function exportWav(mode: 'song' | 'pattern', o: Partial<ExportOptions> = {}) {
  const p = getProject();
  const opts = { ...exportOpts({ mode, ...o }) };
  toast('Rendering mixdown…', 'info');
  try {
    const buf = await renderProject(p, opts);
    if (o.normalize ?? ui().exportNormalize) normalizeBuffer(buf, -1);
    const name = safeFileName(mode === 'pattern' ? `${p.name} - ${p.patterns.find((x) => x.id === ui().selectedPatternId)?.name ?? 'pattern'}` : p.name, 'mixdown');
    downloadBlob(encodeWav(buf, o.bits ?? ui().exportBits), `${name}.wav`);
    toast(`Exported ${buf.duration.toFixed(1)} s WAV`, 'ok');
  } catch (err) {
    console.error(err);
    toast(`Export failed${err instanceof Error ? `: ${err.message}` : ''}`, 'error');
  }
}

/** Renders every used mixer insert separately and downloads them as one ZIP. */
export async function exportStems(o: Partial<ExportOptions> = {}) {
  const p = getProject();
  toast('Rendering stems…', 'info');
  try {
    const stems = await renderStems(p, exportOpts({ mode: 'song', ...o }));
    const used = new Set<string>();
    const bits = o.bits ?? ui().exportBits;
    const entries = await Promise.all(stems.map(async (st, i) => {
      if (o.normalize ?? ui().exportNormalize) normalizeBuffer(st.buffer, -1);
      const data = new Uint8Array(await encodeWav(st.buffer, bits).arrayBuffer());
      return { name: zipName(`${String(i).padStart(2, '0')} ${st.name}`, used, '.wav'), data };
    }));
    downloadBlob(makeZip(entries), `${safeFileName(p.name, 'project')} - stems.zip`);
    toast(`Exported ${stems.length} stems`, 'ok');
  } catch (err) {
    console.error(err);
    toast(`Stem export failed${err instanceof Error ? `: ${err.message}` : ''}`, 'error');
  }
}

export function showDock(tab: ReturnType<typeof ui>['dockTab']) {
  setUI({ dockTab: tab, dockOpen: true });
}

export const openSettings = () => openWindow({ kind: 'settings' });

/** Asks before deleting something that has content (Settings → General → Ask before deleting). */
export function confirmDelete(what: string, hasContent: boolean): boolean {
  if (!ui().confirmDeletes || !hasContent) return true;
  return confirm(`Delete ${what}? It still contains music. (You can undo with Ctrl+Z.)`);
}

export const channelHasContent = (id: string) => getProject().patterns.some((p) => (p.notes[id]?.length ?? 0) > 0 || (p.steps[id] ?? []).some((v) => v > 0));
export const patternHasContent = (id: string) => {
  const p = getProject();
  const pt = p.patterns.find((x) => x.id === id);
  return !!pt && (Object.values(pt.notes).some((l) => l.length > 0) || Object.values(pt.steps).some((l) => l.some((v) => v > 0)) || p.clips.some((c) => c.patternId === id));
};

/** Opens the right editor for a channel (DreamChord has its own device window). */
export function openChannelEditor(channelId: string) {
  const ch = getProject().channels.find((c) => c.id === channelId);
  if (!ch) return;
  openWindow(ch.kind === 'dream' ? { kind: 'dreamchord', channelId } : { kind: 'channel', channelId });
}

/** Adds a DreamChord channel (optionally with a preset) and opens it. */
export function addDreamChord(preset?: string) {
  const id = addChannel('dream', { dreamPreset: preset });
  setUI({ selectedChannelId: id, pianoChannelId: id });
  openWindow({ kind: 'dreamchord', channelId: id });
  return id;
}
