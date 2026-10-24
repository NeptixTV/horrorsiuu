// Small offline DSP toolkit for the sound build scripts. Everything works on
// arrays of Float32Array channels at 48 kHz (see io.mjs).
import { SR } from './io.mjs';

export const db = (x) => Math.pow(10, x / 20);

export function peak(ch) {
  let p = 0;
  for (const c of ch) for (let i = 0; i < c.length; i++) { const a = Math.abs(c[i]); if (a > p) p = a; }
  return p;
}

export function gain(ch, g) {
  for (const c of ch) for (let i = 0; i < c.length; i++) c[i] *= g;
  return ch;
}

export function normalize(ch, target = db(-0.5)) {
  const p = peak(ch);
  if (p > 0) gain(ch, target / p);
  return ch;
}

export const slice = (ch, a, b) => ch.map((c) => c.slice(a, b));

/** Index of the first sample louder than `rel` × peak (start of the sound). */
export function onset(ch, rel = 0.002) {
  const t = peak(ch) * rel;
  const n = ch[0].length;
  for (let i = 0; i < n; i++) for (const c of ch) if (Math.abs(c[i]) > t) return i;
  return 0;
}

/** RMS envelope in windows of `win` samples. */
export function rmsEnvelope(ch, win = Math.round(SR * 0.02)) {
  const n = ch[0].length;
  const out = new Float32Array(Math.ceil(n / win));
  for (let w = 0; w < out.length; w++) {
    let s = 0;
    let k = 0;
    for (let i = w * win; i < Math.min(n, (w + 1) * win); i++) {
      for (const c of ch) s += c[i] * c[i];
      k += ch.length;
    }
    out[w] = Math.sqrt(s / Math.max(1, k));
  }
  return out;
}

/** Sample index after which the sound stays below `relDb` (relative to its loudest window). */
export function tailEnd(ch, relDb = -62) {
  const win = Math.round(SR * 0.02);
  const env = rmsEnvelope(ch, win);
  let max = 0;
  for (const v of env) if (v > max) max = v;
  const t = max * db(relDb);
  for (let w = env.length - 1; w >= 0; w--) if (env[w] > t) return Math.min(ch[0].length, (w + 1) * win);
  return ch[0].length;
}

/** Raised-cosine fades (in seconds). */
export function fade(ch, inSec = 0, outSec = 0) {
  const n = ch[0].length;
  const fi = Math.min(n, Math.round(inSec * SR));
  const fo = Math.min(n, Math.round(outSec * SR));
  for (const c of ch) {
    for (let i = 0; i < fi; i++) c[i] *= 0.5 - 0.5 * Math.cos((Math.PI * i) / fi);
    for (let i = 0; i < fo; i++) c[n - 1 - i] *= 0.5 - 0.5 * Math.cos((Math.PI * i) / fo);
  }
  return ch;
}

/**
 * Makes [start, end) loop seamlessly: the `xfade` samples before `end` are
 * blended (equal power) into the samples before `start`, so jumping from end
 * back to start continues the waveform. The end is first moved (±search) to
 * the position whose waveform matches the start best.
 */
export function makeLoop(ch, start, end, xfade, search = Math.round(SR * 0.02)) {
  const mono = ch[0].map((v, i) => (ch[1] ? 0.5 * (v + ch[1][i]) : v));
  const win = 1024;
  let best = end;
  let bestScore = -Infinity;
  for (let e = end - search; e <= end + search; e++) {
    if (e - win < start || e + 1 >= mono.length) continue;
    let s = 0;
    let na = 0;
    let nb = 0;
    for (let i = -win; i < 0; i++) {
      const a = mono[start + i];
      const b = mono[e + i];
      s += a * b;
      na += a * a;
      nb += b * b;
    }
    const score = s / Math.sqrt(na * nb + 1e-12);
    if (score > bestScore) { bestScore = score; best = e; }
  }
  end = best;
  for (const c of ch) {
    for (let i = 0; i < xfade; i++) {
      const w = i / xfade; // 0 → 1 towards the loop end
      const a = Math.cos((w * Math.PI) / 2);
      const b = Math.sin((w * Math.PI) / 2);
      const k = end - xfade + i;
      c[k] = c[k] * a + c[start - xfade + i] * b;
    }
  }
  return { start, end, score: bestScore };
}

