import { describe, expect, it } from 'vitest';
import type { Note } from '../state/types';
import { arpeggiate, chordAt, duplicateAfter, humanize, invert, legato, reverse, snapToScale, strum, transpose, velocityRamp } from '../midi/noteTools';
import { scaleMask } from '../midi/music';

const n = (pitch: number, start: number, length = 24, velocity = 0.8): Note => ({ id: `${pitch}-${start}`, pitch, start, length, velocity });
const C_MAJOR = scaleMask(0, 'Major');

describe('piano roll note tools', () => {
  it('snaps to the scale', () => {
    expect(snapToScale(61, C_MAJOR)).toBe(62); // C# → D (ties go up)
    expect(snapToScale(60, C_MAJOR)).toBe(60);
    expect(snapToScale(66, C_MAJOR)).toBe(67);
  });

  it('stamps diatonic chords', () => {
    expect(chordAt(60, 'triad', C_MAJOR)).toEqual([60, 64, 67]);
    expect(chordAt(62, 'triad', C_MAJOR)).toEqual([62, 65, 69]); // D minor
    expect(chordAt(67, 'seventh', C_MAJOR)).toEqual([67, 71, 74, 77]); // G7
    expect(chordAt(60, 'power', C_MAJOR)).toEqual([60, 67, 72]);
    expect(chordAt(60, 'sus4', C_MAJOR)).toEqual([60, 65, 67]);
    // other keys: the mask holds absolute pitch classes
    const A_MINOR = scaleMask(9, 'Minor');
    expect(chordAt(69, 'triad', A_MINOR)).toEqual([69, 72, 76]); // Am
    expect(chordAt(73, 'triad', A_MINOR)).toEqual([74, 77, 81]); // C# is snapped to D → Dm
    expect(snapToScale(68, A_MINOR)).toBe(69); // G# → A
  });

  it('transposes and keeps pitches in MIDI range', () => {
    expect(transpose([n(126, 0)], 5)[0].pitch).toBe(127);
    expect(transpose([n(60, 0)], -12)[0].pitch).toBe(48);
  });

  it('legato extends notes to the next chord', () => {
    const out = legato([n(60, 0, 10), n(64, 0, 10), n(67, 48, 10)]);
    expect(out.find((x) => x.pitch === 60)!.length).toBe(48);
    expect(out.find((x) => x.pitch === 67)!.length).toBe(10);
  });

  it('strums and arpeggiates chords', () => {
    const s = strum([n(67, 0), n(60, 0), n(64, 0)], 4);
    expect(s.map((x) => [x.pitch, x.start])).toEqual([[60, 0], [64, 4], [67, 8]]);
    const a = arpeggiate([n(60, 0, 96), n(64, 0, 96), n(67, 0, 96)], 24);
    expect(a.map((x) => x.pitch)).toEqual([60, 64, 67, 60]);
    expect(a.map((x) => x.start)).toEqual([0, 24, 48, 72]);
  });

  it('reverses in time inside the selection', () => {
    const r = reverse([n(60, 0, 24), n(62, 24, 24), n(64, 72, 24)]);
    expect(r.map((x) => [x.pitch, x.start])).toEqual([[60, 72], [62, 48], [64, 0]]);
  });

  it('ramps velocities and inverts pitches', () => {
    const v = velocityRamp([n(60, 0), n(60, 48), n(60, 96)], 0.2, 1);
    expect(v.map((x) => Math.round(x.velocity * 10) / 10)).toEqual([0.2, 0.6, 1]);
    expect(invert([n(60, 0), n(64, 0), n(67, 0)]).map((x) => x.pitch)).toEqual([67, 63, 60]);
  });

  it('humanize is bounded and duplicate places copies after the selection', () => {
    const h = humanize([n(60, 48), n(62, 96)], 6, 0.1, 42);
    for (const x of h) {
      expect(Math.abs(x.start - (x.pitch === 60 ? 48 : 96))).toBeLessThanOrEqual(6);
      expect(x.velocity).toBeLessThanOrEqual(0.9 + 1e-9);
    }
    const d = duplicateAfter([n(60, 0, 24), n(62, 24, 24)], 96);
    expect(d.map((x) => x.start)).toEqual([96, 120]);
    expect(d[0].id).not.toBe('60-0');
  });
});
