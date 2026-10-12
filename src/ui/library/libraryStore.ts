// "My Library": user samples (imported, recorded, edited, split from songs).
// Stored in IndexedDB, mirrored into the asset registry for playback.
import { create } from 'zustand';
import { assets } from '../../audio/assets';
import { storage, recordToBuffer } from '../../project/storage';
import { getProject } from '../../state/store';
import { renderWithEffects } from '../../audio/offlineFx';
import { ticksToSeconds, uid } from '../../state/utils';
import { toast } from '../../state/ui';

export interface LibrarySampleInfo {
  id: string;
  name: string;
  duration: number;
}

interface LibraryState {
  samples: LibrarySampleInfo[];
  loaded: boolean;
}

export const useLibrary = create<LibraryState>(() => ({ samples: [], loaded: false }));

const info = (id: string, name: string): LibrarySampleInfo => ({ id, name, duration: assets.get(id)?.duration ?? 0 });

export async function loadLibrary() {
  if (useLibrary.getState().loaded) return;
  try {
    const list = await storage.listLibraryAssets();
    for (const rec of list) if (!assets.has(rec.id)) assets.add(rec.id, recordToBuffer(rec), rec.name);
    useLibrary.setState({ samples: list.map((r) => info(r.id, r.name)), loaded: true });
  } catch {
    useLibrary.setState({ loaded: true });
  }
}

export async function addToLibrary(buffer: AudioBuffer, name: string, id = uid('lib')): Promise<string> {
  assets.add(id, buffer, name);
  await storage.saveAsset(id, name, buffer, true).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: [...s.samples.filter((x) => x.id !== id), info(id, name)] }));
  return id;
}

export async function updateLibrarySample(id: string, buffer: AudioBuffer) {
  const name = useLibrary.getState().samples.find((s) => s.id === id)?.name ?? assets.name(id);
  assets.add(id, buffer, name);
  await storage.saveAsset(id, name, buffer, true).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: s.samples.map((x) => (x.id === id ? info(id, name) : x)) }));
}

export async function renameLibrarySample(id: string, name: string) {
  const b = assets.get(id);
  if (b) assets.add(id, b, name);
  await storage.renameAsset(id, name).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: s.samples.map((x) => (x.id === id ? { ...x, name } : x)) }));
}

export async function deleteLibrarySample(id: string) {
  const usedInProject = Object.prototype.hasOwnProperty.call(getProject().assets, id);
  const b = assets.get(id);
  if (usedInProject && b) {
    // still used by the open project: keep the audio, just remove it from the library
    await storage.saveAsset(id, assets.name(id), b, false).catch(() => undefined);
  } else {
    assets.remove(id);
    await storage.deleteAsset(id).catch(() => undefined);
  }
  useLibrary.setState((s) => ({ samples: s.samples.filter((x) => x.id !== id) }));
}

export function isLibrarySample(id: string) {
  return useLibrary.getState().samples.some((s) => s.id === id);
}

/** Renders a playlist audio clip (with its volume, speed, fades and effects) into a new library sample. */
export async function saveClipToLibrary(clipId: string) {
  const p = getProject();
  const c = p.clips.find((x) => x.id === clipId);
  const src = c?.assetId ? assets.get(c.assetId) : undefined;
  if (!c || !src) return;
  toast('Rendering clip…', 'info');
  const speed = c.speed ?? 1;
  const buf = await renderWithEffects(src, c.fx ?? [], {
    start: c.offset, duration: ticksToSeconds(c.length, p.bpm) * speed, gain: c.gain ?? 1, speed, reverse: c.reverse,
    fadeIn: c.fadeIn, fadeOut: c.fadeOut, bpm: p.bpm,
  });
  const name = `${c.name ?? p.assets[c.assetId!]?.name ?? 'Clip'} (edit)`;
  await addToLibrary(buf, name);
  toast(`Saved “${name}” to My Library`, 'ok');
}
