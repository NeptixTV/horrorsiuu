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

  it('drops unsafe links and malformed values from the feed', () => {
    const f = parseFeed(JSON.stringify({
      messages: [{ id: 'm', title: 'Hi', body: 42, kind: 'evil' }, { title: 'no id' }, null],
      update: {
        version: '1.3.0', notes: 'n',
        assets: { setup: 'javascript:alert(1)', portable: 'https://github.com/NeptixTV/horrorsiuu/releases/download/v1.3.0/a.exe', page: 'file:///C:/x', setupSha256: 'xyz', portableSha256: 'A'.repeat(64) },
        mandatory: 'yes',
      },
    }))!;
    expect(f.messages).toEqual([{ id: 'm', title: 'Hi', body: '', kind: 'info', date: '' }]);
    expect(f.update?.assets).toEqual({ portable: 'https://github.com/NeptixTV/horrorsiuu/releases/download/v1.3.0/a.exe', portableSha256: 'a'.repeat(64) });
    expect(f.update?.mandatory).toBe(false);
    expect(parseFeed(JSON.stringify({ update: { version: '<script>' } }))?.update).toBeNull();
  });
});
