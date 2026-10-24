// Synthesized "Pro" drums, 808s and synth instruments, plus real percussion
// one-shots from VSCO-2-CE (CC0). All synthesis is original code in this file.
import path from 'node:path';
import { readAudio, writeOpus, SR } from './io.mjs';
import { compress, db, fade, gain, makeLoop, normalize, onset, peak, room, slice, tailEnd } from './dsp.mjs';
import { R, add, arr, bq, clip, decimate, drive, env, noise, pink, saw, scale, sine, square, tail } from './synth.mjs';

const exp = Math.exp;

/** Mono 192 kHz signal → stereo 48 kHz. */
const out = (x) => { const d = decimate(x); return [d, d.slice()]; };
const outLR = (l, r) => [decimate(l), decimate(r)];

/** Short tonal "tick" (attack of a beater / stick). */
function tick(freq = 3000, ms = 1.5, amp = 1) {
  const x = sine(ms / 1000, () => freq);
  const n = x.length;
  for (let i = 0; i < n; i++) x[i] *= Math.sin((Math.PI * i) / n) * amp;
  return x;
}

function noiseBurst(sec, seed, hp, t60, amp = 1) {
  const x = noise(sec, seed);
  bq(x, 'hp', hp, 0.7);
  return scale(env(x, { a: 0.0002, t60 }), amp);
}

// ---------------------------------------------------------------- kicks

function kick({ f0 = 50, k = 4, tp = 0.02, t60 = 0.45, t60b = 0.12, mixb = 0.35, drv = 2, asym = 0.08, clickAmt = 0.4, clickHp = 2500, len = 0, eq = [], lp = 16000 }) {
  const T = len || t60 + 0.15;
  const body = sine(T, (t) => f0 * (1 + (k - 1) * exp(-t / tp)));
  env(body, { a: 0.0006, t60, t60b, mixb });
  drive(body, drv, asym);
  if (clickAmt) {
    add(body, noiseBurst(0.012, 11, clickHp, 0.006, clickAmt));
    add(body, tick(3200, 1.2, clickAmt * 0.6));
  }
  for (const [type, f, q, g] of eq) bq(body, type, f, q, g);
  bq(body, 'hp', 24, 0.7);
  if (lp < 20000) bq(body, 'lp', lp, 0.7);
  tail(body, 0.02);
  return out(body);
}

const KICKS = {
  'drum.kick.808': () => kick({ f0: 48, k: 2.3, tp: 0.016, t60: 1.1, t60b: 0.25, mixb: 0.3, drv: 1.4, asym: 0.12, clickAmt: 0.25 }),
  'drum.kick.trap': () => kick({ f0: 52, k: 5, tp: 0.018, t60: 0.5, t60b: 0.12, mixb: 0.4, drv: 2.6, clickAmt: 0.55, eq: [['peak', 62, 1, 2], ['peak', 380, 1, -3], ['highshelf', 5000, 0.7, 2]] }),
  'drum.kick.hard': () => kick({ f0: 54, k: 6, tp: 0.014, t60: 0.42, t60b: 0.1, mixb: 0.45, drv: 5, asym: 0.15, clickAmt: 0.6, eq: [['peak', 120, 1, 2], ['peak', 2500, 1, 3]], lp: 12000 }),
  'drum.kick.drill': () => kick({ f0: 50, k: 4.5, tp: 0.022, t60: 0.36, t60b: 0.09, mixb: 0.5, drv: 3.2, clickAmt: 0.7, clickHp: 3500, eq: [['peak', 4200, 1, 3]] }),
  'drum.kick.boombap': () => {
    const [l] = kick({ f0: 58, k: 2.7, tp: 0.03, t60: 0.34, t60b: 0.1, mixb: 0.35, drv: 1.6, asym: 0.05, clickAmt: 0.35, clickHp: 900, lp: 7500, eq: [['peak', 90, 1, 2], ['peak', 1500, 1.5, 2]] });
    return room([l], { size: 0.25, decay: 0.3, wet: 0.08, damp: 4500, seed: 5 });
  },
  'drum.kick.lofi': () => {
    const [l] = kick({ f0: 55, k: 2.2, tp: 0.03, t60: 0.3, t60b: 0.1, mixb: 0.3, drv: 1.2, clickAmt: 0.15, clickHp: 800, lp: 2200 });
    return room([l], { size: 0.2, decay: 0.25, wet: 0.06, damp: 3000, seed: 6 });
  },
  'drum.kick.909': () => kick({ f0: 54, k: 4.3, tp: 0.025, t60: 0.4, t60b: 0.1, mixb: 0.3, drv: 1.9, asym: 0.04, clickAmt: 0.7, clickHp: 4000 }),
  'drum.kick.house': () => kick({ f0: 50, k: 4, tp: 0.022, t60: 0.55, t60b: 0.15, mixb: 0.3, drv: 2.2, clickAmt: 0.5, clickHp: 3500, eq: [['peak', 55, 1, 2], ['peak', 300, 1, -4]] }),
  'drum.kick.phonk': () => kick({ f0: 47, k: 5, tp: 0.02, t60: 0.7, t60b: 0.15, mixb: 0.4, drv: 7, asym: 0.2, clickAmt: 0.5, lp: 9000, eq: [['peak', 800, 1, 3]] }),
};

