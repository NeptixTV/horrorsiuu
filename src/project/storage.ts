// IndexedDB persistence for projects and audio assets.
import type { Project } from '../state/types';

const DB_NAME = 'goofy-studio';
const DB_VERSION = 1;

export interface ProjectRecord {
  id: string;
  name: string;
  updatedAt: number;
  data: Project;
}

export interface AssetRecord {
  id: string;
  name: string;
  sampleRate: number;
  channels: Float32Array[];
  /** true for samples the user imported into the browser library */
  library?: boolean;
}

let dbPromise: Promise<IDBDatabase> | null = null;

function openDb(): Promise<IDBDatabase> {
  if (!dbPromise) {
    dbPromise = new Promise((resolve, reject) => {
      if (typeof indexedDB === 'undefined') {
        reject(new Error('IndexedDB unavailable'));
        return;
      }
      const req = indexedDB.open(DB_NAME, DB_VERSION);
      req.onupgradeneeded = () => {
        const db = req.result;
        if (!db.objectStoreNames.contains('projects')) db.createObjectStore('projects', { keyPath: 'id' });
        if (!db.objectStoreNames.contains('assets')) db.createObjectStore('assets', { keyPath: 'id' });
      };
      req.onsuccess = () => resolve(req.result);
      req.onerror = () => reject(req.error);
    });
    dbPromise.catch(() => { dbPromise = null; });
  }
  return dbPromise;
}

function tx<T>(store: string, mode: IDBTransactionMode, fn: (s: IDBObjectStore) => IDBRequest<T>): Promise<T> {
  return openDb().then(
    (db) =>
      new Promise<T>((resolve, reject) => {
        const t = db.transaction(store, mode);
        const req = fn(t.objectStore(store));
        req.onsuccess = () => resolve(req.result);
        req.onerror = () => reject(req.error);
      }),
  );
}

export const storage = {
  async saveProject(project: Project): Promise<void> {
    await tx('projects', 'readwrite', (s) => s.put({ id: project.id, name: project.name, updatedAt: project.updatedAt, data: project } satisfies ProjectRecord));
  },
  async loadProject(id: string): Promise<Project | null> {
    const rec = await tx<ProjectRecord | undefined>('projects', 'readonly', (s) => s.get(id));
    return rec?.data ?? null;
  },
  async listProjects(): Promise<Omit<ProjectRecord, 'data'>[]> {
    const all = await tx<ProjectRecord[]>('projects', 'readonly', (s) => s.getAll());
    return all.map(({ id, name, updatedAt }) => ({ id, name, updatedAt })).sort((a, b) => b.updatedAt - a.updatedAt);
  },
  async deleteProject(id: string): Promise<void> {
    await tx('projects', 'readwrite', (s) => s.delete(id));
  },
  async saveAsset(id: string, name: string, buffer: AudioBuffer, library = false): Promise<void> {
    const channels = Array.from({ length: buffer.numberOfChannels }, (_, c) => buffer.getChannelData(c).slice());
    await tx('assets', 'readwrite', (s) => s.put({ id, name, sampleRate: buffer.sampleRate, channels, library } satisfies AssetRecord));
  },
  async loadAsset(id: string): Promise<AssetRecord | null> {
    return (await tx<AssetRecord | undefined>('assets', 'readonly', (s) => s.get(id))) ?? null;
  },
  async hasAsset(id: string): Promise<boolean> {
    const key = await tx<IDBValidKey | undefined>('assets', 'readonly', (s) => s.getKey(id));
    return key !== undefined;
  },
  async listLibraryAssets(): Promise<AssetRecord[]> {
    const all = await tx<AssetRecord[]>('assets', 'readonly', (s) => s.getAll());
    return all.filter((a) => a.library);
  },
};

export function recordToBuffer(rec: AssetRecord): AudioBuffer {
  const len = rec.channels[0]?.length ?? 1;
  const b = new AudioBuffer({ length: Math.max(1, len), numberOfChannels: Math.max(1, rec.channels.length), sampleRate: rec.sampleRate });
  rec.channels.forEach((d, c) => b.copyToChannel(new Float32Array(d), c));
  return b;
}
