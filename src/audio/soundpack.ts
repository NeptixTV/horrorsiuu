// Sound pack 2: more synthesised one-shots and loops (drums, percussion,
// melodic, vocals, FX, funny and game sounds). Everything is generated at
// start-up – no third-party samples.
import { DRUM_BUILDERS } from './instruments/drums';
import { defaultDrum } from '../state/defaults';
import type { DrumParams, DrumType } from '../state/types';
import type { LibrarySample } from './library';

const SR = 44100;

async function render(seconds: number, fn: (ctx: OfflineAudioContext) => void): Promise<AudioBuffer> {
  const ctx = new OfflineAudioContext(2, Math.ceil(SR * seconds), SR);
  fn(ctx);
  return ctx.startRendering();
}

function drumAt(ctx: OfflineAudioContext, type: DrumType, t: number, vel = 1, over: Partial<DrumParams> = {}, dest: AudioNode = ctx.destination) {
  DRUM_BUILDERS[type](ctx, dest, { ...defaultDrum(type), ...over }, t, vel);
}

function noise(ctx: BaseAudioContext, seconds: number, seed = 1) {
  const b = ctx.createBuffer(1, Math.ceil(ctx.sampleRate * seconds), ctx.sampleRate);
  const d = b.getChannelData(0);
  let s = seed * 9301 + 49297;
  for (let i = 0; i < d.length; i++) {
    s = (s * 1664525 + 1013904223) >>> 0;
    d[i] = (s / 4294967296) * 2 - 1;
  }
  const src = ctx.createBufferSource();
  src.buffer = b;
  return src;
}

/** Simple enveloped oscillator. */
function osc(ctx: OfflineAudioContext, type: OscillatorType, f0: number, f1: number, t: number, dur: number, gain: number, dest: AudioNode = ctx.destination, attack = 0.005) {
  const o = ctx.createOscillator();
  o.type = type;
  o.frequency.setValueAtTime(f0, t);
  if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(Math.max(1, f1), t + dur);
  const g = ctx.createGain();
  g.gain.setValueAtTime(0, t);
  g.gain.linearRampToValueAtTime(gain, t + attack);
  g.gain.exponentialRampToValueAtTime(0.0001, t + dur);
  o.connect(g).connect(dest);
  o.start(t);
  o.stop(t + dur + 0.02);
  return o;
}

function bandpass(ctx: BaseAudioContext, f: number, q: number, dest: AudioNode) {
  const b = ctx.createBiquadFilter();
  b.type = 'bandpass';
  b.frequency.value = f;
  b.Q.value = q;
  b.connect(dest);
  return b;
}

function lowpass(ctx: BaseAudioContext, f: number, q: number, dest: AudioNode) {
  const b = ctx.createBiquadFilter();
  b.type = 'lowpass';
  b.frequency.value = f;
  b.Q.value = q;
  b.connect(dest);
  return b;
}

/** Karplus-Strong string. */
function pluckString(freq: number, seconds: number, decay = 0.996, bright = 0.5, seed = 7): Float32Array {
  const len = Math.ceil(SR * seconds);
  const out = new Float32Array(len);
  const period = Math.max(2, Math.round(SR / freq));
  const ring = new Float32Array(period);
  let s = seed;
  for (let i = 0; i < period; i++) {
    s = (s * 1664525 + 1013904223) >>> 0;
    ring[i] = (s / 4294967296) * 2 - 1;
  }
  let idx = 0;
  let prev = 0;
  for (let i = 0; i < len; i++) {
    const next = (idx + 1) % period;
    const v = ring[idx];
    const avg = bright * v + (1 - bright) * 0.5 * (v + ring[next]);
    ring[idx] = decay * (0.5 * (avg + prev));
    prev = ring[idx];
    idx = next;
    out[i] = v;
  }
  return out;
}

function bufferFrom(chans: Float32Array[], gain = 0.6): AudioBuffer {
  const b = new AudioBuffer({ length: chans[0].length, numberOfChannels: 2, sampleRate: SR });
  b.copyToChannel(chans[0].map((v) => v * gain), 0);
  b.copyToChannel((chans[1] ?? chans[0]).map((v) => v * gain), 1);
  return b;
}

