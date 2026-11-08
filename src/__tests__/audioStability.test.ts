import { describe, expect, it } from 'vitest';
import { nextBuffer, nextLookahead, tooManyDropouts, tooManyLate } from '../ui/audioStability';

describe('automatic audio stability steps', () => {
  it('raises the output buffer one step at a time and stops at the safest', () => {
    let s = { latencyHint: 'interactive' as const as 'interactive' | 'balanced' | 'playback', latencyMs: 0 };
    const seen: string[] = [];
    for (let i = 0; i < 6; i++) {
      const step = nextBuffer(s);
      if (!step) break;
      seen.push(step.label);
      s = { ...s, ...step.patch } as typeof s;
    }
    expect(seen).toEqual(['“Balanced”', '“Most stable”', '150 ms', '250 ms']);
    expect(nextBuffer(s)).toBeNull();
  });

  it('raises the scheduling window up to half a second', () => {
    expect(nextLookahead(0.07)).toBe(0.2);
    expect(nextLookahead(0.12)).toBe(0.2);
    expect(nextLookahead(0.2)).toBe(0.35);
    expect(nextLookahead(0.35)).toBe(0.5);
    expect(nextLookahead(0.5)).toBeNull();
  });
});

describe('when the studio steps in', () => {
  const now = 1_000_000;
  it('reacts to a burst of dropouts', () => {
    expect(tooManyDropouts([now - 1000, now - 2000, now - 3000], now)).toBe(true);
  });
  it('reacts to dropouts now and then (2 within 5 minutes)', () => {
    expect(tooManyDropouts([now - 200_000, now - 1000], now)).toBe(true);
    expect(tooManyDropouts([now - 1000], now)).toBe(false);
    expect(tooManyDropouts([now - 400_000, now - 1000], now)).toBe(false);
  });
  it('reacts to a scheduler that is late now and then', () => {
    expect(tooManyLate([now], now, 2)).toBe(true);
    expect(tooManyLate([now - 100_000, now - 50_000, now], now, 1)).toBe(true);
    expect(tooManyLate([now - 100_000, now], now, 1)).toBe(false);
  });
});