// ---------------------------------------------------------------- 808 basses (melodic)

/** One 808 note at `f` Hz. Played chromatically from the nearest root. */
function bass808(f, { k = 1.6, tp = 0.02, t60 = 3.2, drv = 1.6, asym = 0.15, click = 0.2, lp = 9000, mid = 0 }) {
  const T = Math.min(4.2, t60 + 0.2);
  const body = sine(T, (t) => f * (1 + (k - 1) * exp(-t / tp)));
  env(body, { a: 0.0008, t60, t60b: t60 * 0.25, mixb: 0.2 });
  drive(body, drv, asym);
  if (mid) bq(body, 'peak', 700, 0.8, mid);
  if (click) add(body, tick(2400, 1.4, click));
  bq(body, 'hp', 20, 0.7);
  bq(body, 'lp', lp, 0.7);
  tail(body, 0.08);
  return out(body);
}

const BASS_ROOTS = [24, 30, 36, 42, 48];
const hz = (m) => 440 * Math.pow(2, (m - 69) / 12);

const BASSES = {
  'bass.808.classic': { k: 1.6, t60: 3.2, drv: 1.6, asym: 0.15 },
  'bass.808.dist': { k: 1.8, t60: 2.8, drv: 6, asym: 0.25, lp: 7000, mid: 2 },
  'bass.808.drill': { k: 2.1, tp: 0.03, t60: 2.4, drv: 3.5, asym: 0.18, click: 0.35, mid: 3 },
  'bass.808.soft': { k: 1.3, t60: 2.2, drv: 1.1, asym: 0.05, click: 0.05, lp: 1500 },
};

// ---------------------------------------------------------------- snares, claps, rims

function snare({ f1 = 190, f2 = 340, t1 = 0.11, t2 = 0.07, a1 = 0.55, a2 = 0.3, nHp = 1500, nPeak = 5500, nPeakG = 5, nLp = 13000, nT60 = 0.24, nAmt = 0.75, drv = 1.6, seed = 21, roomWet = 0.12, roomSize = 0.3, roomDecay = 0.35, lpOut = 0, clickAmt = 0.3 }) {
  const T = Math.max(nT60, t1) + 0.25;
  const b1 = env(sine(T, (t) => f1 * (1 + 0.6 * exp(-t / 0.006))), { a: 0.0004, t60: t1 });
  const b2 = env(sine(T, (t) => f2 * (1 + 0.4 * exp(-t / 0.005))), { a: 0.0004, t60: t2 });
  const n = noise(T, seed);
  bq(n, 'hp', nHp, 0.7);
  bq(n, 'peak', nPeak, 0.8, nPeakG);
  bq(n, 'lp', nLp, 0.7);
  env(n, { a: 0.0003, t60: nT60, t60b: nT60 * 0.25, mixb: 0.5 });
  const x = arr(T);
  add(x, b1, a1);
  add(x, b2, a2);
  add(x, n, nAmt);
  if (clickAmt) add(x, tick(4000, 0.8, clickAmt));
  drive(x, drv, 0.03);
  if (lpOut) bq(x, 'lp', lpOut, 0.7);
  bq(x, 'hp', 110, 0.7);
  tail(x, 0.03);
  let ch = out(x);
  ch = room([ch[0]], { size: roomSize, decay: roomDecay, wet: roomWet, damp: 7000, seed });
  return compress(ch, { threshold: db(-10), ratio: 3, release: 0.06 });
}