function vowel(ctx: OfflineAudioContext, f0: number, f1: number, formants: [number, number, number][], t: number, dur: number, gain = 0.8, vib = 5, dest: AudioNode = ctx.destination) {
  const o = ctx.createOscillator();
  o.type = 'sawtooth';
  o.frequency.setValueAtTime(f0, t);
  if (f1 !== f0) o.frequency.exponentialRampToValueAtTime(f1, t + dur);
  const lfo = ctx.createOscillator();
  lfo.frequency.value = 5.3;
  const lg = ctx.createGain();
  lg.gain.value = vib;
  lfo.connect(lg).connect(o.detune);
  const out = ctx.createGain();
  out.gain.setValueAtTime(0, t);
  out.gain.linearRampToValueAtTime(gain, t + Math.min(0.06, dur * 0.3));
  out.gain.setValueAtTime(gain, t + dur * 0.75);
  out.gain.linearRampToValueAtTime(0, t + dur);
  for (const [f, q, g] of formants) {
    const gg = ctx.createGain();
    gg.gain.value = g;
    o.connect(bandpass(ctx, f, q, gg));
    gg.connect(out);
  }
  out.connect(dest);
  o.start(t);
  lfo.start(t);
  o.stop(t + dur);
  lfo.stop(t + dur);
}

const AH: [number, number, number][] = [[730, 7, 1.4], [1090, 9, 0.8], [2440, 12, 0.3]];
const OH: [number, number, number][] = [[500, 8, 1.5], [800, 9, 0.6], [2500, 12, 0.15]];
const EH: [number, number, number][] = [[530, 7, 1.2], [1850, 10, 0.7], [2500, 12, 0.3]];

const MIDI = (n: number) => 440 * Math.pow(2, (n - 69) / 12);

