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
  | { kind: 'clipEffect'; clipId: Id; effectId: Id }
  | { kind: 'clip'; clipId: Id }
  | { kind: 'track'; trackId: Id }
  | { kind: 'sampleEditor'; assetId: Id }
  | { kind: 'stems' }
  | { kind: 'admin' }
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
  /** Playlist placement: off = exactly at the mouse (default), grid = hard grid snap */
  snapMode: 'grid' | 'off';
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
  // ---- audio settings ----
  /** custom output buffer in ms (0 = use latencyHint) */
  latencyMs: number;
  /** 0 = device default */
  sampleRate: number;
  limiter: boolean;
  ceilingDb: number;
  protect: boolean;
  outputVolume: number;
  /** scheduler look-ahead in seconds */
  lookahead: number;
  metronomeVolume: number;
  countInBars: number;
  autoFallbackOutput: boolean;
  releaseMic: boolean;
  // ---- design ----
  uiScale: number;
  reduceMotion: boolean;
  meterFps: number;
  showSplash: boolean;
  // ---- projects ----
  autosaveInterval: number; // seconds
  backups: boolean;
  backupCount: number;
  backupInterval: number; // minutes between automatic backups
  openLastProject: boolean;
  historyLimit: number;
  // ---- general ----
  notifyLevel: 'all' | 'important' | 'errors';
  checkUpdates: boolean;
  showNews: boolean;
  showHints: boolean;
  confirmDeletes: boolean;
  midiInput: string; // '' = all devices, 'off' = disabled
  seenWhatsNew: string;
  toast: { text: string; kind: 'info' | 'error' | 'ok'; id: number } | null;
  dragPayload: DragPayload | null;
}

export type DragPayload =
  | { type: 'instrument'; kind: 'synth' | 'sampler'; preset?: string }
  | { type: 'drum'; drum: string }
  | { type: 'sample'; assetId: string; name: string }
  | { type: 'effect'; effect: EffectType; preset?: string }
  | { type: 'synthPreset'; preset: string }
  | { type: 'pattern'; patternId: string }
  | { type: 'fxchain'; name: string };

const PERSIST_KEY = 'goofy-studio-ui';
const PERSISTED: (keyof UIState)[] = [
  'playlistSnap', 'snapMode', 'playlistZoom', 'trackHeight', 'pianoSnap', 'pianoZoomX', 'pianoZoomY', 'followPlayhead', 'dockTab',
  'browserWidth', 'dockHeight', 'browserOpen', 'dockOpen', 'theme', 'accent', 'custom', 'autosave', 'latencyHint',
  'inputDeviceId', 'outputDeviceId', 'inputGain', 'setupDone', 'metronome', 'lastNoteLength',
  'latencyMs', 'sampleRate', 'limiter', 'ceilingDb', 'protect', 'outputVolume', 'lookahead', 'metronomeVolume', 'countInBars',
  'autoFallbackOutput', 'releaseMic', 'uiScale', 'reduceMotion', 'meterFps', 'showSplash', 'autosaveInterval', 'backups',
  'backupCount', 'backupInterval', 'openLastProject', 'historyLimit', 'notifyLevel', 'checkUpdates', 'showNews', 'showHints',
  'confirmDeletes', 'midiInput', 'seenWhatsNew',
];

/** Defaults of every persisted setting (Settings → "Reset to defaults" uses these). */
export const SETTING_DEFAULTS = {
  latencyHint: 'interactive' as UIState['latencyHint'],
  latencyMs: 0,
  sampleRate: 0,
  limiter: true,
  ceilingDb: -0.3,
  protect: true,
  outputVolume: 1,
  lookahead: 0.12,
  metronomeVolume: 0.7,
  countInBars: 0,
  autoFallbackOutput: true,
  releaseMic: true,
  uiScale: 1,
  reduceMotion: false,
  meterFps: 60,
  showSplash: true,
  autosave: true,
  autosaveInterval: 45,
  backups: true,
  backupCount: 10,
  backupInterval: 10,
  openLastProject: true,
  historyLimit: 150,
  notifyLevel: 'all' as UIState['notifyLevel'],
  checkUpdates: true,
  showNews: true,
  showHints: true,
  confirmDeletes: true,
  midiInput: '',
  followPlayhead: true,
};

/** Reads the saved settings; values of the wrong type are ignored (defaults win). */
function loadPersisted(): Partial<UIState> {
  try {
    const raw = localStorage.getItem(PERSIST_KEY);
    const parsed: unknown = raw ? JSON.parse(raw) : {};
    if (typeof parsed !== 'object' || parsed === null) return {};
    const p = parsed as Record<string, unknown>;
    const out: Record<string, unknown> = {};
    const base = DEFAULT_STATE as unknown as Record<string, unknown>;
    for (const k of PERSISTED) {
      if (!(k in p)) continue;
      const v = p[k];
      const d = base[k];
      if (d === null || typeof v === typeof d) {
        if (typeof v === 'number' && !Number.isFinite(v)) continue;
        out[k] = v;
      }
    }
    if (out.snapMode !== 'grid') out.snapMode = 'off'; // the old "magnet" mode was removed in 1.1.1
    return out as Partial<UIState>;
  } catch {
    return {};
  }
}

const DEFAULT_STATE: UIState = {
  selectedChannelId: null,
  selectedPatternId: null,
  selectedClipIds: [],
  selectedNoteIds: [],
  selectedMixerId: 'master',
  selectedTrackId: null,
  focus: 'playlist',
  playlistTool: 'draw',
  playlistSnap: PPQ,
  snapMode: 'off',
  playlistZoom: 22,
  trackHeight: 44,
  pianoSnap: PPQ / 4,
  pianoZoomX: 80,
  pianoZoomY: 16,
  pianoChannelId: null,
  lastNoteLength: PPQ / 4,
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
  inputDeviceId: '',
  outputDeviceId: '',
  inputGain: 1,
  monitoring: false,
  setupDone: false,
  ...SETTING_DEFAULTS,
  seenWhatsNew: '',
  toast: null,
  dragPayload: null,
};

export const useUI = create<UIState>(() => ({ ...DEFAULT_STATE, ...loadPersisted() }));

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
/**
 * Shows a short message. `important` info messages survive the "important only"
 * notification level; errors are always shown.
 */
export function toast(text: string, kind: 'info' | 'error' | 'ok' = 'info', important = false) {
  const level = useUI.getState().notifyLevel;
  if (kind !== 'error' && !important && (level === 'errors' || (level === 'important' && kind === 'info'))) return;
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
