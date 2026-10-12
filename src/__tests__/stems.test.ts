import { describe, expect, it } from 'vitest';
import { separate, stereoCorrelation } from '../audio/stems/dsp';

const SR = 22050;

function goertzel(x: Float32Array, f: number, from: number, to: number) {
  const k = (2 * Math.PI * f) / SR;
  const c = 2 * Math.cos(k);
  let s1 = 0;
  let s2 = 0;
  for (let i = from; i < to; i++) {
    const s0 = x[i] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  return s1 * s1 + s2 * s2 - c * s1 * s2;
}

function testSignal() {
  const n = SR * 2;
  const L = new Float32Array(n);
  const R = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const centre = 0.4 * Math.sin(2 * Math.PI * 440 * t); // "vocal": centred and sustained
    L[i] = centre + 0.3 * Math.sin(2 * Math.PI * 1320 * t); // instrument hard left
    R[i] = centre + 0.3 * Math.sin(2 * Math.PI * 990 * t); // instrument hard right
  }
  return { L, R };
}

describe('stem separation', () => {
  it('puts the centred tone into vocals and side tones into the instrumental', () => {
    const { L, R } = testSignal();
    const res = separate(L, R, SR, { stems: ['vocals', 'instrumental'] });
    const [vL] = res.vocals!;
    const [iL] = res.instrumental!;
    const a = SR / 2;
    const b = SR * 1.5;
    // vocals: 440 Hz dominates over the left instrument
    expect(goertzel(vL, 440, a, b)).toBeGreaterThan(20 * goertzel(vL, 1320, a, b));
    // instrumental: the left instrument dominates over the vocal
    expect(goertzel(iL, 1320, a, b)).toBeGreaterThan(5 * goertzel(iL, 440, a, b));
  });

  it('vocals + instrumental reconstructs the original', () => {
    const { L, R } = testSignal();
    const res = separate(L, R, SR, { stems: ['vocals', 'instrumental'] });
    let err = 0;
    let ref = 0;
    for (let i = 4096; i < L.length - 4096; i++) {
      const s = res.vocals![0][i] + res.instrumental![0][i];
      err += (s - L[i]) ** 2;
      ref += L[i] ** 2;
    }
    expect(err / ref).toBeLessThan(1e-4);
  });

  it('detects mono material', () => {
    const { L } = testSignal();
    expect(stereoCorrelation(L, L)).toBeCloseTo(1, 5);
  });
});