export const PACK: LibrarySample[] = [
  // drums
  { id: 'builtin:kick-trap', name: 'Trap 808 Kick', category: 'Drums' },
  { id: 'builtin:kick-house', name: 'House Kick', category: 'Drums' },
  { id: 'builtin:kick-dist', name: 'Distorted Kick', category: 'Drums' },
  { id: 'builtin:snare-trap', name: 'Trap Snare', category: 'Drums' },
  { id: 'builtin:snare-lofi', name: 'Lo-Fi Snare', category: 'Drums' },
  { id: 'builtin:snap', name: 'Finger Snap', category: 'Drums' },
  { id: 'builtin:ride', name: 'Ride Cymbal', category: 'Drums' },
  { id: 'builtin:hat-pedal', name: 'Pedal Hat', category: 'Drums' },
  // percussion
  { id: 'builtin:conga-hi', name: 'Conga High', category: 'Percussion' },
  { id: 'builtin:conga-lo', name: 'Conga Low', category: 'Percussion' },
  { id: 'builtin:bongo', name: 'Bongo', category: 'Percussion' },
  { id: 'builtin:tambourine', name: 'Tambourine', category: 'Percussion' },
  { id: 'builtin:triangle', name: 'Triangle', category: 'Percussion' },
  { id: 'builtin:clave', name: 'Clave', category: 'Percussion' },
  { id: 'builtin:woodblock', name: 'Woodblock', category: 'Percussion' },
  // bass
  { id: 'builtin:subdrop', name: 'Sub Drop', category: 'Bass' },
  { id: 'builtin:pluckbass', name: 'Pluck Bass (C)', category: 'Bass', root: 36 },
  { id: 'builtin:wobble', name: 'Wobble Bass (C)', category: 'Bass', root: 36 },
  // melodic
  { id: 'builtin:piano', name: 'Soft Piano (C)', category: 'Melodic', root: 60 },
  { id: 'builtin:bell', name: 'Glass Bell (C)', category: 'Melodic', root: 72 },
  { id: 'builtin:organ', name: 'Organ Chord (C)', category: 'Melodic', root: 60 },
  { id: 'builtin:stab', name: 'Synth Stab (Cm)', category: 'Melodic', root: 60 },
  { id: 'builtin:brass', name: 'Brass Stab (C)', category: 'Melodic', root: 60 },
  { id: 'builtin:guitar', name: 'Guitar Strum (C)', category: 'Melodic', root: 48 },
  // vocals
  { id: 'builtin:vox-hey', name: 'Vox “Hey!”', category: 'Vocals' },
  { id: 'builtin:vox-oh', name: 'Vox “Oh” (C)', category: 'Vocals', root: 60 },
  { id: 'builtin:choir', name: 'Choir Pad (Am)', category: 'Vocals', root: 57 },
  // fx
  { id: 'builtin:siren', name: 'Siren', category: 'FX' },
  { id: 'builtin:airhorn', name: 'Air Horn', category: 'FX' },
  { id: 'builtin:sweep', name: 'Saw Sweep Up', category: 'FX' },
  { id: 'builtin:glitch', name: 'Glitch Burst', category: 'FX' },
  { id: 'builtin:thunder', name: 'Thunder', category: 'FX' },
  // funny
  { id: 'builtin:quack', name: 'Quack', category: 'Funny' },
  { id: 'builtin:fart', name: 'Fart', category: 'Funny' },
  { id: 'builtin:slide-up', name: 'Slide Whistle Up', category: 'Funny' },
  { id: 'builtin:slide-down', name: 'Slide Whistle Down', category: 'Funny' },
  { id: 'builtin:badum', name: 'Ba-Dum-Tss', category: 'Funny' },
  { id: 'builtin:sad-trombone', name: 'Sad Trombone', category: 'Funny' },
  { id: 'builtin:pop', name: 'Cartoon Pop', category: 'Funny' },
  { id: 'builtin:kazoo', name: 'Kazoo (C)', category: 'Funny', root: 60 },
  { id: 'builtin:moo', name: 'Cow Moo', category: 'Funny' },
  // game
  { id: 'builtin:coin', name: 'Coin', category: 'Game' },
  { id: 'builtin:jump', name: 'Jump', category: 'Game' },
  { id: 'builtin:powerup', name: 'Power Up', category: 'Game' },
  { id: 'builtin:gameover', name: 'Game Over', category: 'Game' },
  { id: 'builtin:explosion', name: 'Explosion', category: 'Game' },
  { id: 'builtin:hit', name: 'Hit / Punch', category: 'Game' },
  // loops
  { id: 'builtin:trap-loop', name: 'Trap Loop 140', category: 'Loops' },
  { id: 'builtin:house-loop', name: 'House Loop 124', category: 'Loops' },
  { id: 'builtin:dnb-loop', name: 'DnB Loop 174', category: 'Loops' },
];

