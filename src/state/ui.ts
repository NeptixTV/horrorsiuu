// Non-undoable UI state: selection, editors, layout, theme and transport flags.
import { create } from 'zustand';
import type { EffectType, Id } from './types';
import { PPQ } from './types';

export type DockTab = 'rack' | 'piano' | 'mixer' | 'drums' | 'vocal' | 'analyzer';
export type Focus = 'playlist' | 'piano' | 'rack' | 'mixer' | 'other';
export type ThemeName = 'dark' | 'darkblue' | 'purple' | 'neon' | 'custom';
export type PlaylistTool = 'draw' | 'select' | 'slice';

export interface CustomTheme {
  accent: string;
  bg: string;
  panel: string;
  grid: string;
  text: string;
}

export type WindowKind =
  | { kind: 'channel'; channelId: Id }
  | { kind: 'effect'; trackId: Id; effectId: Id }
  | { kind: 'settings' }
  | { kind: 'projects' }
  | { kind: 'export' }
  | { kind: 'shortcuts' }
  | { kind: 'about' };

export interface FloatingWindow {
  id: string;
  win: WindowKind;
  x: number;
  y: number;
  z: number;
}

export interface UIState {
  // selection
  selectedChannelId: Id | null;
  selectedPatternId: Id | null;
  selectedClipIds: Id[];
  selectedNoteIds: Id[];
  selectedMixerId: Id;
  selectedTrackId: Id | null;
  focus: Focus;
  // editors
  playlistTool: PlaylistTool;
  playlistSnap: number; // ticks
  playlistZoom: number; // px per beat
  trackHeight: number;
  pianoSnap: number;
  pianoZoomX: number; // px per beat
  pianoZoomY: number; // px per key
  pianoChannelId: Id | null;
  lastNoteLength: number;
  followPlayhead: boolean;
  // layout
  dockTab: DockTab;
  browserWidth: number;
  dockHeight: number;
  browserOpen: boolean;
  dockOpen: boolean;
  windows: FloatingWindow[];
  // transport mirror (set from the engine)
  playing: boolean;
  recording: boolean;
  mode: 'pattern' | 'song';
  metronome: boolean;
  typingKeyboard: boolean;
  // settings
  theme: ThemeName;
  accent: string | null;
  custom: CustomTheme;
  autosave: boolean;
  latencyHint: 'interactive' | 'balanced' | 'playback';
  inputDeviceId: string;
  outputDeviceId: string;
  inputGain: number;
  monitoring: boolean;
  setupDone: boolean;
  toast: { text: string; kind: 'info' | 'error' | 'ok'; id: number } | null;
  dragPayload: DragPayload | null;
}

export type DragPayload =
  | { type: 'instrument'; kind: 'synth' | 'sampler'; preset?: string }
  | { type: 'drum'; drum: string }
  | { type: 'sample'; assetId: string; name: string }
  | { type: 'effect'; effect: EffectType; preset?: string }
  | { type: 'synthPreset'; preset: string }
  | { type: 'pattern'; patternId: string };

const PERSIST_KEY = 'goofy-studio-ui';
const PERSISTED: (keyof UIState)[] = [
  'playlistSnap', 'playlistZoom', 'trackHeight', 'pianoSnap', 'pianoZoomX', 'pianoZoomY', 'followPlayhead', 'dockTab',
  'browserWidth', 'dockHeight', 'browserOpen', 'dockOpen', 'theme', 'accent', 'custom', 'autosave', 'latencyHint',
  'inputDeviceId', 'outputDeviceId', 'inputGain', 'setupDone', 'metronome', 'lastNoteLength',
];

function loadPersisted(): Partial<UIState> {
  try {
    const raw = localStorage.getItem(PERSIST_KEY);
    return raw ? JSON.parse(raw) : {};
  } catch {
    return {};
  }
}

export const useUI = create<UIState>(() => ({
  selectedChannelId: null,
  selectedPatternId: null,
  selectedClipIds: [],
  selectedNoteIds: [],
  selectedMixerId: 'master',
  selectedTrackId: null,
  focus: 'playlist',
  playlistTool: 'draw',
  playlistSnap: PPQ,
  playlistZoom: 22,
  trackHeight: 44,
  pianoSnap: PPQ / 4,
  pianoZoomX: 80,
  pianoZoomY: 16,
  pianoChannelId: null,
  lastNoteLength: PPQ / 4,
  followPlayhead: true,
  dockTab: 'rack',
  browserWidth: 230,
  dockHeight: 330,
  browserOpen: true,
  dockOpen: true,
  windows: [],
  playing: false,
  recording: false,
  mode: 'pattern',
  metronome: false,
  typingKeyboard: false,
  theme: 'dark',
  accent: null,
  custom: { accent: '#f28c28', bg: '#16191d', panel: '#22272d', grid: '#2c343c', text: '#d7dde3' },
  autosave: true,
  latencyHint: 'interactive',
  inputDeviceId: '',
  outputDeviceId: '',
  inputGain: 1,
  monitoring: false,
  setupDone: false,
  toast: null,
  dragPayload: null,
  ...loadPersisted(),
}));

let persistTimer: ReturnType<typeof setTimeout> | null = null;
useUI.subscribe((s) => {
  if (persistTimer) clearTimeout(persistTimer);
  persistTimer = setTimeout(() => {
    const out: Record<string, unknown> = {};
    for (const k of PERSISTED) out[k] = s[k];
    try { localStorage.setItem(PERSIST_KEY, JSON.stringify(out)); } catch { /* private mode */ }
  }, 300);
});

export const ui = () => useUI.getState();
export const setUI = (p: Partial<UIState> | ((s: UIState) => Partial<UIState>)) => useUI.setState(p);

let toastId = 0;
export function toast(text: string, kind: 'info' | 'error' | 'ok' = 'info') {
  const id = ++toastId;
  setUI({ toast: { text, kind, id } });
  setTimeout(() => {
    if (useUI.getState().toast?.id === id) setUI({ toast: null });
  }, kind === 'error' ? 5000 : 2600);
}

let zTop = 10;
export function openWindow(win: WindowKind) {
  const key = JSON.stringify(win);
  const s = useUI.getState();
  const existing = s.windows.find((w) => JSON.stringify(w.win) === key);
  zTop++;
  if (existing) {
    setUI({ windows: s.windows.map((w) => (w.id === existing.id ? { ...w, z: zTop } : w)) });
    return;
  }
  const n = s.windows.length;
  setUI({
    windows: [...s.windows, { id: `w${Date.now()}${n}`, win, x: 260 + (n % 6) * 28, y: 110 + (n % 6) * 28, z: zTop }],
  });
}

export function closeWindow(id: string) {
  setUI((s) => ({ windows: s.windows.filter((w) => w.id !== id) }));
}

export function focusWindow(id: string) {
  zTop++;
  setUI((s) => ({ windows: s.windows.map((w) => (w.id === id ? { ...w, z: zTop } : w)) }));
}

export function moveWindow(id: string, x: number, y: number) {
  setUI((s) => ({ windows: s.windows.map((w) => (w.id === id ? { ...w, x, y } : w)) }));
}