function clap({ bp = 1250, q = 0.8, tailT = 0.28, bursts = [0, 0.009, 0.018, 0.028], wet = 0.18, size = 0.35, decay = 0.5, drv = 1.4, seed = 31, lp = 12000 }) {
  const T = tailT + 0.3;
  const shape = arr(T);
  bursts.forEach((b, i) => {
    const last = i === bursts.length - 1;
    const e = env(new Float32Array(shape.length).fill(1), { a: 0.0003, t60: last ? tailT : 0.012, delay: b });
    add(shape, e, last ? 1 : 0.9);
  });
  const make = (s) => {
    const n = noise(T, s);
    bq(n, 'bp', bp, q);
    bq(n, 'hp', 650, 0.7);
    bq(n, 'peak', 2600, 1, 3);
    bq(n, 'lp', lp, 0.7);
    for (let i = 0; i < n.length; i++) n[i] *= shape[i];
    return drive(n, drv);
  };
  const a = make(seed);
  const b = make(seed + 7);
  const l = arr(T); add(l, a, 0.75); add(l, b, 0.25);
  const r = arr(T); add(r, a, 0.25); add(r, b, 0.75);
  tail(l, 0.03); tail(r, 0.03);
  const ch = room(outLR(l, r), { size, decay, wet, damp: 8000, seed });
  return ch;
}

function rim808() {
  const T = 0.25;
  const x = arr(T);
  add(x, env(sine(T, () => 1720), { a: 0.0002, t60: 0.022 }), 0.6);
  add(x, env(sine(T, () => 480), { a: 0.0002, t60: 0.04 }), 0.5);
  add(x, noiseBurst(0.01, 41, 2000, 0.003, 0.4));
  drive(x, 2.5);
  bq(x, 'hp', 350, 0.7);
  tail(x, 0.02);
  return room([decimate(x)], { size: 0.2, decay: 0.25, wet: 0.08, seed: 41 });
}

function snap() {
  const T = 0.4;
  const n = noise(T, 51);
  bq(n, 'bp', 2300, 2);
  bq(n, 'hp', 900, 0.7);
  env(n, { a: 0.0002, t60: 0.06 });
  const x = arr(T);
  add(x, n, 1);
  add(x, env(sine(T, () => 1900), { a: 0.0002, t60: 0.03 }), 0.25);
  drive(x, 1.5);
  tail(x, 0.02);
  return room([decimate(x)], { size: 0.25, decay: 0.3, wet: 0.2, damp: 9000, seed: 51 });
}

const SNARES = {
  'drum.snare.trap': () => snare({}),
  'drum.snare.drill': () => snare({ f1: 235, f2: 430, t1: 0.08, t2: 0.05, nHp: 2400, nPeak: 3200, nPeakG: 6, nT60: 0.16, drv: 2.2, seed: 22, clickAmt: 0.5, roomWet: 0.1 }),
  'drum.snare.boombap': () => snare({ f1: 175, f2: 290, t1: 0.14, t2: 0.09, a1: 0.5, nHp: 1400, nPeak: 2600, nPeakG: 4, nLp: 8500, nT60: 0.3, nAmt: 1, drv: 1.5, seed: 23, roomWet: 0.26, roomSize: 0.5, roomDecay: 0.6, lpOut: 9000 }),
  'drum.snare.lofi': () => snare({ f1: 190, f2: 320, t1: 0.1, a1: 0.45, nHp: 1100, nPeak: 2800, nLp: 6000, nPeakG: 3, nT60: 0.2, nAmt: 0.9, drv: 1.2, seed: 24, roomWet: 0.2, lpOut: 5000, clickAmt: 0.1 }),
  'drum.snare.909': () => snare({ f1: 180, f2: 330, t1: 0.09, t2: 0.06, nHp: 1200, nPeak: 7000, nPeakG: 3, nT60: 0.26, nAmt: 0.85, drv: 1.8, seed: 25, roomWet: 0.08 }),
  'drum.snare.phonk': () => snare({ f1: 200, f2: 360, nHp: 1200, nT60: 0.2, drv: 5, seed: 26, roomWet: 0.1, lpOut: 10000 }),
  'drum.clap.trap': () => clap({}),
  'drum.clap.big': () => clap({ bp: 1100, tailT: 0.45, wet: 0.3, size: 0.6, decay: 1.1, seed: 33 }),
  'drum.clap.909': () => clap({ bp: 1400, q: 1.1, tailT: 0.22, wet: 0.12, size: 0.3, seed: 35, drv: 1.2 }),
  'drum.rim.808': rim808,
  'drum.snap': snap,
};

