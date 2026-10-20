import { describe, expect, it } from 'vitest';
import { arpSequence, diatonicChord, joyDirection, romanNumerals, voiceChord } from '../midi/chords';
import { normalizeDream } from '../state/normalize';
import { applyDreamPreset, DREAM_PRESETS } from '../audio/instruments/dreamPresets';
import { defaultDream } from '../state/defaults';

const names = (key: number, mode: 'major' | 'minor', mod: Parameters<typeof diatonicChord>[3] = 'none') =>
  [0, 1, 2, 3, 4, 5, 6].map((d) => diatonicChord(key, mode, d, mod).name);

describe('DreamChord chord theory', () => {
  it('builds the seven diatonic chords of a key', () => {
    expect(names(0, 'major')).toEqual(['C', 'Dm', 'Em', 'F', 'G', 'Am', 'B°']);
    expect(names(9, 'minor')).toEqual(['Am', 'B°', 'C', 'Dm', 'Em', 'F', 'G']);
    expect(names(7, 'major')).toEqual(['G', 'Am', 'Bm', 'C', 'D', 'Em', 'F♯°']);
    expect(names(5, 'major')).toEqual(['F', 'Gm', 'Am', 'B♭', 'C', 'Dm', 'E°']);
  });

  it('roman numerals follow the chord quality', () => {
    expect(romanNumerals('major')).toEqual(['I', 'ii', 'iii', 'IV', 'V', 'vi', 'vii°']);
    expect(romanNumerals('minor')).toEqual(['i', 'ii°', 'III', 'iv', 'v', 'VI', 'VII']);
  });

  it('joystick variations', () => {
    expect(names(0, 'major', 'seventh')).toEqual(['Cmaj7', 'Dm7', 'Em7', 'Fmaj7', 'G7', 'Am7', 'Bm7♭5']);
    expect(diatonicChord(0, 'major', 0, 'ninth').name).toBe('Cmaj9');
    expect(diatonicChord(0, 'major', 4, 'ninth').name).toBe('G9');
    expect(diatonicChord(0, 'major', 0, 'add9').name).toBe('Cadd9');
    expect(diatonicChord(0, 'major', 1, 'six').name).toBe('Dm6');
    expect(diatonicChord(0, 'major', 0, 'sus4').name).toBe('Csus4');
    expect(diatonicChord(0, 'major', 0, 'sus2').name).toBe('Csus2');
    expect(diatonicChord(0, 'major', 0, 'flip').name).toBe('Cm');
    expect(diatonicChord(0, 'major', 1, 'flip').name).toBe('D');
    expect(diatonicChord(0, 'major', 6, 'flip').name).toBe('B'); // the third flips, the fifth becomes perfect
    expect(diatonicChord(0, 'major', 5, 'dom7').name).toBe('A7');
  });

  it('maps the joystick vector to 8 directions with a dead zone', () => {
    expect(joyDirection(0, 0)).toBe('c');
    expect(joyDirection(0.2, 0.1)).toBe('c');
    expect(joyDirection(0, 1)).toBe('n');
    expect(joyDirection(0.7, 0.7)).toBe('ne');
    expect(joyDirection(1, 0)).toBe('e');
    expect(joyDirection(0, -1)).toBe('s');
    expect(joyDirection(-1, 0)).toBe('w');
    expect(joyDirection(-0.7, 0.7)).toBe('nw');
  });

  it('voices chords in range and moves smoothly between them', () => {
    const c = voiceChord(diatonicChord(0, 'major', 0), 0, 'close');
    const f = voiceChord(diatonicChord(0, 'major', 3), 0, 'close', c);
    for (const n of [...c, ...f]) {
      expect(n).toBeGreaterThanOrEqual(53);
      expect(n).toBeLessThanOrEqual(79);
    }
    // C → F with voice leading keeps the common tone C and moves the others by a step or two
    const moved = f.map((n) => Math.min(...c.map((x) => Math.abs(x - n))));
    expect(Math.max(...moved)).toBeLessThanOrEqual(2);
    // open / spread voicings start with the bass note
    const open = voiceChord(diatonicChord(0, 'major', 0), 0, 'open');
    expect(open[0] % 12).toBe(0);
    expect(open[0]).toBeLessThan(48);
    expect(voiceChord(diatonicChord(9, 'minor', 0), 1, 'spread')[0]).toBe(36 + 9 + 12);
  });

  it('arpeggiator orders', () => {
    expect(arpSequence([60, 64, 67], 'up')).toEqual([60, 64, 67]);
    expect(arpSequence([60, 64, 67], 'down')).toEqual([67, 64, 60]);
    expect(arpSequence([60, 64, 67], 'updown')).toEqual([60, 64, 67, 64]);
    expect(arpSequence([60, 64], 'up', 2)).toEqual([60, 64, 72, 76]);
  });
});

describe('DreamChord presets', () => {
  it('every preset produces valid settings', () => {
    for (const p of DREAM_PRESETS) {
      const applied = applyDreamPreset(defaultDream(), p);
      expect(normalizeDream(applied)).toEqual(applied);
    }
  });

  it('a preset keeps key, octave and hold of the player', () => {
    const cur = { ...defaultDream(), key: 4, mode: 'minor' as const, hold: true };
    const next = applyDreamPreset(cur, DREAM_PRESETS[2]);
    expect(next.key).toBe(4);
    expect(next.mode).toBe('minor');
    expect(next.hold).toBe(true);
    expect(next.preset).toBe(DREAM_PRESETS[2].name);
  });

  it('the requested presets exist', () => {
    const want = ['Ocean Dream', 'Nostalgic Memories', 'Midnight Beach', 'Warm Sunset', 'Dreamy Piano', 'Space Ambient', 'Retro Synth', 'Lo-Fi Cassette'];
    for (const n of want) expect(DREAM_PRESETS.some((p) => p.name === n)).toBe(true);
  });
});
