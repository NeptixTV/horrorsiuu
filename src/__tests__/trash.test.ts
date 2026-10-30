import { describe, expect, it } from 'vitest';
import { daysLeft, expiredTrash, lostProjects, TRASH_MS } from '../project/trash';

const DAY = 86400000;
const NOW = Date.UTC(2026, 9, 30, 12);

describe('project trash', () => {
  it('keeps deleted projects for 30 days', () => {
    const entries = [
      { id: 'fresh', deletedAt: NOW - DAY },
      { id: 'edge', deletedAt: NOW - TRASH_MS + 1000 },
      { id: 'old', deletedAt: NOW - TRASH_MS },
      { id: 'older', deletedAt: NOW - 90 * DAY },
    ];
    expect(expiredTrash(entries, NOW)).toEqual(['old', 'older']);
  });

  it('never removes entries with a damaged or future date', () => {
    expect(expiredTrash([{ id: 'nan', deletedAt: NaN }, { id: 'future', deletedAt: NOW + 5 * DAY }], NOW)).toEqual([]);
  });

  it('counts the days that are left', () => {
    expect(daysLeft(NOW, NOW)).toBe(30);
    expect(daysLeft(NOW - 29.5 * DAY, NOW)).toBe(1);
    expect(daysLeft(NOW - 40 * DAY, NOW)).toBe(0);
  });

  it('finds deleted projects in their newest backup', () => {
    const backups = [
      { id: 'b1', projectId: 'live', time: 5 },
      { id: 'b2', projectId: 'gone', time: 10 },
      { id: 'b3', projectId: 'gone', time: 30 },
      { id: 'b4', projectId: 'gone', time: 20 },
      { id: 'b5', projectId: 'trashed', time: 40 },
      { id: 'b6', projectId: 'also-gone', time: 50 },
    ];
    const lost = lostProjects(backups, new Set(['live', 'trashed']));
    expect(lost.map((b) => b.id)).toEqual(['b6', 'b3']);
  });

  it('finds nothing when every project still exists', () => {
    expect(lostProjects([{ id: 'b', projectId: 'p', time: 1 }], new Set(['p']))).toEqual([]);
  });
});