// ---------------------------------------------------------------- hats & cymbals

const METAL = [205.3, 304.4, 369.6, 522.7, 540, 800];

function metal(T, ratio = 1, freqs = METAL, seed = 1) {
  const x = arr(T);
  freqs.forEach((f, i) => add(x, square(T, f * ratio, 0.5, ((seed * 0.37 + i * 0.21) % 1)), 1 / freqs.length));
  return x;
}

function hat({ t60 = 0.045, open = false, ratio = 1, bp = 7400, hp = 6800, noiseAmt = 0.25, nHp = 8000, drv = 1.2, lp = 18000, seed = 61 }) {
  const T = (open ? t60 * 1.2 : t60 * 2) + 0.05;
  const m = metal(T, ratio, METAL, seed);
  bq(m, 'bp', bp, 1.4);
  bq(m, 'hp', hp, 0.7);
  const n = noise(T, seed);
  bq(n, 'hp', nHp, 0.7);
  const x = arr(T);
  add(x, m, 1);
  add(x, n, noiseAmt);
  env(x, open ? { a: 0.0002, t60, t60b: t60 * 0.2, mixb: 0.4 } : { a: 0.0002, t60 });
  drive(x, drv);
  if (lp < 18000) bq(x, 'lp', lp, 0.7);
  tail(x, 0.01);
  return out(x);
}

function cymbal({ t60 = 2.2, hp = 4000, seed = 71, ride = false }) {
  const T = t60 + 0.3;
  const r = (s) => { let v = s; return () => { v = (v * 1664525 + 1013904223) >>> 0; return v / 4294967296; }; };
  const rnd = r(seed);
  const freqs = Array.from({ length: 12 }, () => 280 + rnd() * 1100);
  const side = (s) => {
    const x = arr(T);
    add(x, metal(T, 1, freqs, s), 1);
    add(x, noise(T, s), ride ? 0.2 : 0.5);
    bq(x, 'hp', hp, 0.7);
    bq(x, 'peak', ride ? 6000 : 9000, 0.8, 4);
    env(x, { a: 0.0008, t60, t60b: t60 * 0.15, mixb: ride ? 0.3 : 0.5 });
    if (ride) add(x, env(sine(T, () => 4200), { a: 0.0002, t60: 0.5 }), 0.08);
    drive(x, 1.1);
    tail(x, 0.2);
    return x;
  };
  return outLR(side(seed), side(seed + 3));
}

function shaker() {
  const T = 0.25;
  const x = arr(T);
  const g1 = noise(T, 81); bq(g1, 'bp', 6500, 1.2); env(g1, { a: 0.012, t60: 0.09 });
  const g2 = noise(T, 82); bq(g2, 'bp', 7500, 1.4); env(g2, { a: 0.006, t60: 0.06, delay: 0.022 });
  add(x, g1, 1); add(x, g2, 0.5);
  bq(x, 'hp', 3000, 0.7);
  tail(x, 0.02);
  return out(x);
}

