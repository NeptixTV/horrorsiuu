// IndexedDB persistence for projects, audio assets, automatic backups and the
// project trash. Everything stays on this computer (browser profile / app data folder).
import type { Project } from '../state/types';
import type { SoundInfo } from '../audio/soundAnalysis';

const DB_NAME = 'goofy-studio';
const DB_VERSION = 3;

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
  /** what the sound is (category, root note, tempo) – library samples */
  info?: SoundInfo;
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

/** A deleted project (restorable for 30 days). */
export interface TrashRecord {
  id: string;
  name: string;
  updatedAt: number;
  deletedAt: number;
  /** 'backup': deleted before the trash existed, found again in its backups */
  from: 'delete' | 'backup';
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
        if (!db.objectStoreNames.contains('trash')) db.createObjectStore('trash', { keyPath: 'id' });
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
    await tx(['projects', 'trash'], 'readwrite', (t) => {
      // a project that is saved again is no longer deleted
      t.objectStore('trash').delete(project.id);
      return t.objectStore('projects').put({ id: project.id, name: project.name, updatedAt: project.updatedAt, data: project } satisfies ProjectRecord);
    });
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
  // ---------- trash ----------
  /** Moves a saved project to the trash. False if there was no such project. */
  async trashProject(id: string, now = Date.now()): Promise<boolean> {
    const rec = await tx<ProjectRecord | undefined>(['projects', 'trash'], 'readwrite', (t) => {
      const req = t.objectStore('projects').get(id);
      req.addEventListener('success', () => {
        const p = req.result as ProjectRecord | undefined;
        if (!p) return;
        t.objectStore('trash').put({ id, name: p.name, updatedAt: p.updatedAt, deletedAt: now, from: 'delete', data: p.data } satisfies TrashRecord);
        t.objectStore('projects').delete(id);
      });
      return req;
    });
    return !!rec;
  },
  /** Newest deletion first, without the project data. */
  async listTrash(): Promise<Omit<TrashRecord, 'data'>[]> {
    const all = await one<TrashRecord[]>('trash', 'readonly', (s) => s.getAll());
    return all.map(({ id, name, updatedAt, deletedAt, from }) => ({ id, name, updatedAt, deletedAt, from })).sort((a, b) => b.deletedAt - a.deletedAt);
  },
  async addToTrash(rec: TrashRecord): Promise<void> {
    await one('trash', 'readwrite', (s) => s.put(rec));
  },
  /**
   * Puts a project from the trash back into the saved projects. If a project
   * with the same id exists meanwhile, it comes back as a copy under `copyId`.
   */
  async restoreFromTrash(id: string, copyId: string): Promise<{ id: string; name: string } | null> {
    let out: { id: string; name: string } | null = null;
    await tx(['projects', 'trash'], 'readwrite', (t) => {
      const req = t.objectStore('trash').get(id);
      req.addEventListener('success', () => {
        const rec = req.result as TrashRecord | undefined;
        if (!rec) return;
        const projects = t.objectStore('projects');
        const clash = projects.getKey(id);
        clash.addEventListener('success', () => {
          const pid = clash.result === undefined ? id : copyId;
          const name = pid === id ? rec.name : `${rec.name} (restored)`;
          projects.put({ id: pid, name, updatedAt: rec.updatedAt, data: { ...rec.data, id: pid, name } } satisfies ProjectRecord);
          t.objectStore('trash').delete(id);
          out = { id: pid, name };
        });
      });
      return req;
    });
    return out;
  },
  /** Removes a project from the trash for good, together with its backups. */
  async deleteFromTrash(id: string): Promise<void> {
    await tx(['projects', 'trash', 'backups'], 'readwrite', (t) => {
      t.objectStore('trash').delete(id);
      const live = t.objectStore('projects').getKey(id);
      live.addEventListener('success', () => {
        // saved again meanwhile: its backups belong to the live project
        if (live.result !== undefined) return;
        const backups = t.objectStore('backups');
        const keys = backups.index('byProject').getAllKeys(id);
        keys.addEventListener('success', () => { for (const k of keys.result) backups.delete(k); });
      });
      return live;
    });
  },
  async allTrashProjects(): Promise<Project[]> {
    return (await one<TrashRecord[]>('trash', 'readonly', (s) => s.getAll())).map((r) => r.data);
  },
  async saveAsset(id: string, name: string, buffer: AudioBuffer, library = false, info?: SoundInfo): Promise<void> {
    const channels = Array.from({ length: buffer.numberOfChannels }, (_, c) => buffer.getChannelData(c).slice());
    const rec: AssetRecord = { id, name, sampleRate: buffer.sampleRate, channels, library, ...(library ? { lib: 1 } : {}), ...(info ? { info } : {}) };
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
  async setAssetInfo(id: string, info: SoundInfo): Promise<void> {
    const rec = await this.loadAsset(id);
    if (!rec) return;
    await one('assets', 'readwrite', (s) => s.put({ ...rec, info }));
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