// ---------- synthesis helpers ----------

export function buffer(seconds, channels = 1) {
  const n = Math.max(1, Math.round(seconds * SR));
  return Array.from({ length: channels }, () => new Float32Array(n));
}

/** Deterministic noise source (so builds are reproducible). */
export function rng(seed = 1) {
  let s = seed >>> 0 || 1;
  return () => {
    s ^= s << 13; s >>>= 0;
    s ^= s >> 17;
    s ^= s << 5; s >>>= 0;
    return (s / 4294967296) * 2 - 1;
  };
}

/** RBJ biquad, processed in place. type: lp hp bp peak lowshelf highshelf notch */
export function biquad(x, type, f, q = 0.707, gainDb = 0, sr = SR) {
  const w = (2 * Math.PI * Math.min(f, sr * 0.49)) / sr;
  const cw = Math.cos(w);
  const sw = Math.sin(w);
  const A = Math.pow(10, gainDb / 40);
  const alpha = sw / (2 * q);
  let b0, b1, b2, a0, a1, a2;
  switch (type) {
    case 'lp': b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case 'hp': b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case 'bp': b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case 'notch': b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
    case 'peak': b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A; a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
    case 'lowshelf': {
      const s2 = 2 * Math.sqrt(A) * alpha;
      b0 = A * ((A + 1) - (A - 1) * cw + s2); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - s2);
      a0 = (A + 1) + (A - 1) * cw + s2; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - s2;
      break;
    }
    case 'highshelf': {
      const s2 = 2 * Math.sqrt(A) * alpha;
      b0 = A * ((A + 1) + (A - 1) * cw + s2); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - s2);
      a0 = (A + 1) - (A - 1) * cw + s2; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - s2;
      break;
    }
    default: throw new Error(`unknown filter ${type}`);
  }
  b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const x0 = x[i];
    const y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x0; y2 = y1; y1 = y0;
    x[i] = y0;
  }
  return x;
}

/** One-pole lowpass with a time-varying cutoff (fn(i) → Hz). */
export function sweepLp(x, fn) {
  let y = 0;
  for (let i = 0; i < x.length; i++) {
    const a = 1 - Math.exp((-2 * Math.PI * fn(i)) / SR);
    y += a * (x[i] - y);
    x[i] = y;
  }
  return x;
}

/** Exponential decay envelope value at time t (s): reaches −60 dB after `t60`. */
export const expDecay = (t, t60) => Math.exp((-6.9078 * t) / t60);

/** Attack/decay envelope applied in place. */
export function envelope(x, { attack = 0.001, t60 = 0.5, hold = 0, curve = 1 } = {}) {
  for (let i = 0; i < x.length; i++) {
    const t = i / SR;
    let e;
    if (t < attack) e = t / attack;
    else if (t < attack + hold) e = 1;
    else e = Math.pow(expDecay(t - attack - hold, t60), curve);
    x[i] *= e;
  }
  return x;
}

/** Soft clipper with a little asymmetry (adds even harmonics like analog gear). */
export function saturate(x, drive = 1, asym = 0) {
  const norm = Math.tanh(drive);
  for (let i = 0; i < x.length; i++) {
    const v = x[i] * drive + asym;
    x[i] = (Math.tanh(v) - Math.tanh(asym)) / norm;
  }
  return x;
}

export function mix(dest, src, g = 1, offset = 0) {
  const n = Math.min(dest.length - offset, src.length);
  for (let i = 0; i < n; i++) dest[i + offset] += src[i] * g;
  return dest;
}

/** Sine with a frequency function fn(t) → Hz. */
export function sweepSine(seconds, fn, phase = 0) {
  const [x] = buffer(seconds);
  let ph = phase;
  for (let i = 0; i < x.length; i++) {
    x[i] = Math.sin(ph);
    ph += (2 * Math.PI * fn(i / SR)) / SR;
  }
  return x;
}