export const PACK_GENERATORS: Record<string, () => Promise<AudioBuffer>> = {
  'builtin:kick-trap': () => render(2.2, (c) => drumAt(c, 'kick', 0, 1, { decay: 3, tune: -3, tone: 0.15, snap: 0.3 })),
  'builtin:kick-house': () => render(0.6, (c) => drumAt(c, 'kick', 0, 1, { decay: 0.7, tune: 2, tone: 0.6, snap: 0.7 })),
  'builtin:kick-dist': () => render(0.8, (c) => {
    const sh = c.createWaveShaper();
    sh.curve = Float32Array.from({ length: 1024 }, (_, i) => Math.tanh(((i / 511.5) - 1) * 6));
    const g = c.createGain();
    g.gain.value = 0.6;
    sh.connect(g).connect(c.destination);
    drumAt(c, 'kick', 0, 1, { decay: 1.1, tone: 0.9, snap: 0.8 }, sh);
  }),
  'builtin:snare-trap': () => render(0.5, (c) => drumAt(c, 'snare', 0, 1, { tune: 5, decay: 0.7, tone: 0.9, snap: 0.9 })),
  'builtin:snare-lofi': () => render(0.6, (c) => {
    const lp = lowpass(c, 2800, 0.7, c.destination);
    drumAt(c, 'snare', 0, 1, { tune: -2, decay: 1.2, tone: 0.3, snap: 0.6 }, lp);
  }),
  'builtin:snap': () => render(0.3, (c) => {
    const n = noise(c, 0.3, 3);
    const g = c.createGain();
    g.gain.setValueAtTime(0.9, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 0.07);
    n.connect(bandpass(c, 2600, 3, g));
    g.connect(c.destination);
    n.start(0);
    osc(c, 'sine', 1800, 900, 0, 0.02, 0.3);
  }),
  'builtin:ride': () => render(3, (c) => {
    const hp = c.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 3000;
    hp.connect(c.destination);
    [3.1, 4.7, 6.3, 7.9, 9.4].forEach((m, i) => osc(c, 'square', 420 * m, 420 * m, 0, 2.8, 0.05 / (i + 1), hp, 0.002));
    osc(c, 'sine', 5200, 5200, 0, 1.5, 0.15, c.destination, 0.001);
  }),
  'builtin:hat-pedal': () => render(0.25, (c) => drumAt(c, 'hihat', 0, 0.8, { decay: 0.6, tone: 0.2, snap: 0.2 })),
  'builtin:conga-hi': () => render(0.5, (c) => { osc(c, 'sine', 420, 330, 0, 0.3, 0.9); drumAt(c, 'perc', 0, 0.2); }),
  'builtin:conga-lo': () => render(0.6, (c) => { osc(c, 'sine', 260, 200, 0, 0.42, 0.9); drumAt(c, 'perc', 0, 0.15, { tune: -7 }); }),
  'builtin:bongo': () => render(0.35, (c) => { osc(c, 'sine', 600, 480, 0, 0.18, 0.8); osc(c, 'triangle', 1200, 1000, 0, 0.05, 0.2); }),
  'builtin:tambourine': () => render(0.6, (c) => {
    const hp = c.createBiquadFilter();
    hp.type = 'highpass';
    hp.frequency.value = 6000;
    hp.connect(c.destination);
    for (let k = 0; k < 4; k++) {
      const n = noise(c, 0.4, 10 + k);
      const g = c.createGain();
      const t = k * 0.012;
      g.gain.setValueAtTime(0, t);
      g.gain.linearRampToValueAtTime(0.5, t + 0.002);
      g.gain.exponentialRampToValueAtTime(0.001, t + 0.35);
      n.connect(g).connect(hp);
      n.start(t);
    }
    [7400, 9100, 11200].forEach((f) => osc(c, 'square', f, f, 0, 0.3, 0.03, hp));
  }),
  'builtin:triangle': () => render(3, (c) => { [1760, 4410, 6050].forEach((f, i) => osc(c, 'sine', f, f, 0, 2.8 - i * 0.6, 0.25 / (i + 1), c.destination, 0.001)); }),
  'builtin:clave': () => render(0.25, (c) => osc(c, 'sine', 2500, 2450, 0, 0.12, 0.8, c.destination, 0.001)),
  'builtin:woodblock': () => render(0.3, (c) => { osc(c, 'sine', 900, 850, 0, 0.1, 0.8, c.destination, 0.001); osc(c, 'triangle', 1900, 1900, 0, 0.04, 0.3); }),
  'builtin:subdrop': () => render(2.5, (c) => osc(c, 'sine', 90, 28, 0, 2.4, 1, c.destination, 0.01)),
  'builtin:pluckbass': () => render(1.2, (c) => {
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.Q.value = 6;
    lp.frequency.setValueAtTime(2500, 0);
    lp.frequency.exponentialRampToValueAtTime(180, 0.35);
    lp.connect(c.destination);
    osc(c, 'sawtooth', 65.4, 65.4, 0, 1.1, 0.6, lp);
    osc(c, 'square', 32.7, 32.7, 0, 1.1, 0.3, lp);
  }),
  'builtin:wobble': () => render(2, (c) => {
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.Q.value = 9;
    lp.frequency.value = 500;
    const lfo = c.createOscillator();
    lfo.frequency.value = 4;
    const lg = c.createGain();
    lg.gain.value = 2400;
    lfo.connect(lg).connect(lp.detune);
    lfo.start(0);
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.5, 0.02);
    g.gain.setValueAtTime(0.5, 1.8);
    g.gain.linearRampToValueAtTime(0, 2);
    lp.connect(g).connect(c.destination);
    [-7, 7].forEach((d) => { const o = c.createOscillator(); o.type = 'sawtooth'; o.frequency.value = 65.4; o.detune.value = d; o.connect(lp); o.start(0); });
  }),
  'builtin:piano': async () => {
    const l = pluckString(261.63, 3, 0.998, 0.15, 3);
    const r = pluckString(261.63 * 1.002, 3, 0.998, 0.15, 5);
    const b = bufferFrom([l, r], 0.55);
    // gentle lowpass for a softer, felt-like tone
    const c = new OfflineAudioContext(2, b.length, SR);
    const s = c.createBufferSource();
    s.buffer = b;
    s.connect(lowpass(c, 3200, 0.6, c.destination));
    s.start();
    return c.startRendering();
  },
  'builtin:bell': () => render(4, (c) => { [[1, 0.5], [2.76, 0.25], [5.4, 0.12], [8.93, 0.07]].forEach(([m, g]) => osc(c, 'sine', 523.25 * m, 523.25 * m, 0, 3.8 / Math.sqrt(m), g, c.destination, 0.002)); }),
  'builtin:organ': () => render(3, (c) => {
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.12, 0.05);
    g.gain.setValueAtTime(0.12, 2.7);
    g.gain.linearRampToValueAtTime(0, 3);
    g.connect(c.destination);
    for (const n of [60, 64, 67]) for (const [m, a] of [[1, 1], [2, 0.6], [3, 0.35], [4, 0.25]]) {
      const o = c.createOscillator();
      o.frequency.value = MIDI(n) * m;
      const og = c.createGain();
      og.gain.value = a;
      o.connect(og).connect(g);
      o.start(0);
    }
  }),
  'builtin:stab': () => render(0.8, (c) => {
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.Q.value = 3;
    lp.frequency.setValueAtTime(6000, 0);
    lp.frequency.exponentialRampToValueAtTime(400, 0.5);
    lp.connect(c.destination);
    for (const n of [60, 63, 67, 72]) for (const d of [-9, 9]) { const o = osc(c, 'sawtooth', MIDI(n), MIDI(n), 0, 0.7, 0.08, lp); o.detune.value = d; }
  }),
  'builtin:brass': () => render(1.2, (c) => {
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.Q.value = 2;
    lp.frequency.setValueAtTime(300, 0);
    lp.frequency.linearRampToValueAtTime(3500, 0.08);
    lp.frequency.exponentialRampToValueAtTime(1200, 1);
    lp.connect(c.destination);
    for (const n of [60, 64, 67]) for (const d of [-6, 6]) { const o = osc(c, 'sawtooth', MIDI(n), MIDI(n), 0, 1.1, 0.09, lp, 0.04); o.detune.value = d; }
  }),
  'builtin:guitar': async () => {
    const notes = [48, 52, 55, 60, 64, 67];
    const len = SR * 3;
    const L = new Float32Array(len);
    const R = new Float32Array(len);
    notes.forEach((n, i) => {
      const s = pluckString(MIDI(n), 3, 0.997, 0.4, 11 + i);
      const off = Math.floor(i * 0.018 * SR);
      const pan = (i / (notes.length - 1)) * 0.6 - 0.3;
      for (let k = 0; k + off < len; k++) {
        L[k + off] += s[k] * (0.5 - pan * 0.5) * 0.45;
        R[k + off] += s[k] * (0.5 + pan * 0.5) * 0.45;
      }
    });
    return bufferFrom([L, R], 1);
  },
  'builtin:vox-hey': () => render(0.6, (c) => {
    const n = noise(c, 0.1, 4);
    const ng = c.createGain();
    ng.gain.setValueAtTime(0.5, 0);
    ng.gain.exponentialRampToValueAtTime(0.001, 0.08);
    n.connect(bandpass(c, 2500, 1.5, ng));
    ng.connect(c.destination);
    n.start(0);
    vowel(c, 330, 300, EH, 0.03, 0.4, 0.9, 0);
  }),
  'builtin:vox-oh': () => render(2, (c) => vowel(c, 261.63, 261.63, OH, 0, 2)),
  'builtin:choir': () => render(4, (c) => {
    for (const [n, det] of [[57, -6], [60, 5], [64, -3], [69, 8], [57, 9], [64, 4]]) vowel(c, MIDI(n) * Math.pow(2, det / 1200), MIDI(n) * Math.pow(2, det / 1200), AH, 0, 4, 0.22, 9);
  }),
  'builtin:siren': () => render(3, (c) => {
    const o = c.createOscillator();
    o.type = 'sawtooth';
    o.frequency.value = 800;
    const lfo = c.createOscillator();
    lfo.frequency.value = 0.7;
    const lg = c.createGain();
    lg.gain.value = 1200;
    lfo.connect(lg).connect(o.detune);
    const g = c.createGain();
    g.gain.value = 0.2;
    o.connect(lowpass(c, 3000, 1, g));
    g.connect(c.destination);
    o.start(0);
    lfo.start(0);
  }),
  'builtin:airhorn': () => render(1.6, (c) => {
    const bp = bandpass(c, 1400, 1.2, c.destination);
    [0, 0.18, 0.36].forEach((t, k) => {
      const dur = k === 2 ? 1.1 : 0.14;
      for (const d of [-12, 0, 12]) {
        const o = c.createOscillator();
        o.type = 'sawtooth';
        o.frequency.setValueAtTime(440, t);
        o.frequency.linearRampToValueAtTime(466, t + 0.05);
        o.detune.value = d;
        const g = c.createGain();
        g.gain.setValueAtTime(0, t);
        g.gain.linearRampToValueAtTime(0.25, t + 0.01);
        g.gain.setValueAtTime(0.25, t + dur - 0.02);
        g.gain.linearRampToValueAtTime(0, t + dur);
        o.connect(g).connect(bp);
        o.start(t);
        o.stop(t + dur + 0.01);
      }
    });
  }),
  'builtin:sweep': () => render(3, (c) => {
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.Q.value = 8;
    lp.frequency.setValueAtTime(150, 0);
    lp.frequency.exponentialRampToValueAtTime(12000, 2.9);
    lp.connect(c.destination);
    for (const d of [-15, 0, 15]) { const o = osc(c, 'sawtooth', 110, 110, 0, 3, 0.15, lp, 0.05); o.detune.value = d; }
  }),
  'builtin:glitch': () => render(1, (c) => {
    let t = 0;
    let seed = 42;
    while (t < 0.9) {
      seed = (seed * 1664525 + 1013904223) >>> 0;
      const r = seed / 4294967296;
      const dur = 0.02 + r * 0.06;
      if (r > 0.4) osc(c, 'square', 200 + r * 3000, 100 + r * 800, t, dur, 0.25);
      else { const n = noise(c, dur, seed); const g = c.createGain(); g.gain.value = 0.4; n.connect(bandpass(c, 1000 + r * 8000, 4, g)); g.connect(c.destination); n.start(t); n.stop(t + dur); }
      t += dur + r * 0.03;
    }
  }),
  'builtin:thunder': () => render(5, (c) => {
    const n = noise(c, 5, 9);
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.frequency.setValueAtTime(1800, 0);
    lp.frequency.exponentialRampToValueAtTime(120, 4.5);
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(1, 0.05);
    g.gain.exponentialRampToValueAtTime(0.25, 0.8);
    g.gain.linearRampToValueAtTime(0.5, 1.4);
    g.gain.exponentialRampToValueAtTime(0.001, 4.8);
    n.connect(lp).connect(g).connect(c.destination);
    n.start(0);
  }),
  'builtin:quack': () => render(0.45, (c) => {
    const o = c.createOscillator();
    o.type = 'sawtooth';
    o.frequency.setValueAtTime(520, 0);
    o.frequency.exponentialRampToValueAtTime(380, 0.25);
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.6, 0.02);
    g.gain.setValueAtTime(0.6, 0.18);
    g.gain.exponentialRampToValueAtTime(0.001, 0.3);
    const bp1 = bandpass(c, 1200, 6, g);
    const bp2 = bandpass(c, 2600, 8, g);
    o.connect(bp1);
    o.connect(bp2);
    g.connect(c.destination);
    o.start(0);
    o.stop(0.35);
  }),
  'builtin:fart': () => render(1.1, (c) => {
    const o = c.createOscillator();
    o.type = 'sawtooth';
    o.frequency.setValueAtTime(95, 0);
    o.frequency.linearRampToValueAtTime(70, 0.9);
    const flutter = c.createOscillator();
    flutter.type = 'square';
    flutter.frequency.setValueAtTime(18, 0);
    flutter.frequency.linearRampToValueAtTime(9, 0.9);
    const fg = c.createGain();
    fg.gain.value = 0.5;
    const amp = c.createGain();
    amp.gain.value = 0.5;
    flutter.connect(fg).connect(amp.gain);
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.9, 0.03);
    g.gain.setValueAtTime(0.9, 0.7);
    g.gain.exponentialRampToValueAtTime(0.001, 1);
    o.connect(amp).connect(lowpass(c, 600, 4, g));
    g.connect(c.destination);
    o.start(0);
    flutter.start(0);
  }),
  'builtin:slide-up': () => render(1, (c) => {
    const o = osc(c, 'sine', 500, 2000, 0, 0.9, 0.5, c.destination, 0.02);
    const v = c.createOscillator();
    v.frequency.value = 6;
    const vg = c.createGain();
    vg.gain.value = 25;
    v.connect(vg).connect(o.detune);
    v.start(0);
  }),
  'builtin:slide-down': () => render(1, (c) => {
    const o = osc(c, 'sine', 2000, 450, 0, 0.9, 0.5, c.destination, 0.02);
    const v = c.createOscillator();
    v.frequency.value = 6;
    const vg = c.createGain();
    vg.gain.value = 25;
    v.connect(vg).connect(o.detune);
    v.start(0);
  }),
  'builtin:badum': () => render(2.4, (c) => {
    drumAt(c, 'tom', 0, 0.9, { tune: 3 });
    drumAt(c, 'tom', 0.18, 0.9, { tune: -2 });
    drumAt(c, 'kick', 0.42, 0.9);
    drumAt(c, 'crash', 0.42, 0.8, { decay: 1 });
  }),
  'builtin:sad-trombone': () => render(3, (c) => {
    const notes = [[58, 0, 0.45], [57, 0.5, 0.45], [56, 1, 0.45], [55, 1.5, 1.3]] as const;
    for (const [n, t, d] of notes) {
      const o = c.createOscillator();
      o.type = 'sawtooth';
      o.frequency.value = MIDI(n);
      if (d > 1) {
        const v = c.createOscillator();
        v.frequency.value = 6;
        const vg = c.createGain();
        vg.gain.value = 35;
        v.connect(vg).connect(o.detune);
        v.start(t);
        v.stop(t + d);
      }
      const lp = c.createBiquadFilter();
      lp.type = 'lowpass';
      lp.Q.value = 5;
      lp.frequency.setValueAtTime(400, t);
      lp.frequency.linearRampToValueAtTime(1600, t + 0.15);
      lp.frequency.linearRampToValueAtTime(500, t + d);
      const g = c.createGain();
      g.gain.setValueAtTime(0, t);
      g.gain.linearRampToValueAtTime(0.4, t + 0.05);
      g.gain.setValueAtTime(0.4, t + d - 0.08);
      g.gain.linearRampToValueAtTime(0, t + d);
      o.connect(lp).connect(g).connect(c.destination);
      o.start(t);
      o.stop(t + d);
    }
  }),
  'builtin:pop': () => render(0.25, (c) => { osc(c, 'sine', 300, 1400, 0, 0.08, 0.8, c.destination, 0.001); }),
  'builtin:kazoo': () => render(1.5, (c) => {
    const o = c.createOscillator();
    o.type = 'sawtooth';
    o.frequency.value = 261.63;
    const buzz = c.createOscillator();
    buzz.type = 'square';
    buzz.frequency.value = 261.63 * 0.5;
    const bg = c.createGain();
    bg.gain.value = 0.35;
    buzz.connect(bg);
    const g = c.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.35, 0.04);
    g.gain.setValueAtTime(0.35, 1.3);
    g.gain.linearRampToValueAtTime(0, 1.5);
    const bp = bandpass(c, 1100, 2, g);
    const bp2 = bandpass(c, 2800, 3, g);
    o.connect(bp);
    o.connect(bp2);
    bg.connect(bp);
    g.connect(c.destination);
    o.start(0);
    buzz.start(0);
  }),
  'builtin:moo': () => render(1.6, (c) => vowel(c, 140, 110, [[400, 5, 1.5], [700, 6, 0.6], [2400, 10, 0.1]], 0, 1.5, 0.9, 12)),
  'builtin:coin': () => render(0.6, (c) => { osc(c, 'square', 988, 988, 0, 0.08, 0.2, c.destination, 0.001); osc(c, 'square', 1319, 1319, 0.08, 0.45, 0.2, c.destination, 0.001); }),
  'builtin:jump': () => render(0.4, (c) => { osc(c, 'square', 200, 900, 0, 0.3, 0.2, c.destination, 0.002); }),
  'builtin:powerup': () => render(1, (c) => { [60, 64, 67, 72, 76, 79, 84].forEach((n, i) => osc(c, 'square', MIDI(n), MIDI(n), i * 0.07, 0.12, 0.15, c.destination, 0.001)); }),
  'builtin:gameover': () => render(2, (c) => { [72, 67, 64, 60].forEach((n, i) => osc(c, 'triangle', MIDI(n), MIDI(n) * (i === 3 ? 0.94 : 1), i * 0.3, i === 3 ? 0.9 : 0.28, 0.35, c.destination, 0.005)); }),
  'builtin:explosion': () => render(3, (c) => {
    const n = noise(c, 3, 21);
    const lp = c.createBiquadFilter();
    lp.type = 'lowpass';
    lp.frequency.setValueAtTime(5000, 0);
    lp.frequency.exponentialRampToValueAtTime(80, 2.5);
    const g = c.createGain();
    g.gain.setValueAtTime(1, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 2.8);
    n.connect(lp).connect(g).connect(c.destination);
    n.start(0);
    osc(c, 'sine', 70, 30, 0, 1.5, 0.8);
  }),
  'builtin:hit': () => render(0.4, (c) => {
    drumAt(c, 'kick', 0, 0.8, { decay: 0.4, tone: 0.9 });
    const n = noise(c, 0.1, 5);
    const g = c.createGain();
    g.gain.setValueAtTime(0.7, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 0.09);
    n.connect(bandpass(c, 1500, 0.8, g));
    g.connect(c.destination);
    n.start(0);
  }),
  'builtin:trap-loop': () => {
    const beat = 60 / 140;
    return render(beat * 8, (c) => {
      const s = (st: number) => (st * beat) / 4;
      [0, 10, 16, 22, 26].forEach((st) => drumAt(c, 'kick', s(st), 1, { decay: 2.4, tune: -3, tone: 0.15 }));
      [8, 24].forEach((st) => drumAt(c, 'clap', s(st), 0.9));
      for (let st = 0; st < 32; st += 2) drumAt(c, 'hihat', s(st), 0.5);
      for (let k = 0; k < 6; k++) drumAt(c, 'hihat', s(28) + k * (beat / 12), 0.35 + k * 0.05);
    });
  },
  'builtin:house-loop': () => {
    const beat = 60 / 124;
    return render(beat * 4, (c) => {
      const s = (st: number) => (st * beat) / 4;
      for (let st = 0; st < 16; st += 4) drumAt(c, 'kick', s(st), 1, { decay: 0.8, tune: 1, tone: 0.5 });
      [4, 12].forEach((st) => drumAt(c, 'clap', s(st), 0.8));
      [2, 6, 10, 14].forEach((st) => drumAt(c, 'openhat', s(st), 0.45, { decay: 0.6 }));
      for (let st = 1; st < 16; st += 2) drumAt(c, 'shaker', s(st), 0.3);
    });
  },
  'builtin:dnb-loop': () => {
    const beat = 60 / 174;
    return render(beat * 4, (c) => {
      const s = (st: number) => (st * beat) / 4;
      [0, 10].forEach((st) => drumAt(c, 'kick', s(st), 1, { decay: 0.6 }));
      [4, 12].forEach((st) => drumAt(c, 'snare', s(st), 1, { tone: 0.8, snap: 0.8 }));
      for (let st = 0; st < 16; st += 2) drumAt(c, 'hihat', s(st), 0.4);
      drumAt(c, 'snare', s(15), 0.4);
    });
  },
};
