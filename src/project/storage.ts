// IndexedDB persistence for projects, audio assets and automatic backups.
// Everything stays on this computer (browser profile / app data folder).
import type { Project } from '../state/types';

const DB_NAME = 'goofy-studio';
const DB_VERSION = 2;

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
  /** 1 for library samples (indexable copy of `library`) */
  lib?: number;
}

/** Small per-asset info so listings never have to load the audio itself. */
export interface AssetMeta {
  id: string;
  name: string;
  bytes: number;
  library: boolean;
  savedAt: number;
}

export interface BackupRecord {
  id: string;
  projectId: string;
  name: string;
  time: number;
  reason: 'auto' | 'save' | 'before-restore' | 'repair';
  data: Project;
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
      req.onupgradeneeded = (ev) => {
        const db = req.result;
        const t = req.transaction!;
        if (!db.objectStoreNames.contains('projects')) db.createObjectStore('projects', { keyPath: 'id' });
        const assets = db.objectStoreNames.contains('assets') ? t.objectStore('assets') : db.createObjectStore('assets', { keyPath: 'id' });
        if (!assets.indexNames.contains('byLibrary')) assets.createIndex('byLibrary', 'lib');
        if (!db.objectStoreNames.contains('assetMeta')) db.createObjectStore('assetMeta', { keyPath: 'id' });
        if (!db.objectStoreNames.contains('backups')) {
          const b = db.createObjectStore('backups', { keyPath: 'id' });
          b.createIndex('byProject', 'projectId');
        }
        // v1 → v2: index library samples and build the asset info table once
        if (ev.oldVersion < 2 && ev.oldVersion > 0) {
          const meta = t.objectStore('assetMeta');
          assets.openCursor().onsuccess = (e) => {
            const cur = (e.target as IDBRequest<IDBCursorWithValue | null>).result;
            if (!cur) return;
            const rec = cur.value as AssetRecord;
            if (rec.library && rec.lib !== 1) cur.update({ ...rec, lib: 1 });
            meta.put(metaOf(rec));
            cur.continue();
          };
        }
      };
      req.onsuccess = () => {
        const db = req.result;
        // another window upgraded the database: close so it can proceed
        db.onversionchange = () => db.close();
        resolve(db);
      };
      req.onerror = () => reject(req.error);
      req.onblocked = () => reject(new Error('Storage is busy (another Goofy Studio window is open?)'));
    });
    dbPromise.catch(() => { dbPromise = null; });
  }
  return dbPromise;
}

function metaOf(rec: AssetRecord): AssetMeta {
  const bytes = (rec.channels ?? []).reduce((n, c) => n + (c?.byteLength ?? 0), 0);
  return { id: rec.id, name: rec.name, bytes, library: !!rec.library, savedAt: Date.now() };
}

function tx<T>(store: string | string[], mode: IDBTransactionMode, fn: (t: IDBTransaction) => IDBRequest<T>): Promise<T> {
  return openDb().then(
    (db) =>
      new Promise<T>((resolve, reject) => {
        const t = db.transaction(store, mode);
        const req = fn(t);
        let result: T;
        req.onsuccess = () => { result = req.result; };
        req.onerror = () => reject(req.error);
        t.oncomplete = () => resolve(result);
        t.onerror = () => reject(t.error);
        t.onabort = () => reject(t.error ?? new Error('Storage transaction aborted (disk full?)'));
      }),
  );
}

const one = <T>(store: string, mode: IDBTransactionMode, fn: (s: IDBObjectStore) => IDBRequest<T>) => tx<T>(store, mode, (t) => fn(t.objectStore(store)));

