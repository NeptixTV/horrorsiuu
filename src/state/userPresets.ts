// User presets for instruments and effects ("Save preset…"). Stored locally
// (localStorage) and exportable / importable as a small JSON file so presets
// can be shared between computers.
import { create } from 'zustand';
import { uid } from './utils';

/** 'dream' | 'synth' | 'effect:<type>' */
export type PresetKind = string;

export interface UserPreset {
  id: string;
  kind: PresetKind;
  name: string;
  params: Record<string, unknown>;
  created: number;
}

const KEY = 'goofy-studio-user-presets';
const MAX = 500;

function load(): UserPreset[] {
  try {
    const raw = JSON.parse(localStorage.getItem(KEY) ?? '[]');
    return Array.isArray(raw) ? raw.filter(valid).slice(0, MAX) : [];
  } catch {
    return [];
  }
}

function valid(p: unknown): p is UserPreset {
  if (typeof p !== 'object' || p === null) return false;
  const x = p as Record<string, unknown>;
  return typeof x.id === 'string' && typeof x.kind === 'string' && typeof x.name === 'string' && typeof x.params === 'object' && x.params !== null && !Array.isArray(x.params);
}

export const usePresets = create<{ list: UserPreset[] }>(() => ({ list: load() }));

function persist(list: UserPreset[]) {
  usePresets.setState({ list });
  try { localStorage.setItem(KEY, JSON.stringify(list)); } catch { /* storage full / private mode */ }
}

export function presetsOf(kind: PresetKind) {
  return usePresets.getState().list.filter((p) => p.kind === kind);
}

export function saveUserPreset(kind: PresetKind, name: string, params: object): UserPreset {
  const clean = name.trim().slice(0, 60) || 'My preset';
  const list = usePresets.getState().list;
  const existing = list.find((p) => p.kind === kind && p.name.toLowerCase() === clean.toLowerCase());
  const preset: UserPreset = { id: existing?.id ?? uid('up'), kind, name: clean, params: structuredClone(params) as Record<string, unknown>, created: Date.now() };
  persist([...list.filter((p) => p.id !== preset.id), preset].slice(-MAX));
  return preset;
}

export function renameUserPreset(id: string, name: string) {
  persist(usePresets.getState().list.map((p) => (p.id === id ? { ...p, name: name.trim().slice(0, 60) || p.name } : p)));
}

export function deleteUserPreset(id: string) {
  persist(usePresets.getState().list.filter((p) => p.id !== id));
}

export function exportPresets(): Blob {
  return new Blob([JSON.stringify({ format: 'goofy-studio-presets', version: 1, presets: usePresets.getState().list }, null, 1)], { type: 'application/json' });
}

/** Adds presets from a file; returns how many were imported. */
export async function importPresets(file: File): Promise<number> {
  if (file.size > 5 * 1024 * 1024) throw new Error('Preset file too large');
  const json = JSON.parse(await file.text());
  if (!json || json.format !== 'goofy-studio-presets' || !Array.isArray(json.presets)) throw new Error('Not a Goofy Studio preset file');
  const incoming = (json.presets as unknown[]).filter(valid).map((p) => ({ ...p, id: uid('up'), name: p.name.slice(0, 60) }));
  persist([...usePresets.getState().list, ...incoming].slice(-MAX));
  return incoming.length;
}
