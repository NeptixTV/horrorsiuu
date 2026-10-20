// Projects saved by version 1.1 must open in 1.2 unchanged (no repairs, same
// music). The fixture was created with the 1.1.1 code (createDemoProject).
import { describe, expect, it } from 'vitest';
import { normalizeProject } from '../state/normalize';
import { collectNoteEvents, songLength } from '../audio/sequencer';
import old from './fixtures/v1.1-demo.json';

describe('compatibility with 1.1 projects', () => {
  it('opens without repairs', () => {
    const r = normalizeProject(structuredClone(old));
    expect(r.fixes).toEqual([]);
    expect(r.project).toEqual(old);
  });

  it('plays exactly the same notes', () => {
    const { project } = normalizeProject(structuredClone(old));
    const end = songLength(project);
    const a = collectNoteEvents(project, 'song', null, 0, end);
    expect(a.length).toBeGreaterThan(100);
    // reference: the same call on the untouched file
    expect(a).toEqual(collectNoteEvents(old as never, 'song', null, 0, end));
  });

  it('keeps 1.1 projects compatible after edits with new features', () => {
    const { project } = normalizeProject(structuredClone(old));
    project.markers = [{ id: 'm1', tick: 0, name: 'Intro', color: '#fff' }];
    project.notes = '[Intro]\nhello';
    const again = normalizeProject(JSON.parse(JSON.stringify(project)));
    expect(again.fixes).toEqual([]);
    expect(again.project.notes).toBe('[Intro]\nhello');
  });
});
