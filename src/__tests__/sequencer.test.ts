import { describe, expect, it } from 'vitest';
import { collectAudioSpanning, collectAudioStarts, collectNoteEvents, patternLength, songLength } from '../audio/sequencer';
import { createDemoProject, createEmptyProject } from '../state/defaults';
import { PPQ, TICKS_PER_STEP, type Project } from '../state/types';

function simpleProject(): Project {
  const p = createEmptyProject();
  const pat = p.patterns[0];
  const kick = p.channels[0];
  pat.steps[kick.id] = [1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0];
  return p;
}

describe('pattern events', () => {
  it('collects step events for one bar', () => {
    const p = simpleProject();
    const ev = collectNoteEvents(p, 'pattern', p.patterns[0].id, 0, PPQ * 4);
    expect(ev.map((e) => e.tick)).toEqual([0, PPQ, PPQ * 2, PPQ * 3]);
    expect(ev.every((e) => e.channelId === p.channels[0].id)).toBe(true);
  });

  it('loops the pattern when the range spans several repetitions', () => {
    const p = simpleProject();
    const ev = collectNoteEvents(p, 'pattern', p.patterns[0].id, PPQ * 3, PPQ * 5);
    expect(ev.map((e) => e.tick)).toEqual([PPQ * 3, PPQ * 4]);
  });

  it('respects mute and solo', () => {
    const p = simpleProject();
    const pat = p.patterns[0];
    pat.steps[p.channels[1].id] = [1, ...new Array(15).fill(0)];
    p.channels[0].mute = true;
    let ev = collectNoteEvents(p, 'pattern', pat.id, 0, PPQ * 4);
    expect(new Set(ev.map((e) => e.channelId))).toEqual(new Set([p.channels[1].id]));
    p.channels[0].mute = false;
    p.channels[0].solo = true;
    ev = collectNoteEvents(p, 'pattern', pat.id, 0, PPQ * 4);
    expect(new Set(ev.map((e) => e.channelId))).toEqual(new Set([p.channels[0].id]));
  });

  it('applies swing to odd steps only', () => {
    const p = simpleProject();
    const pat = p.patterns[0];
    pat.steps[p.channels[0].id] = [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    p.swing = 1;
    const ev = collectNoteEvents(p, 'pattern', pat.id, 0, PPQ * 4);
    expect(ev.map((e) => e.tick)).toEqual([0, TICKS_PER_STEP + TICKS_PER_STEP / 2]);
  });

  it('extends pattern length to whole bars for long notes', () => {
    const p = simpleProject();
    const pat = p.patterns[0];
    pat.notes[p.channels[4].id] = [{ id: 'n1', pitch: 60, start: PPQ * 5, length: PPQ, velocity: 1 }];
    expect(patternLength(pat)).toBe(PPQ * 8);
  });
});

describe('song events', () => {
  it('places pattern clips at their playlist positions and skips muted tracks', () => {
    const p = createDemoProject();
    const bar = PPQ * 4;
    const ev = collectNoteEvents(p, 'song', null, 0, bar);
    expect(ev.length).toBeGreaterThan(0);
    expect(ev.every((e) => e.tick >= 0 && e.tick < bar)).toBe(true);
    p.tracks[0].mute = true;
    const muted = collectNoteEvents(p, 'song', null, 0, bar);
    expect(muted.length).toBe(0);
  });

  it('truncates notes at the clip end and repeats content inside long clips', () => {
    const p = simpleProject();
    const pat = p.patterns[0];
    p.clips = [{ id: 'c', trackId: p.tracks[0].id, kind: 'pattern', start: PPQ * 4, length: PPQ * 8, offset: 0, patternId: pat.id }];
    const ev = collectNoteEvents(p, 'song', null, 0, PPQ * 16);
    expect(ev.map((e) => e.tick)).toEqual([4, 5, 6, 7, 8, 9, 10, 11].map((b) => b * PPQ));
  });

  it('finds audio clip starts and clips spanning the playhead', () => {
    const p = createEmptyProject();
    p.clips = [{ id: 'a', trackId: p.tracks[0].id, kind: 'audio', start: PPQ * 4, length: PPQ * 4, offset: 0, assetId: 'x' }];
    expect(collectAudioStarts(p, 0, PPQ * 4)).toHaveLength(0);
    expect(collectAudioStarts(p, PPQ * 4, PPQ * 5)).toHaveLength(1);
    const span = collectAudioSpanning(p, PPQ * 6, 120);
    expect(span).toHaveLength(1);
    expect(span[0].sourceOffset).toBeCloseTo(1); // two beats at 120 bpm
    expect(span[0].remaining).toBe(PPQ * 2);
  });

  it('computes song length in whole bars', () => {
    const p = createEmptyProject();
    expect(songLength(p)).toBe(PPQ * 16);
    p.clips = [{ id: 'a', trackId: p.tracks[0].id, kind: 'pattern', start: PPQ * 70, length: PPQ, offset: 0, patternId: p.patterns[0].id }];
    expect(songLength(p)).toBe(PPQ * 72);
  });
});
