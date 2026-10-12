import { describe, expect, it } from 'vitest';
import { compareVersions, parseFeed } from '../admin/feed';

describe('update feed', () => {
  it('compares versions numerically', () => {
    expect(compareVersions('1.1.0', '1.0.9')).toBe(1);
    expect(compareVersions('v1.10.0', '1.9.3')).toBe(1);
    expect(compareVersions('1.2', '1.2.0')).toBe(0);
    expect(compareVersions('2.0.0', '10.0.0')).toBe(-1);
  });

  it('parses a feed and ignores broken entries', () => {
    const f = parseFeed(JSON.stringify({
      messages: [{ id: 'a', title: 'Hi', body: 'x', kind: 'info', date: '2026-01-01' }, { nope: true }],
      update: { version: '1.2.0', notes: 'new', date: '2026-01-01', assets: {} },
    }));
    expect(f?.messages).toHaveLength(1);
    expect(f?.update?.version).toBe('1.2.0');
    expect(parseFeed('not json')).toBeNull();
    expect(parseFeed('{}')?.update).toBeNull();
  });
});