/** Band-limited (PolyBLEP) square / saw oscillator at a fixed frequency. */
export function blep(seconds, freq, shape = 'square', phase0 = 0) {
  const [x] = buffer(seconds);
  const dt = freq / SR;
  let ph = phase0;
  const pb = (t) => {
    if (t < dt) { t /= dt; return t + t - t * t - 1; }
    if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
    return 0;
  };
  for (let i = 0; i < x.length; i++) {
    if (shape === 'saw') {
      x[i] = 2 * ph - 1 - pb(ph);
    } else {
      let v = ph < 0.5 ? 1 : -1;
      v += pb(ph);
      v -= pb((ph + 0.5) % 1);
      x[i] = v;
    }
    ph += dt;
    if (ph >= 1) ph -= 1;
  }
  return x;
}

export function noise(seconds, seed = 1) {
  const [x] = buffer(seconds);
  const r = rng(seed);
  for (let i = 0; i < x.length; i++) x[i] = r();
  return x;
}

/** Small stereo room: a few early reflections + a short diffuse tail (Schroeder). */
export function room(ch, { size = 0.5, decay = 0.6, wet = 0.2, damp = 6000, seed = 3 } = {}) {
  const n = ch[0].length;
  const outs = [new Float32Array(n), new Float32Array(n)];
  const r = rng(seed);
  for (let side = 0; side < 2; side++) {
    const src = ch[Math.min(side, ch.length - 1)];
    const out = outs[side];
    // early reflections
    for (let k = 0; k < 8; k++) {
      const d = Math.round(SR * (0.004 + size * 0.03 * (k + 1) / 8 + Math.abs(r()) * 0.003));
      const g = 0.5 * Math.pow(0.75, k) * (r() > 0 ? 1 : -1);
      for (let i = d; i < n; i++) out[i] += src[i - d] * g;
    }
    // comb filters in parallel → all-passes in series
    const combs = [1557, 1617, 1491, 1422].map((m) => Math.round(m * (0.6 + size * 0.8) * (side ? 1.03 : 1) * SR / 44100));
    const tail = new Float32Array(n);
    for (const m of combs) {
      const buf = new Float32Array(m);
      const fb = Math.pow(0.001, (m / SR) / Math.max(0.05, decay));
      let lp = 0;
      const a = 1 - Math.exp((-2 * Math.PI * damp) / SR);
      let idx = 0;
      for (let i = 0; i < n; i++) {
        const y = buf[idx];
        lp += a * (y - lp);
        buf[idx] = src[i] + lp * fb;
        idx = (idx + 1) % m;
        tail[i] += y * 0.25;
      }
    }
    for (const m of [225, 556, 441].map((v) => Math.round(v * SR / 44100))) {
      const buf = new Float32Array(m);
      let idx = 0;
      for (let i = 0; i < n; i++) {
        const b = buf[idx];
        const y = -0.5 * tail[i] + b;
        buf[idx] = tail[i] + 0.5 * b;
        idx = (idx + 1) % m;
        tail[i] = y;
      }
    }
    for (let i = 0; i < n; i++) out[i] = out[i] * 0.6 + tail[i];
  }
  const dry = ch.length === 1 ? [ch[0], ch[0].slice()] : ch;
  return dry.map((c, s) => c.map((v, i) => v + outs[s][i] * wet));
}

/** Simple peak compressor for one-shots (instant attack look-ahead, smooth release). */
export function compress(ch, { threshold = db(-12), ratio = 4, release = 0.08, makeup = 1 } = {}) {
  const n = ch[0].length;
  const rel = Math.exp(-1 / (release * SR));
  let env = 0;
  for (let i = 0; i < n; i++) {
    let a = 0;
    for (const c of ch) a = Math.max(a, Math.abs(c[i]));
    env = a > env ? a : env * rel + a * (1 - rel);
    let g = 1;
    if (env > threshold) g = Math.pow(env / threshold, 1 / ratio - 1);
    for (const c of ch) c[i] *= g * makeup;
  }
  return ch;
}
