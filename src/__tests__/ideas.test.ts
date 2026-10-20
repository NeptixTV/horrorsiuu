import { describe, expect, it } from 'vitest';
import { IDEA_PARTS, MOODS, STYLES, rollIdea, type IdeaOptions } from '../midi/ideas';
import { SCALES } from '../midi/music';
import { TICKS_PER_STEP } from '../state/types';

const base: IdeaOptions = { key: 9, mode: 'minor', mood: 'dreamy', style: 'lofi', bars: 4, seeds: { chords: 1, bass: 2, melody: 3, drums: 4 } };

describe('idea dice', () => {
  it('is deterministic for the same seeds', () => {
    expect(rollIdea(base)).toEqual(rollIdea(base));
  });

  it('re-rolling one part keeps the others', () => {
    const a = rollIdea(base);
    const b = rollIdea({ ...base, seeds: { ...base.seeds, melody: 99 } });
    expect(b.chords).toEqual(a.chords);
    expect(b.bass).toEqual(a.bass);
    expect(b.drums).toEqual(a.drums);
    expect(b.melody).not.toEqual(a.melody);
  });

  it('stays in key and inside the pattern for every mood, style and length', () => {
    for (const mode of ['major', 'minor'] as const) {
      const scale = SCALES[mode === 'minor' ? 'Minor' : 'Major'];
      for (const { id: mood } of MOODS) for (const { id: style } of STYLES) for (const bars of [2, 4, 8] as const) {
        for (let seed = 0; seed < 4; seed++) {
          const key = (seed * 5) % 12;
          const idea = rollIdea({ key, mode, mood, style, bars, seeds: { chords: seed, bass: seed + 1, melody: seed + 2, drums: seed + 3 } });
          const end = idea.lengthSteps * TICKS_PER_STEP;
          expect(idea.lengthSteps).toBe(bars * 16);
          for (const n of [...idea.chords, ...idea.bass, ...idea.melody]) {
            expect(n.start).toBeGreaterThanOrEqual(0);
            expect(n.start + n.length).toBeLessThanOrEqual(end + 1);
            expect(n.length).toBeGreaterThan(0);
            expect(n.velocity).toBeGreaterThan(0);
            expect(n.velocity).toBeLessThanOrEqual(1);
          }
          // melody uses scale tones only and sits in a singable lead range
          for (const n of idea.melody) {
            expect(scale).toContain((((n.pitch - key) % 12) + 12) % 12);
            expect(n.pitch).toBeGreaterThanOrEqual(64);
            expect(n.pitch).toBeLessThanOrEqual(88);
          }
          for (const n of idea.bass) {
            expect(n.pitch).toBeGreaterThanOrEqual(28);
            expect(n.pitch).toBeLessThanOrEqual(64);
          }
          expect(idea.melody.length).toBeGreaterThan(bars);
          expect(idea.chords.length).toBeGreaterThan(0);
          for (const line of Object.values(idea.drums)) expect(line!.length).toBe(bars * 16);
          expect(idea.drums.kick?.some((v) => v > 0)).toBe(true);
        }
      }
    }
  });

  it('ends the melody on the tonic', () => {
    for (let seed = 0; seed < 20; seed++) {
      const idea = rollIdea({ ...base, key: 2, mode: 'major', seeds: { ...base.seeds, melody: seed } });
      expect(idea.melody[idea.melody.length - 1].pitch % 12).toBe(2);
    }
  });

  it('names the progression', () => {
    const idea = rollIdea({ ...base, mood: 'sad', key: 9, mode: 'minor', seeds: { ...base.seeds, chords: 7 } });
    expect(idea.progression.split(' – ').length).toBe(4);
    expect(IDEA_PARTS).toHaveLength(4);
  });
});