const HATS = {
  'drum.hat.808': () => hat({}),
  'drum.hat.trap': () => hat({ t60: 0.034, noiseAmt: 0.45, hp: 7600, nHp: 8500, bp: 8200, drv: 1.4, seed: 62 }),
  'drum.hat.dusty': () => { const [l] = hat({ t60: 0.07, noiseAmt: 0.8, ratio: 1.1, bp: 7000, hp: 5000, lp: 10000, seed: 63 }); return room([l], { size: 0.2, decay: 0.25, wet: 0.1, seed: 63 }); },
  'drum.hat.909': () => hat({ t60: 0.06, noiseAmt: 0.7, ratio: 1.2, bp: 8500, seed: 64 }),
  'drum.hat.lofi': () => hat({ t60: 0.05, noiseAmt: 0.5, lp: 7000, hp: 5000, seed: 65, drv: 1 }),
  'drum.ohat.808': () => hat({ t60: 0.42, open: true, seed: 66 }),
  'drum.ohat.trap': () => hat({ t60: 0.34, open: true, noiseAmt: 0.4, hp: 8000, nHp: 9000, bp: 8800, seed: 67 }),
  'drum.ohat.909': () => hat({ t60: 0.5, open: true, noiseAmt: 0.8, ratio: 1.2, bp: 8500, seed: 68 }),
  'drum.crash': () => cymbal({}),
  'drum.ride': () => cymbal({ t60: 2.6, hp: 3000, seed: 72, ride: true }),
  'drum.shaker': shaker,
};

// ---------------------------------------------------------------- toms, cowbell, percs

function tom(f) {
  const T = 0.7;
  const x = env(sine(T, (t) => f * (1 + 0.55 * exp(-t / 0.03))), { a: 0.0005, t60: 0.55 });
  drive(x, 1.3);
  add(x, noiseBurst(0.01, 91, 600, 0.005, 0.25));
  bq(x, 'hp', 40, 0.7);
  tail(x, 0.02);
  return out(x);
}

function cowbell808(dist = false) {
  const T = 0.7;
  const x = arr(T);
  add(x, square(T, 540), 0.5);
  add(x, square(T, 800), 0.5);
  bq(x, 'bp', 2640, 1.8);
  bq(x, 'bp', 1600, 0.6);
  env(x, { a: 0.0003, t60: 0.5, t60b: 0.05, mixb: 0.6 });
  drive(x, dist ? 6 : 1.6, dist ? 0.1 : 0);
  bq(x, 'hp', 500, 0.7);
  if (dist) bq(x, 'lp', 7000, 0.7);
  tail(x, 0.03);
  return out(x);
}

const PERCS = {
  'drum.tom.808': () => tom(110),
  'drum.cowbell.808': () => cowbell808(false),
  'drum.cowbell.phonk': () => cowbell808(true),
};

// ---------------------------------------------------------------- FX

function impact() {
  const T = 3;
  const sub = env(sine(T, (t) => 28 + 52 * exp(-t / 0.35)), { a: 0.001, t60: 2.4 });
  drive(sub, 1.5);
  const nz = pink(T, 101);
  for (let i = 0, y = 0; i < nz.length; i++) {
    const f = 200 + 9000 * exp(-(i / R) / 0.25);
    const a = 1 - exp((-2 * Math.PI * f) / R);
    y += a * (nz[i] - y);
    nz[i] = y;
  }
  env(nz, { a: 0.001, t60: 1.4 });
  const x = arr(T);
  add(x, sub, 0.9);
  add(x, nz, 0.9);
  tail(x, 0.3);
  return room([decimate(x)], { size: 0.9, decay: 2.5, wet: 0.35, damp: 5000, seed: 101 });
}

function subDrop() {
  const T = 2;
  const x = env(sine(T, (t) => 30 + 70 * exp(-t / 0.4)), { a: 0.002, t60: 1.8 });
  drive(x, 1.6, 0.1);
  tail(x, 0.2);
  return out(x);
}

function riser() {
  const T = 4;
  const side = (s) => {
    const nz = noise(T, s);
    const x = arr(T);
    let y1 = 0, y2 = 0;
    for (let i = 0; i < nz.length; i++) {
      const t = i / R;
      const f = 400 * Math.pow(22, t / T);
      const a = 1 - exp((-2 * Math.PI * f) / R);
      y1 += a * (nz[i] - y1);
      y2 += a * (y1 - y2);
      x[i] = (nz[i] - y2) * 0.4 + y1 * 0.6;
      x[i] *= Math.pow(t / T, 2) * 0.9 + 0.02;
    }
    const tone = arr(T);
    for (let i = 0, ph = 0; i < tone.length; i++) {
      const t = i / R;
      ph += (110 * Math.pow(4, t / T) * (1 + (s % 2 ? 0.006 : -0.006))) / R;
      tone[i] = (2 * (ph % 1) - 1) * Math.pow(t / T, 2.5) * 0.25;
    }
    bq(tone, 'lp', 5000, 0.7);
    add(x, tone, 1);
    for (let i = x.length - Math.round(0.03 * R); i < x.length; i++) x[i] *= (x.length - i) / (0.03 * R);
    return x;
  };
  return outLR(side(111), side(112));
}

