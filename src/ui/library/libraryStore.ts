// "My Library": user samples (imported, recorded, edited, split from songs).
// Stored in IndexedDB, mirrored into the asset registry for playback.
import { create } from 'zustand';
import { assets } from '../../audio/assets';
import { storage, recordToBuffer } from '../../project/storage';
import { getProject } from '../../state/store';
import { renderWithEffects } from '../../audio/offlineFx';
import { ticksToSeconds, uid } from '../../state/utils';
import { toast } from '../../state/ui';
import { engine } from '../../audio/engine';
import { analyzeSound, trimRange, type SoundCategory, type SoundInfo } from '../../audio/soundAnalysis';

export interface LibrarySampleInfo {
  id: string;
  name: string;
  duration: number;
  category: SoundCategory;
  root?: number;
  bpm?: number;
}

interface LibraryState {
  samples: LibrarySampleInfo[];
  loaded: boolean;
}

export const useLibrary = create<LibraryState>(() => ({ samples: [], loaded: false }));

const info = (id: string, name: string, si?: SoundInfo): LibrarySampleInfo => ({ id, name, duration: assets.get(id)?.duration ?? 0, category: si?.category ?? 'Other', root: si?.root, bpm: si?.bpm });

export async function loadLibrary() {
  if (useLibrary.getState().loaded) return;
  try {
    const list = await storage.listLibraryAssets();
    for (const rec of list) if (!assets.has(rec.id)) assets.add(rec.id, recordToBuffer(rec), rec.name);
    // samples from older versions have no category yet: guess it now
    const guess = (r: (typeof list)[number]) => r.info ?? analyzeSound(r.name, r.channels, r.sampleRate);
    useLibrary.setState({ samples: list.map((r) => info(r.id, r.name, guess(r))), loaded: true });
  } catch {
    useLibrary.setState({ loaded: true });
  }
}

export async function addToLibrary(buffer: AudioBuffer, name: string, id = uid('lib'), si?: SoundInfo): Promise<string> {
  assets.add(id, buffer, name);
  const meta = si ?? analyzeSound(name, Array.from({ length: buffer.numberOfChannels }, (_, c) => buffer.getChannelData(c)), buffer.sampleRate);
  await storage.saveAsset(id, name, buffer, true, meta).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: [...s.samples.filter((x) => x.id !== id), info(id, name, meta)] }));
  return id;
}

/** Moves a sound to another category of My Sounds. */
export async function setSampleCategory(id: string, category: SoundCategory) {
  const cur = useLibrary.getState().samples.find((x) => x.id === id);
  if (!cur) return;
  const si: SoundInfo = { category, root: cur.root, bpm: cur.bpm };
  await storage.setAssetInfo(id, si).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: s.samples.map((x) => (x.id === id ? { ...x, category } : x)) }));
}

export const AUDIO_FILE = /\.(wav|wave|mp3|ogg|oga|opus|flac|m4a|aac|aif|aiff|webm)$/i;
const MAX_FILE_BYTES = 80 * 1024 * 1024;
const MAX_FILES = 500;

export function isAudioFile(f: File) {
  return AUDIO_FILE.test(f.name) || f.type.startsWith('audio/');
}

/** Silence before and after the sound removed; identical channels stored once. */
function cleanBuffer(b: AudioBuffer): AudioBuffer {
  const chans = Array.from({ length: b.numberOfChannels }, (_, c) => b.getChannelData(c));
  const { start, end } = trimRange(chans, b.sampleRate);
  let mono = chans.length > 1;
  if (mono) for (let i = start; i < end; i += 3) if (Math.abs(chans[0][i] - chans[1][i]) > 1e-5) { mono = false; break; }
  const n = Math.max(1, end - start);
  const out = new AudioBuffer({ length: n, numberOfChannels: mono ? 1 : chans.length, sampleRate: b.sampleRate });
  for (let c = 0; c < out.numberOfChannels; c++) out.copyToChannel(chans[c].subarray(start, start + n), c);
  return out;
}

/**
 * Imports audio files (also whole sample-pack folders) into My Sounds: trims
 * them, sorts them into categories and remembers them for every project.
 * Files that are already in My Sounds are skipped.
 */
export async function importSounds(files: File[]): Promise<string[]> {
  await loadLibrary();
  const audio = files.filter(isAudioFile);
  if (!audio.length) {
    toast('No audio files found (WAV, MP3, OGG, FLAC, M4A, AIFF)', 'error');
    return [];
  }
  const list = audio.slice(0, MAX_FILES);
  const known = new Set(useLibrary.getState().samples.map((x) => `${x.name}|${x.duration.toFixed(3)}`));
  const ids: string[] = [];
  const counts = new Map<SoundCategory, number>();
  let skipped = 0;
  let failed = 0;
  for (let i = 0; i < list.length; i++) {
    const f = list[i];
    if (list.length > 3 && i % 5 === 0) toast(`Importing sounds… ${i + 1} / ${list.length}`, 'info', true);
    if (f.size > MAX_FILE_BYTES) { failed++; continue; }
    try {
      const decoded = await engine.ctx.decodeAudioData(await f.arrayBuffer());
      const buffer = cleanBuffer(decoded);
      const name = f.name.replace(/\.[^.]+$/, '').slice(0, 120) || 'Sound';
      if (known.has(`${name}|${buffer.duration.toFixed(3)}`)) { skipped++; continue; }
      // the folder names of a sample pack help ("Drums/Kicks/…")
      const hint = (f as File & { webkitRelativePath?: string }).webkitRelativePath || f.name;
      const chans = Array.from({ length: buffer.numberOfChannels }, (_, c) => buffer.getChannelData(c));
      const si = analyzeSound(hint.replace(/\.[^.]+$/, ''), chans, buffer.sampleRate);
      ids.push(await addToLibrary(buffer, name, uid('lib'), si));
      counts.set(si.category, (counts.get(si.category) ?? 0) + 1);
    } catch {
      failed++;
    }
  }
  const summary = [...counts.entries()].map(([c, n]) => `${n} ${c.toLowerCase()}`).join(', ');
  if (ids.length) toast(`Added ${ids.length} sound${ids.length > 1 ? 's' : ''} to My Sounds${summary ? ` (${summary})` : ''}${skipped ? ` · ${skipped} already there` : ''}${failed ? ` · ${failed} could not be read` : ''}`, 'ok', true);
  else if (skipped) toast(`Those sounds are already in My Sounds (${skipped})`, 'info', true);
  else toast('Could not read those files', 'error');
  if (audio.length > MAX_FILES) toast(`Only the first ${MAX_FILES} files were imported`, 'info', true);
  return ids;
}

export async function updateLibrarySample(id: string, buffer: AudioBuffer) {
  const cur = useLibrary.getState().samples.find((s) => s.id === id);
  const name = cur?.name ?? assets.name(id);
  const si: SoundInfo | undefined = cur ? { category: cur.category, root: cur.root, bpm: cur.bpm } : undefined;
  assets.add(id, buffer, name);
  await storage.saveAsset(id, name, buffer, true, si).catch(() => undefined);
  useLibrary.setState((s) => ({ samples: s.samples.map((x) => (x.id === id ? info(id, name, si) : x)) }));
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
