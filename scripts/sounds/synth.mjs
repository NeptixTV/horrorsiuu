// Oversampled synthesis helpers for the drum kits and synth instruments.
// Everything is computed at 4 × 48 kHz so that saturation and metallic
// oscillators do not alias, then filtered and decimated to 48 kHz.
import { SR } from './io.mjs';
import { biquad, rng } from './dsp.mjs';

export const OS = 4;
export const R = SR * OS;

export const len = (s) => Math.max(1, Math.round(s * R));
export const arr = (s) => new Float32Array(len(s));

/** Windowed-sinc lowpass for the decimation (Blackman-Harris, 383 taps). */
const FIR = (() => {
  const N = 383;
  const fc = 21000 / R;
  const h = new Float64Array(N);
  let sum = 0;
  for (let i = 0; i < N; i++) {
    const m = i - (N - 1) / 2;
    const sinc = m === 0 ? 2 * fc : Math.sin(2 * Math.PI * fc * m) / (Math.PI * m);
    const w = 0.35875 - 0.48829 * Math.cos((2 * Math.PI * i) / (N - 1)) + 0.14128 * Math.cos((4 * Math.PI * i) / (N - 1)) - 0.01168 * Math.cos((6 * Math.PI * i) / (N - 1));
    h[i] = sinc * w;
    sum += h[i];
  }
  for (let i = 0; i < N; i++) h[i] /= sum;
  return h;
})();

/** 192 kHz → 48 kHz. The filter delay is compensated, so the attack stays at sample 0. */
export function decimate(x) {
  const N = FIR.length;
  const half = (N - 1) / 2;
  const out = new Float32Array(Math.ceil(x.length / OS));
  for (let o = 0; o < out.length; o++) {
    const c = o * OS;
    let s = 0;
    const a = Math.max(0, half - c);
    const b = Math.min(N, x.length - c + half);
    for (let k = a; k < b; k++) s += FIR[k] * x[c - half + k];
    out[o] = s;
  }
  return out;
}

export const bq = (x, type, f, q = 0.707, g = 0) => biquad(x, type, f, q, g, R);

/** Sine whose frequency follows fn(t). */
export function sine(seconds, fn, phase = 0) {
  const x = arr(seconds);
  let ph = phase;
  for (let i = 0; i < x.length; i++) {
    x[i] = Math.sin(ph);
    ph += (2 * Math.PI * fn(i / R)) / R;
  }
  return x;
}

/** Naive square at 192 kHz (aliasing is far above the audible band after decimation). */
export function square(seconds, f, duty = 0.5, phase = 0) {
  const x = arr(seconds);
  let ph = phase;
  const dt = f / R;
  for (let i = 0; i < x.length; i++) {
    x[i] = ph < duty ? 1 : -1;
    ph += dt;
    if (ph >= 1) ph -= 1;
  }
  return x;
}

export function saw(seconds, f, phase = 0, drift = 0, seed = 1) {
  const x = arr(seconds);
  let ph = phase;
  const r = rng(seed);
  let d = 0;
  for (let i = 0; i < x.length; i++) {
    if (drift && i % 960 === 0) d = d * 0.98 + r() * drift * 0.02;
    x[i] = 2 * ph - 1;
    ph += (f * (1 + d)) / R;
    if (ph >= 1) ph -= 1;
  }
  return x;
}

export function noise(seconds, seed = 1) {
  const x = arr(seconds);
  const r = rng(seed);
  for (let i = 0; i < x.length; i++) x[i] = r();
  return x;
}

/** Pink-ish noise (Paul Kellet's filter). */
export function pink(seconds, seed = 1) {
  const x = noise(seconds, seed);
  let b0 = 0, b1 = 0, b2 = 0;
  for (let i = 0; i < x.length; i++) {
    const w = x[i];
    b0 = 0.99765 * b0 + w * 0.099046;
    b1 = 0.963 * b1 + w * 0.2965164;
    b2 = 0.57 * b2 + w * 1.0526913;
    x[i] = (b0 + b1 + b2 + w * 0.1848) * 0.2;
  }
  return x;
}

/** Envelope: linear attack, then exponential decay to −60 dB in t60 (optionally a second, slower stage). */
export function env(x, { a = 0.0005, t60 = 0.3, hold = 0, t60b = 0, mixb = 0, delay = 0 } = {}) {
  const k = -6.9078;
  for (let i = 0; i < x.length; i++) {
    const t = i / R - delay;
    let e;
    if (t < 0) e = 0;
    else if (t < a) e = t / a;
    else if (t < a + hold) e = 1;
    else {
      const u = t - a - hold;
      e = (1 - mixb) * Math.exp((k * u) / t60) + (t60b ? mixb * Math.exp((k * u) / t60b) : 0);
    }
    x[i] *= e;
  }
  return x;
}

export function drive(x, amount = 1, asym = 0) {
  const n = Math.tanh(amount + Math.abs(asym)) || 1;
  const off = Math.tanh(asym);
  for (let i = 0; i < x.length; i++) x[i] = (Math.tanh(x[i] * amount + asym) - off) / n;
  return x;
}

/** Hard-ish clipper with a soft knee (the "loud trap drum" sound). */
export function clip(x, ceiling = 0.8) {
  for (let i = 0; i < x.length; i++) {
    const v = x[i] / ceiling;
    x[i] = ceiling * (Math.abs(v) < 1 ? v - (v * v * v) / 3 : Math.sign(v) * (2 / 3)) * 1.5;
  }
  return x;
}

export function add(dest, src, g = 1, offsetSec = 0) {
  const o = Math.round(offsetSec * R);
  const n = Math.min(dest.length - o, src.length);
  for (let i = 0; i < n; i++) dest[i + o] += src[i] * g;
  return dest;
}

export function scale(x, g) {
  for (let i = 0; i < x.length; i++) x[i] *= g;
  return x;
}

/** Linear fade-out over the last `sec` seconds. */
export function tail(x, sec = 0.01) {
  const n = Math.min(x.length, Math.round(sec * R));
  for (let i = 0; i < n; i++) x[x.length - 1 - i] *= i / n;
  return x;
}