const FX = {
  'fx.impact': impact,
  'fx.subdrop': subDrop,
  'fx.riser': riser,
  'fx.revcrash': () => {
    const ch = cymbal({ t60: 2.4, seed: 73 });
    const n = Math.round(SR * 2.4);
    const rev = ch.map((c) => { const s = c.slice(0, n); s.reverse(); return s; });
    return fade(rev, 0.6, 0.004);
  },
};

// ---------------------------------------------------------------- synth instruments (multi-sampled)

/** FM electric piano (two operator pairs: body + "tine" bark). */
function epiano(f, vel) {
  const t60 = Math.min(6, Math.max(1.3, 6 * Math.sqrt(110 / f)));
  const T = Math.min(5, t60 * 0.8) + 0.1;
  const x = arr(T);
  const w = 2 * Math.PI * f;
  for (let i = 0; i < x.length; i++) {
    const t = i / R;
    const i1 = (0.5 + 1.7 * vel) * exp(-t / 0.4) + 0.2;
    const body = Math.sin(w * t + i1 * Math.sin(w * t)) * exp((-6.9 * t) / t60);
    const i2 = (0.2 + 1.8 * vel) * exp(-t / 0.018);
    const bark = Math.sin(w * t + i2 * Math.sin(14 * w * t)) * exp(-t / 0.25) * (0.15 + 0.25 * vel);
    x[i] = (body * 0.8 + bark) * Math.min(1, t / 0.0015);
  }
  drive(x, 1 + vel * 0.6);
  tail(x, 0.05);
  return out(x);
}

/** Music box / bell: struck free bar (inharmonic modes). */
function musicbox(f) {
  const modes = [[1, 1, 3.2], [2.756, 0.45, 1.5], [5.404, 0.25, 0.8], [8.933, 0.12, 0.45], [13.34, 0.06, 0.25]];
  const k = Math.pow(440 / f, 0.3);
  const T = 3.6;
  const x = arr(T);
  for (const [m, a, t60] of modes) {
    if (f * m > 18000) continue;
    add(x, env(sine(T, () => f * m), { a: 0.0003, t60: t60 * k }), a);
  }
  add(x, noiseBurst(0.004, 121, 3000, 0.002, 0.15));
  tail(x, 0.1);
  return out(x);
}

/** Supersaw pluck with a decaying lowpass. */
function pluck(f) {
  const T = 1.6;
  const det = [-14, -7, 0, 7, 14];
  const side = (s) => {
    const x = arr(T);
    det.forEach((c, i) => { if ((i + s) % 2 === 0 || c === 0) add(x, saw(T, f * Math.pow(2, c / 1200), (i * 0.13 + s * 0.3) % 1), 0.3); });
    let y1 = 0, y2 = 0;
    for (let i = 0; i < x.length; i++) {
      const t = i / R;
      const fc = 350 + 7000 * exp(-t / 0.12);
      const a = 1 - exp((-2 * Math.PI * fc) / R);
      y1 += a * (x[i] - y1);
      y2 += a * (y1 - y2);
      x[i] = y2;
    }
    env(x, { a: 0.001, t60: 1.3 });
    tail(x, 0.05);
    return x;
  };
  return outLR(side(0), side(1));
}

/** Warm supersaw pad – rendered long and looped. */
function pad(f) {
  const T = 5;
  const det = [-18, -11, -5, 0, 5, 11, 18];
  const side = (s) => {
    const x = arr(T);
    det.forEach((c, i) => { if ((i + s) % 2 === 0 || c === 0) add(x, saw(T, f * Math.pow(2, c / 1200), (i * 0.17 + s * 0.41) % 1, 0.0015, 7 + i + s), 0.22); });
    add(x, sine(T, () => f / 2), 0.25);
    bq(x, 'lp', Math.min(2400, f * 9), 0.6);
    bq(x, 'lp', Math.min(4000, f * 14), 0.6);
    return x;
  };
  return outLR(side(0), side(1));
}

