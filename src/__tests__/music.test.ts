import { describe, expect, it } from 'vitest';
import { freqToMidi, midiToFreq, nearestScaleNote, noteName, scaleMask } from '../midi/music';

describe('music helpers', () => {
  it('converts between MIDI and frequency', () => {
    expect(midiToFreq(69)).toBeCloseTo(440);
    expect(freqToMidi(261.63)).toBeCloseTo(60, 1);
    expect(noteName(60)).toBe('C4');
  });

  it('snaps to the nearest note of a scale', () => {
    const cMajor = scaleMask(0, 'Major');
    expect(nearestScaleNote(60.8, cMajor)).toBe(60); // slightly sharp C → C
    expect(nearestScaleNote(61.3, cMajor)).toBe(62); // closer to D
    const aMinor = scaleMask(9, 'Minor');
    expect(nearestScaleNote(67.7, aMinor)).toBe(67); // flat G# → G
  });
});
