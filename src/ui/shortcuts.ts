// Global keyboard shortcuts, typing-keyboard piano and Web MIDI input.
import { engine } from '../audio/engine';
import { TYPING_KEYS } from '../midi/music';
import { getProject, useProject } from '../state/store';
import { openWindow, setUI, ui } from '../state/ui';
import {
  deleteSelected, doRedo, doSave, doUndo, duplicateSelected, muteSelected, selectAll, setMode, showDock, soloSelected, togglePlay, toggleRecord,
} from './commands';
import { newProject, saveProjectAs } from '../project/manager';
import { askText } from './dialogs/askText';

const held = new Map<string, number>();
let octave = 0;

function liveChannel(): string | null {
  const s = ui();
  return s.focus === 'piano' ? s.pianoChannelId ?? s.selectedChannelId : s.selectedChannelId ?? s.pianoChannelId;
}

function isTextTarget(t: EventTarget | null) {
  if (!(t instanceof HTMLElement)) return false;
  return t.tagName === 'INPUT' || t.tagName === 'TEXTAREA' || t.tagName === 'SELECT' || t.isContentEditable;
}

function onKeyDown(e: KeyboardEvent) {
  if (isTextTarget(e.target)) return;
  const mod = e.ctrlKey || e.metaKey;
  const k = e.key.toLowerCase();

  if (mod) {
    if (k === 's') { e.preventDefault(); if (e.shiftKey) { void askText('Save project as', `${getProject().name} copy`).then((n) => { if (n) void saveProjectAs(n); }); } else void doSave(); return; }
    if (k === 'z') { e.preventDefault(); if (e.shiftKey) doRedo(); else doUndo(); return; }
    if (k === 'y') { e.preventDefault(); doRedo(); return; }
    if (k === 'a') { e.preventDefault(); selectAll(); return; }
    if (k === 'd') { e.preventDefault(); duplicateSelected(); return; }
    if (k === 'o') { e.preventDefault(); openWindow({ kind: 'projects' }); return; }
    if (k === 'n') { e.preventDefault(); if (!useProject.getState().dirty || confirm('Discard unsaved changes?')) newProject(); return; }
    return;
  }

  if (e.key === ' ') {
    e.preventDefault();
    // avoid a second "click" on the focused button when space is released
    if (document.activeElement instanceof HTMLButtonElement) document.activeElement.blur();
    void togglePlay();
    return;
  }
  if (e.key === 'Delete' || e.key === 'Backspace') { e.preventDefault(); deleteSelected(); return; }
  if (e.key === 'Escape') { setUI({ selectedClipIds: [], selectedNoteIds: [] }); return; }
  const fkeys: Record<string, () => void> = {
    F5: () => setUI({ focus: 'playlist' }),
    F6: () => showDock('rack'),
    F7: () => showDock('piano'),
    F9: () => showDock('mixer'),
    F10: () => showDock('drums'),
    F11: () => showDock('vocal'),
    F12: () => showDock('analyzer'),
  };
  if (fkeys[e.key]) { e.preventDefault(); fkeys[e.key](); return; }
  if (e.key === 'F8' && e.altKey) { e.preventDefault(); setUI({ browserOpen: !ui().browserOpen }); return; }

  // typing keyboard piano takes over letter keys
  if (ui().typingKeyboard && !e.altKey) {
    if (k === '-' || k === '+' || k === '=') { octave = Math.max(-3, Math.min(3, octave + (k === '-' ? -1 : 1))); return; }
    const semi = TYPING_KEYS[k];
    if (semi !== undefined) {
      e.preventDefault();
      if (e.repeat || held.has(k)) return;
      const ch = liveChannel();
      if (!ch) return;
      const pitch = 48 + octave * 12 + semi;
      held.set(k, pitch);
      engine.previewNote(ch, pitch, 0.85);
      return;
    }
  }
  if (e.repeat) return;
  if (k === 'r') { e.preventDefault(); void toggleRecord(); return; }
  if (k === 'm') { muteSelected(); return; }
  if (k === 's') { soloSelected(); return; }
  if (k === 'l') { setMode(ui().mode === 'pattern' ? 'song' : 'pattern'); return; }
}

function onKeyUp(e: KeyboardEvent) {
  const k = e.key.toLowerCase();
  const pitch = held.get(k);
  if (pitch !== undefined) {
    held.delete(k);
    const ch = liveChannel();
    if (ch) engine.previewNoteOff(ch, pitch);
  }
}

async function setupMidi() {
  const nav = navigator as Navigator & { requestMIDIAccess?: () => Promise<MIDIAccess> };
  if (!nav.requestMIDIAccess) return;
  try {
    const access = await nav.requestMIDIAccess();
    const bind = () => access.inputs.forEach((input) => {
      input.onmidimessage = (msg) => {
        const d = msg.data;
        if (!d || d.length < 3) return;
        const cmd = d[0] & 0xf0;
        const ch = liveChannel();
        if (!ch) return;
        if (cmd === 0x90 && d[2] > 0) engine.previewNote(ch, d[1], d[2] / 127);
        else if (cmd === 0x80 || (cmd === 0x90 && d[2] === 0)) engine.previewNoteOff(ch, d[1]);
      };
    });
    bind();
    access.onstatechange = bind;
  } catch {
    /* MIDI permission denied – ignore */
  }
}

export function installShortcuts() {
  window.addEventListener('keydown', onKeyDown);
  window.addEventListener('keyup', onKeyUp);
  window.addEventListener('blur', () => {
    const ch = liveChannel();
    for (const p of held.values()) if (ch) engine.previewNoteOff(ch, p);
    held.clear();
  });
  void setupMidi();
}