export const storage = {
  async saveProject(project: Project): Promise<void> {
    await one('projects', 'readwrite', (s) => s.put({ id: project.id, name: project.name, updatedAt: project.updatedAt, data: project } satisfies ProjectRecord));
  },
  async loadProject(id: string): Promise<Project | null> {
    const rec = await one<ProjectRecord | undefined>('projects', 'readonly', (s) => s.get(id));
    return rec?.data ?? null;
  },
  async listProjects(): Promise<Omit<ProjectRecord, 'data'>[]> {
    const all = await one<ProjectRecord[]>('projects', 'readonly', (s) => s.getAll());
    return all.map(({ id, name, updatedAt }) => ({ id, name, updatedAt })).sort((a, b) => b.updatedAt - a.updatedAt);
  },
  async allProjects(): Promise<Project[]> {
    const all = await one<ProjectRecord[]>('projects', 'readonly', (s) => s.getAll());
    return all.map((r) => r.data);
  },
  async deleteProject(id: string): Promise<void> {
    await one('projects', 'readwrite', (s) => s.delete(id));
  },
  async saveAsset(id: string, name: string, buffer: AudioBuffer, library = false): Promise<void> {
    const channels = Array.from({ length: buffer.numberOfChannels }, (_, c) => buffer.getChannelData(c).slice());
    const rec: AssetRecord = { id, name, sampleRate: buffer.sampleRate, channels, library, ...(library ? { lib: 1 } : {}) };
    await tx(['assets', 'assetMeta'], 'readwrite', (t) => {
      t.objectStore('assetMeta').put(metaOf(rec));
      return t.objectStore('assets').put(rec);
    });
  },
  async loadAsset(id: string): Promise<AssetRecord | null> {
    return (await one<AssetRecord | undefined>('assets', 'readonly', (s) => s.get(id))) ?? null;
  },
  async hasAsset(id: string): Promise<boolean> {
    const key = await one<IDBValidKey | undefined>('assets', 'readonly', (s) => s.getKey(id));
    return key !== undefined;
  },
  async deleteAsset(id: string): Promise<void> {
    await tx(['assets', 'assetMeta'], 'readwrite', (t) => {
      t.objectStore('assetMeta').delete(id);
      return t.objectStore('assets').delete(id);
    });
  },
  async renameAsset(id: string, name: string): Promise<void> {
    const rec = await this.loadAsset(id);
    if (!rec) return;
    const next = { ...rec, name };
    await tx(['assets', 'assetMeta'], 'readwrite', (t) => {
      t.objectStore('assetMeta').put(metaOf(next));
      return t.objectStore('assets').put(next);
    });
  },
  /** Only the library samples (uses the index – other audio is not loaded). */
  async listLibraryAssets(): Promise<AssetRecord[]> {
    return one<AssetRecord[]>('assets', 'readonly', (s) => s.index('byLibrary').getAll(1));
  },
  async listAssetMeta(): Promise<AssetMeta[]> {
    return one<AssetMeta[]>('assetMeta', 'readonly', (s) => s.getAll());
  },
  async listAssetIds(): Promise<string[]> {
    return (await one<IDBValidKey[]>('assets', 'readonly', (s) => s.getAllKeys())).map(String);
  },

  // ---------- backups ----------
  async saveBackup(b: BackupRecord, keep: number): Promise<void> {
    await one('backups', 'readwrite', (s) => s.put(b));
    const list = await this.listBackups(b.projectId);
    const extra = list.slice(Math.max(1, keep));
    for (const old of extra) await one('backups', 'readwrite', (s) => s.delete(old.id));
  },
  /** Newest first, without the project data. */
  async listBackups(projectId?: string): Promise<Omit<BackupRecord, 'data'>[]> {
    const all = await one<BackupRecord[]>('backups', 'readonly', (s) => (projectId ? s.index('byProject').getAll(projectId) : s.getAll()));
    return all.map(({ id, projectId: p, name, time, reason }) => ({ id, projectId: p, name, time, reason })).sort((a, b) => b.time - a.time);
  },
  async loadBackup(id: string): Promise<BackupRecord | null> {
    return (await one<BackupRecord | undefined>('backups', 'readonly', (s) => s.get(id))) ?? null;
  },
  async deleteBackup(id: string): Promise<void> {
    await one('backups', 'readwrite', (s) => s.delete(id));
  },
  async allBackupProjects(): Promise<Project[]> {
    return (await one<BackupRecord[]>('backups', 'readonly', (s) => s.getAll())).map((b) => b.data);
  },
};

/** Builds an AudioBuffer from a stored record (defensive against damaged data). */
export function recordToBuffer(rec: AssetRecord): AudioBuffer {
  const chans = (Array.isArray(rec.channels) ? rec.channels : []).filter((c) => c instanceof Float32Array).slice(0, 32);
  const len = Math.max(1, chans[0]?.length ?? 1);
  const sr = Number.isFinite(rec.sampleRate) ? Math.min(768000, Math.max(3000, rec.sampleRate)) : 44100;
  const b = new AudioBuffer({ length: len, numberOfChannels: Math.max(1, chans.length), sampleRate: sr });
  chans.forEach((d, c) => {
    const copy = new Float32Array(len);
    copy.set(d.subarray(0, len));
    // damaged samples would poison the mixer – zero them
    for (let i = 0; i < len; i++) if (!Number.isFinite(copy[i])) copy[i] = 0;
    b.copyToChannel(copy, c);
  });
  return b;
}
