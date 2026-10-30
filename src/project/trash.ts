// Rules of the project trash: deleted projects stay restorable for 30 days,
// and projects deleted before the trash existed are found again in their
// automatic backups. Kept free of storage code so it can be tested directly.

export const TRASH_DAYS = 30;
const DAY = 86400000;
export const TRASH_MS = TRASH_DAYS * DAY;

export interface TrashEntry {
  id: string;
  deletedAt: number;
}

export interface BackupEntry {
  id: string;
  projectId: string;
  time: number;
}

/** Trash entries older than 30 days (a damaged date never removes anything). */
export function expiredTrash(entries: TrashEntry[], now: number): string[] {
  return entries.filter((e) => Number.isFinite(e.deletedAt) && now - e.deletedAt >= TRASH_MS).map((e) => e.id);
}

/** Whole days until an entry is removed for good (at least 0). */
export function daysLeft(deletedAt: number, now: number): number {
  return Math.max(0, Math.ceil((deletedAt + TRASH_MS - now) / DAY));
}

/**
 * Projects that only live on in their backups (the project itself was
 * deleted): the newest backup of every project id that is not `known`.
 */
export function lostProjects(backups: BackupEntry[], known: Set<string>): BackupEntry[] {
  const newest = new Map<string, BackupEntry>();
  for (const b of backups) {
    if (known.has(b.projectId)) continue;
    const cur = newest.get(b.projectId);
    if (!cur || b.time > cur.time) newest.set(b.projectId, b);
  }
  return [...newest.values()].sort((a, b) => b.time - a.time);
}