/** Clean sub bass (sine with a touch of second harmonic) – looped. */
function sub(f) {
  const T = 2;
  const x = sine(T, () => f);
  add(x, sine(T, () => 2 * f), 0.08);
  drive(x, 1.3);
  return out(x);
}

// ---------------------------------------------------------------- real percussion (VSCO-2-CE)

const VSCO_PERC = {
  'perc.conga': ['Percussion/Conga-HitN_v3_rr1_Sum.wav', 'Percussion/Conga-HitN_v3_rr2_Sum.wav'],
  'perc.conga.low': ['Percussion/Tumba-HitN_v3_rr1_Sum.wav', 'Percussion/Tumba-HitN_v3_rr2_Sum.wav'],
  'perc.conga.high': ['Percussion/Quinto-HitN_v3_rr1_Sum.wav', 'Percussion/Quinto-HitN_v3_rr2_Sum.wav'],
  'perc.claves': ['Percussion/Claves1_Hit_v3_rr1_Sum.wav', 'Percussion/Claves1_Hit_v3_rr2_Sum.wav'],
  'perc.cowbell': ['Percussion/Cowbell1_Hit_v4_rr1_Sum.wav', 'Percussion/Cowbell1_Hit_v4_rr2_Sum.wav'],
  'perc.tambourine': ['Percussion/Tamb1-Hit_v2_rr1_Sum.wav', 'Percussion/Tamb1-Hit_v2_rr2_Sum.wav'],
  'perc.triangle': ['Percussion/Triangle3-Hit_v2_rr1_Sum.wav', 'Percussion/Triangle3-Hit_v2_rr2_Sum.wav'],
  'perc.shaker': ["VSCO 1 Percussion/varWood/Camo's Shaker/shake1.wav", "VSCO 1 Percussion/varWood/Camo's Shaker/shake3.wav", "VSCO 1 Percussion/varWood/Camo's Shaker/shake5.wav"],
  'perc.woodblock': ['VSCO 1 Percussion/varWood/wood_click_f.wav', 'VSCO 1 Percussion/varWood/wood_click_f2.wav'],
  'perc.sleighbells': ['Percussion/Sleighbells_Hit_v1_rr1_Mid.wav', 'Percussion/Sleighbells_Hit_v1_rr2_Mid.wav'],
  'perc.cymbal': ['Percussion/susCymb1-hit_fff_rr1.wav'],
};

// ---------------------------------------------------------------- build

function finishOneShot(ch, { target = db(-0.3), maxLen = 0 } = {}) {
  let c = ch.length === 1 ? [ch[0], ch[0].slice()] : ch;
  const end = tailEnd(c, -70);
  c = slice(c, 0, Math.max(64, end));
  if (maxLen && c[0].length > maxLen * SR) { c = slice(c, 0, Math.round(maxLen * SR)); fade(c, 0, Math.min(0.3, maxLen * 0.3)); }
  else fade(c, 0, 0.005);
  return normalize(c, target);
}

