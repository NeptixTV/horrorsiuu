// User level commands shared by toolbar, menus and keyboard shortcuts.
import { engine } from '../audio/engine';
import { downloadBlob, encodeWav, renderProject } from '../audio/render';
import { getProject, redo, undo } from '../state/store';
import { setUI, toast, ui, openWindow } from '../state/ui';
import {
  deleteClips, deleteNotes, duplicateClips, toggleChannelMute, toggleChannelSolo, toggleMixerMute, toggleMixerSolo,
} from '../state/actions';
import { saveProject } from '../project/manager';

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

export async function exportWav(mode: 'song' | 'pattern') {
  const p = getProject();
  toast('Rendering mixdown…', 'info');
  try {
    const buf = await renderProject(p, { mode, patternId: ui().selectedPatternId });
    downloadBlob(encodeWav(buf), `${p.name.replace(/[^\w\- ]+/g, '_') || 'mixdown'}.wav`);
    toast(`Exported ${buf.duration.toFixed(1)} s WAV`, 'ok');
  } catch (err) {
    console.error(err);
    toast('Export failed', 'error');
  }
}

export function showDock(tab: ReturnType<typeof ui>['dockTab']) {
  setUI({ dockTab: tab, dockOpen: true });
}

export const openSettings = () => openWindow({ kind: 'settings' });
