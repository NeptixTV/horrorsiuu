import { describe, expect, it } from 'vitest';
import { currentMarker, matchSection, sections } from '../ui/notes/prompter';
import { createEmptyProject } from '../state/defaults';

const LYRICS = `[Intro]
oh oh

[Verse 1]
first line
# Pre-Chorus
build up
[Chorus]
the hook
[Verse 2]
again`;

describe('teleprompter', () => {
  it('finds [section] and # headings with their line', () => {
    expect(sections(LYRICS)).toEqual([
      { name: 'Intro', line: 0 }, { name: 'Verse 1', line: 3 }, { name: 'Pre-Chorus', line: 5 }, { name: 'Chorus', line: 7 }, { name: 'Verse 2', line: 9 },
    ]);
    expect(sections('just text\nno headings')).toEqual([]);
  });

  it('matches marker names loosely (case, spaces, prefixes)', () => {
    const s = sections(LYRICS);
    expect(matchSection(s, 'chorus')).toBe(3);
    expect(matchSection(s, 'CHORUS ')).toBe(3);
    expect(matchSection(s, 'Verse 2')).toBe(4);
    expect(matchSection(s, 'verse2')).toBe(4);
    expect(matchSection(s, 'Verse')).toBe(1); // first verse
    expect(matchSection(s, 'Bridge')).toBe(-1);
    expect(matchSection(s, '')).toBe(-1);
  });

  it('knows which marker the playhead is in', () => {
    const p = createEmptyProject();
    p.markers = [{ id: 'a', tick: 0, name: 'Intro', color: '#fff' }, { id: 'b', tick: 1536, name: 'Chorus', color: '#fff' }];
    expect(currentMarker(p, 10)?.name).toBe('Intro');
    expect(currentMarker(p, 1536)?.name).toBe('Chorus');
    expect(currentMarker({ ...p, markers: [] }, 50)).toBeNull();
  });
});