export function buildKits({ outDir, vscoRoot, only }) {
  const res = {};
  const want = (id) => !only || only.includes(id);
  const one = (id, ch, opts = {}) => {
    const rel = `${id.replace(/\./g, '/')}.ogg`;
    const w = writeOpus(path.join(outDir, rel), finishOneShot(ch, opts));
    res[id] = { zones: [{ f: rel, r: 60, lo: 0, hi: 127, vl: 0, vh: 127 }], bytes: w.bytes };
  };
  for (const table of [KICKS, SNARES, HATS, PERCS, FX]) {
    for (const [id, fn] of Object.entries(table)) if (want(id)) one(id, fn());
  }
  // melodic, multi-sampled
  const multi = (id, roots, make, { vel = [1], loop = null, bitrate, peakDb = -1 } = {}) => {
    if (!want(id)) return;
    const zones = [];
    let bytes = 0;
    const rendered = [];
    roots.forEach((root, ri) => {
      vel.forEach((v, vi) => {
        let ch = make(hz(root), v);
        let lp = null;
        if (loop) {
          const ls = Math.round(loop.start * SR);
          const le = Math.round(loop.end * SR);
          const l = makeLoop(ch, ls, le, Math.round(loop.xfade * SR));
          ch = slice(ch, 0, l.end + Math.round(SR * 0.01));
          lp = { ls: l.start / SR, le: l.end / SR };
        } else {
          ch = slice(ch, 0, tailEnd(ch, -70));
          fade(ch, 0, 0.02);
        }
        const lo = ri === 0 ? 0 : Math.floor((roots[ri - 1] + root) / 2) + 1;
        const hi = ri === roots.length - 1 ? 127 : Math.floor((root + roots[ri + 1]) / 2);
        const vl = vi === 0 ? 0 : Math.round((vel[vi - 1] + v) * 63.5) + 1;
        const vh = vi === vel.length - 1 ? 127 : Math.round((v + vel[vi + 1]) * 63.5);
        rendered.push({ ch, root, lo, hi, vl, vh, lp, v });
      });
    });
    // one gain for the instrument, so the level relation between notes stays
    let max = 0;
    for (const r of rendered) max = Math.max(max, peak(r.ch));
    const g = db(peakDb) / max;
    for (const r of rendered) {
      gain(r.ch, g);
      const rel = `${id.replace(/\./g, '/')}/${String(r.root).padStart(3, '0')}-v${r.vh}.ogg`;
      const w = writeOpus(path.join(outDir, rel), r.ch, bitrate ? { bitrate } : {});
      bytes += w.bytes;
      const z = { f: rel, r: r.root, lo: r.lo, hi: r.hi, vl: r.vl, vh: r.vh };
      if (r.lp) { z.ls = +r.lp.ls.toFixed(5); z.le = +r.lp.le.toFixed(5); }
      zones.push(z);
    }
    res[id] = { zones, bytes, loop: !!loop };
  };
  for (const [id, p] of Object.entries(BASSES)) multi(id, BASS_ROOTS, (f) => bass808(f, p), { peakDb: -0.5 });
  multi('keys.epiano', [28, 34, 40, 46, 52, 58, 64, 70, 76, 82, 88, 94], (f, v) => epiano(f, v), { vel: [0.45, 1] });
  multi('mallet.musicbox', [60, 66, 72, 78, 84, 90, 96], (f) => musicbox(f));
  multi('synth.pluck', [36, 42, 48, 54, 60, 66, 72, 78, 84], (f) => pluck(f));
  multi('synth.pad', [36, 42, 48, 54, 60, 66, 72, 78], (f) => pad(f), { loop: { start: 1.2, end: 4.4, xfade: 0.8 }, bitrate: 80 });
  multi('bass.sub', [24, 36, 48], (f) => sub(f), { loop: { start: 0.5, end: 1.5, xfade: 0.2 }, peakDb: -1 });
  // real percussion one-shots with round robin
  if (vscoRoot) {
    for (const [id, files] of Object.entries(VSCO_PERC)) {
      if (!want(id)) continue;
      const loaded = files.map((f) => {
        const ch = readAudio(path.join(vscoRoot, f));
        const s = Math.max(0, onset(ch, 0.01) - Math.round(SR * 0.001));
        return slice(ch, s, ch[0].length);
      });
      let max = 0;
      const cut = loaded.map((c) => finishOneShot(c, { target: 1, maxLen: id === 'perc.cymbal' ? 3 : 1.5 }));
      // keep the level relation between round robins: normalize them together
      for (const c of cut) max = Math.max(max, peak(c));
      const zones = [];
      let bytes = 0;
      cut.forEach((c, i) => {
        gain(c, db(-0.5) / max);
        const rel = `${id.replace(/\./g, '/')}-${i + 1}.ogg`;
        bytes += writeOpus(path.join(outDir, rel), c).bytes;
        const z = { f: rel, r: 60, lo: 0, hi: 127, vl: 0, vh: 127 };
        if (i) z.rr = i;
        zones.push(z);
      });
      res[id] = { zones, bytes, source: 'vsco' };
    }
  }
  for (const [id, e] of Object.entries(res)) console.log(`${id}: ${e.zones.length} zone(s), ${(e.bytes / 1024).toFixed(0)} KB`);
  return res;
}
