// A damaged or hostile MIDI file must not freeze the studio or bloat the project.
import { beforeEach, describe, expect, it } from 'vitest';
import { writeMidi } from '../midi/smf';
import { importMidiFile } from '../midi/midiProject';
import { getProject, useProject } from '../state/store';
import { createEmptyProject } from '../state/defaults';

const file = (bytes: Uint8Array) => new File([bytes as BlobPart], 'test.mid');

describe('MIDI import limits', () => {
  beforeEach(() => {
    useProject.setState({ project: createEmptyProject(), past: [], future: [], pastLabels: [], futureLabels: [] });
  });

  it('ignores notes far beyond a sane song length', async () => {
    const bytes = writeMidi({
      ppq: 96, bpm: 120, timeSig: [4, 4],
      tracks: [{ name: 'Lead', channel: 0, notes: [{ pitch: 60, start: 0, length: 96, velocity: 1 }, { pitch: 62, start: 96 * 4 * 600000, length: 96, velocity: 1 }] }],
    });
    const before = getProject().patterns.length;
    await importMidiFile(file(bytes));
    const p = getProject();
    expect(p.patterns.length - before).toBe(1);
    expect(p.patterns[p.patterns.length - 1].lengthSteps).toBeLessThanOrEqual(64 * 16);
  });

  it('caps the number of tracks', async () => {
    const tracks = Array.from({ length: 120 }, (_, i) => ({ name: `T${i}`, channel: i % 9, notes: [{ pitch: 60 + (i % 12), start: 0, length: 96, velocity: 1 }] }));
    const before = getProject().channels.length;
    await importMidiFile(file(writeMidi({ ppq: 96, bpm: 120, timeSig: [4, 4], tracks })));
    expect(getProject().channels.length - before).toBeLessThanOrEqual(48);
  });

  it('rejects files that are not MIDI', async () => {
    await expect(importMidiFile(file(new TextEncoder().encode('{"not":"midi"}')))).rejects.toThrow();
  });
});
