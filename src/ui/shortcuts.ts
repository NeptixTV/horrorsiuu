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
import { liveRecorder } from '../midi/liveRecord';
import { capture } from '../midi/capture';
import { DreamSynth } from '../audio/instruments/dream';
import { diagnostics } from '../audio/diagnostics';

/** Live notes from the computer keyboard / MIDI: sound + recording + capture buffer. */
function liveNoteOn(ch: string, pitch: number, vel: number) {
  const t = engine.ctx.currentTime + 0.005;
  engine.previewNote(ch, pitch, vel, undefined, t);
  liveRecorder.noteOn(ch, pitch, vel, t);
  capture.noteOn(ch, pitch, vel, t);
}

function liveNoteOff(ch: string, pitch: number) {
  const t = engine.ctx.currentTime;
  engine.previewNoteOff(ch, pitch, t);
  liveRecorder.noteOff(ch, pitch, t);
  capture.noteOff(ch, pitch, t);
}

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

// typing 1-8-7 anywhere (outside text fields) opens the admin panel
let code = '';
let codeTime = 0;
function secretCode(e: KeyboardEvent) {
  if (!/^\d$/.test(e.key) || e.ctrlKey || e.metaKey || e.altKey) return false;
  const now = performance.now();
  code = (now - codeTime > 1500 ? '' : code) + e.key;
  codeTime = now;
  if (code.endsWith('187')) {
    code = '';
    openWindow({ kind: 'admin' });
    return true;
  }
  return false;
}

/** Instruments with their own keys (DreamChord) can take a key before the global shortcuts. */
type KeyOverride = (e: KeyboardEvent, down: boolean) => boolean;
const overrides = new Set<KeyOverride>();
export function registerKeyOverride(fn: KeyOverride): () => void {
  overrides.add(fn);
  return () => { overrides.delete(fn); };
}

function override(e: KeyboardEvent, down: boolean) {
  for (const fn of overrides) {
    if (fn(e, down)) {
      e.preventDefault();
      return true;
    }
  }
  return false;
}

function onKeyDown(e: KeyboardEvent) {
  if (isTextTarget(e.target)) return;
  if (override(e, true)) return;
  if (!e.repeat && secretCode(e)) { e.preventDefault(); return; }
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
      liveNoteOn(ch, pitch, 0.85);
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
  if (!isTextTarget(e.target) && override(e, false)) return;
  const k = e.key.toLowerCase();
  const pitch = held.get(k);
  if (pitch !== undefined) {
    held.delete(k);
    const ch = liveChannel();
    if (ch) liveNoteOff(ch, pitch);
  }
}

// ---------- MIDI keyboards ----------

export interface MidiDeviceInfo {
  id: string;
  name: string;
}

let midiAccess: MIDIAccess | null = null;
const midiListeners = new Set<() => void>();
const sustained = new Map<string, Set<number>>(); // channel → notes held by the pedal
const midiHeld = new Map<string, Set<number>>(); // channel → notes held by keys
let pedal = false;

export function midiDevices(): MidiDeviceInfo[] {
  const out: MidiDeviceInfo[] = [];
  midiAccess?.inputs.forEach((i) => out.push({ id: i.id, name: i.name ?? 'MIDI input' }));
  return out;
}

export function onMidiDevices(l: () => void): () => void {
  midiListeners.add(l);
  return () => { midiListeners.delete(l); };
}

function dreamOf(ch: string): DreamSynth | null {
  const inst = engine.initialized ? engine.graph.channels.get(ch)?.instrument : undefined;
  return inst instanceof DreamSynth ? inst : null;
}

function onMidi(input: MIDIInput, msg: MIDIMessageEvent) {
  const want = ui().midiInput;
  if (want === 'off' || (want && want !== input.id)) return;
  const d = msg.data;
  if (!d || d.length < 2) return;
  const cmd = d[0] & 0xf0;
  const ch = liveChannel();
  if (!ch) return;
  if (cmd === 0x90 && d.length > 2 && d[2] > 0) {
    (midiHeld.get(ch) ?? midiHeld.set(ch, new Set()).get(ch)!).add(d[1]);
    sustained.get(ch)?.delete(d[1]);
    liveNoteOn(ch, d[1], d[2] / 127);
  } else if (cmd === 0x80 || (cmd === 0x90 && d.length > 2 && d[2] === 0)) {
    midiHeld.get(ch)?.delete(d[1]);
    // sustain pedal: keep the note until the pedal goes up
    if (pedal) (sustained.get(ch) ?? sustained.set(ch, new Set()).get(ch)!).add(d[1]);
    else liveNoteOff(ch, d[1]);
  } else if (cmd === 0xb0 && d.length > 2) {
    if (d[1] === 64) {
      pedal = d[2] >= 64;
      if (!pedal) {
        for (const [c, set] of sustained) for (const n of set) if (!midiHeld.get(c)?.has(n)) liveNoteOff(c, n);
        sustained.clear();
      }
    } else if (d[1] === 1) {
      dreamOf(ch)?.setExpression(d[2] / 127); // mod wheel → vibrato
    } else if (d[1] === 123 || d[1] === 120) {
      engine.graph.allNotesOff(); // all notes off / all sound off
    }
  } else if (cmd === 0xe0 && d.length > 2) {
    const v = ((d[2] << 7) | d[1]) - 8192;
    dreamOf(ch)?.setBend((v / 8192) * 2);
  }
}

async function setupMidi() {
  const nav = navigator as Navigator & { requestMIDIAccess?: (o?: { sysex?: boolean }) => Promise<MIDIAccess> };
  if (!nav.requestMIDIAccess || ui().midiInput === 'off') return;
  try {
    const access = await nav.requestMIDIAccess({ sysex: false });
    midiAccess = access;
    const bind = () => {
      access.inputs.forEach((input) => { input.onmidimessage = (msg) => onMidi(input, msg); });
      midiListeners.forEach((l) => l());
      diagnostics.log('info', 'midi', `${access.inputs.size} MIDI input${access.inputs.size === 1 ? '' : 's'} available`);
    };
    bind();
    access.onstatechange = bind;
  } catch {
    /* MIDI permission denied – ignore */
  }
}

/** Re-checks MIDI after the setting changed (e.g. switched back on). */
export function refreshMidi() {
  if (!midiAccess) void setupMidi();
  midiListeners.forEach((l) => l());
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
