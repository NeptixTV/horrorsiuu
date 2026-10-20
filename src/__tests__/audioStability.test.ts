import { describe, expect, it } from 'vitest';
import { nextBuffer, nextLookahead } from '../ui/audioStability';

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
