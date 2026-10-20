// The output guard runs on every block of the app: it must never let a sample
// above the ceiling through, must leave quiet audio untouched, and must not
// rough up bass notes. Runs the real worklet code with a tiny fake runtime.
import { describe, expect, it } from 'vitest';
import guardSrc from '../audio/worklets/guard.worklet.js?raw';

const SR = 48000;
const N = 128;

type Proc = { process(i: Float32Array[][], o: Float32Array[][]): boolean; port: { onmessage: ((e: { data: unknown }) => void) | null; posted: unknown[] } };

function makeGuard(): Proc {
  const src = guardSrc;
  let Cls: new () => Proc = null as never;
  class Base {
    port = { onmessage: null, posted: [] as unknown[], postMessage(m: unknown) { this.posted.push(m); } };
  }
  new Function('AudioWorkletProcessor', 'registerProcessor', 'sampleRate', src)(Base, (_: string, c: typeof Cls) => { Cls = c; }, SR);
  return new Cls();
}

/** Feeds `seconds` of a signal through the guard, returns the output (left channel). */
function run(g: Proc, seconds: number, fn: (t: number) => number): Float32Array {
  const total = Math.round((seconds * SR) / N) * N;
  const out = new Float32Array(total);
  for (let b = 0; b < total; b += N) {
    const l = new Float32Array(N);
    for (let i = 0; i < N; i++) l[i] = fn((b + i) / SR);
    const r = l.slice();
    const oL = new Float32Array(N);
    const oR = new Float32Array(N);
    g.process([[l, r]], [[oL, oR]]);
    out.set(oL, b);
  }
  return out;
}

const CEIL = Math.pow(10, -0.3 / 20);
const maxAbs = (a: Float32Array, from = 0, to = a.length) => { let m = 0; for (let i = from; i < to; i++) m = Math.max(m, Math.abs(a[i])); return m; };

describe('output guard (limiter)', () => {
  it('never lets a sample above the ceiling through', () => {
    const g = makeGuard();
    const out = run(g, 1, (t) => 3 * Math.sin(2 * Math.PI * 1000 * t) * (t > 0.3 ? 2 : 1));
    expect(maxAbs(out)).toBeLessThanOrEqual(CEIL + 1e-6);
    // and still plays loud (no over-limiting)
    expect(maxAbs(out, SR * 0.5, SR)).toBeGreaterThan(CEIL * 0.9);
  });

  it('leaves quiet audio untouched (only delayed by one block)', () => {
    const g = makeGuard();
    const f = (t: number) => 0.5 * Math.sin(2 * Math.PI * 220 * t);
    const out = run(g, 0.2, f);
    for (let i = N; i < out.length; i++) expect(out[i]).toBeCloseTo(f((i - N) / SR), 5);
  });

  it('does not pump a loud bass note inside its own waveform', () => {
    const g = makeGuard();
    const out = run(g, 2, (t) => 1.6 * Math.sin(2 * Math.PI * 41 * t));
    // after the attack, every cycle should peak at the same level (constant gain)
    const cycle = Math.round(SR / 41);
    const peaks: number[] = [];
    for (let s = SR; s + cycle < out.length; s += cycle) peaks.push(maxAbs(out, s, s + cycle));
    expect(Math.max(...peaks) / Math.min(...peaks)).toBeLessThan(1.02);
    expect(Math.max(...peaks)).toBeLessThanOrEqual(CEIL + 1e-6);
  });

  it('blocks NaN / Infinity and reports them', () => {
    const g = makeGuard();
    const out = run(g, 0.3, (t) => (Math.floor(t * 1000) % 50 === 0 ? NaN : 0.3));
    expect(out.every((x) => Number.isFinite(x))).toBe(true);
    const stats = g.port.posted.filter((m) => (m as { type: string }).type === 'stats') as { nan: number }[];
    expect(stats.reduce((n, s) => n + s.nan, 0)).toBeGreaterThan(0);
  });

  it('mutes a runaway signal (feedback loop) and reports it', () => {
    const g = makeGuard();
    const out = run(g, 0.8, (t) => (t > 0.1 ? 40 * Math.sin(2 * Math.PI * 300 * t) : 0.2 * Math.sin(2 * Math.PI * 300 * t)));
    expect(g.port.posted.some((m) => (m as { type: string }).type === 'runaway')).toBe(true);
    expect(maxAbs(out, out.length - N * 20)).toBeLessThan(1e-3);
  });

  it('is cheap: 10 s of stereo audio in well under a second', () => {
    const g = makeGuard();
    const t0 = performance.now();
    run(g, 10, (t) => 1.2 * Math.sin(2 * Math.PI * 440 * t));
    expect(performance.now() - t0).toBeLessThan(1500);
  });
});
