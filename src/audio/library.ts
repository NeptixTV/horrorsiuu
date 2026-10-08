// Built-in sample library. All sounds are synthesised at start-up (no
// third-party samples → no licensing issues) and registered as assets with
// ids "builtin:*".
import { assets } from './assets';
import { DRUM_BUILDERS } from './instruments/drums';
import { defaultDrum } from '../state/defaults';
import type { DrumType } from '../state/types';

export interface LibrarySample {
  id: string;
  name: string;
  category: 'Drums' | 'Bass' | 'FX' | 'Vocals' | 'Loops' | 'Melodic';
  /** root note for melodic samples */
  root?: number;
}

export const LIBRARY: LibrarySample[] = [
  { id: 'builtin:kick-808', name: '808 Kick', category: 'Drums' },
  { id: 'builtin:kick-punch', name: 'Punch Kick', category: 'Drums' },
  { id: 'builtin:snare', name: 'Tight Snare', category: 'Drums' },
  { id: 'builtin:clap', name: 'Big Clap', category: 'Drums' },
  { id: 'builtin:hat', name: 'Closed Hat', category: 'Drums' },
  { id: 'builtin:openhat', name: 'Open Hat', category: 'Drums' },
  { id: 'builtin:cowbell', name: 'Cowbell', category: 'Drums' },
  { id: 'builtin:crash', name: 'Crash', category: 'Drums' },
  { id: 'builtin:sub808', name: '808 Sub (C)', category: 'Bass', root: 36 },
  { id: 'builtin:reese', name: 'Reese Bass (C)', category: 'Bass', root: 36 },
  { id: 'builtin:pluck', name: 'String Pluck (C)', category: 'Melodic', root: 60 },
  { id: 'builtin:pad', name: 'Warm Pad Chord', category: 'Melodic', root: 60 },
  { id: 'builtin:vox-ooh', name: 'Vox "Ooh" (C)', category: 'Vocals', root: 60 },
  { id: 'builtin:vox-aah', name: 'Vox "Aah" (C)', category: 'Vocals', root: 60 },
  { id: 'builtin:vox-chop', name: 'Vox Chop (A)', category: 'Vocals', root: 57 },
  { id: 'builtin:riser', name: 'Noise Riser', category: 'FX' },
  { id: 'builtin:downlifter', name: 'Downlifter', category: 'FX' },
  { id: 'builtin:impact', name: 'Impact Boom', category: 'FX' },
  { id: 'builtin:zap', name: 'Laser Zap', category: 'FX' },
  { id: 'builtin:boing', name: 'Goofy Boing', category: 'FX' },
  { id: 'builtin:vinyl', name: 'Vinyl Crackle Loop', category: 'Loops' },
  { id: 'builtin:beat', name: 'Boom Bap Loop 90', category: 'Loops' },
];

type Render = (ctx: OfflineAudioContext) => void;

const SR = 44100;

async function render(seconds: number, fn: Render): Promise<AudioBuffer> {
  const ctx = new OfflineAudioContext(2, Math.ceil(SR * seconds), SR);
  fn(ctx);
  return ctx.startRendering();
}

function drum(type: DrumType, over: Partial<ReturnType<typeof defaultDrum>> = {}, seconds = 1): Promise<AudioBuffer> {
  return render(seconds, (ctx) => {
    DRUM_BUILDERS[type](ctx, ctx.destination, { ...defaultDrum(type), ...over }, 0, 1);
  });
}

function noiseSource(ctx: BaseAudioContext, seconds: number) {
  const b = ctx.createBuffer(1, Math.ceil(ctx.sampleRate * seconds), ctx.sampleRate);
  const d = b.getChannelData(0);
  for (let i = 0; i < d.length; i++) d[i] = Math.random() * 2 - 1;
  const s = ctx.createBufferSource();
  s.buffer = b;
  return s;
}

/** Formant-filtered saw choir voice: a simple vocal-like source. */
function vowel(ctx: OfflineAudioContext, freq: number, formants: [number, number, number][], start: number, dur: number) {
  const osc = ctx.createOscillator();
  osc.type = 'sawtooth';
  osc.frequency.value = freq;
  const vib = ctx.createOscillator();
  vib.frequency.value = 5.2;
  const vibG = ctx.createGain();
  vibG.gain.setValueAtTime(0, start);
  vibG.gain.linearRampToValueAtTime(18, start + dur * 0.6);
  vib.connect(vibG).connect(osc.detune);
  const out = ctx.createGain();
  out.gain.setValueAtTime(0, start);
  const attack = Math.min(0.08, dur * 0.3);
  out.gain.linearRampToValueAtTime(0.9, start + attack);
  out.gain.setValueAtTime(0.9, Math.max(start + attack, start + dur - Math.min(0.2, dur * 0.4)));
  out.gain.linearRampToValueAtTime(0, start + dur);
  for (const [f, q, g] of formants) {
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.frequency.value = f;
    bp.Q.value = q;
    const gg = ctx.createGain();
    gg.gain.value = g;
    osc.connect(bp).connect(gg).connect(out);
  }
  out.connect(ctx.destination);
  osc.start(start);
  vib.start(start);
  osc.stop(start + dur);
  vib.stop(start + dur);
}

const OOH: [number, number, number][] = [[300, 8, 1.6], [870, 10, 0.5], [2240, 12, 0.15]];
const AAH: [number, number, number][] = [[730, 7, 1.4], [1090, 9, 0.8], [2440, 12, 0.3]];

const GENERATORS: Record<string, () => Promise<AudioBuffer>> = {
  'builtin:kick-808': () => drum('kick', { decay: 2.2, tone: 0.2, snap: 0.2 }, 1.4),
  'builtin:kick-punch': () => drum('kick', { decay: 0.8, tone: 0.8, snap: 0.9 }),
  'builtin:snare': () => drum('snare', { tone: 0.6, snap: 0.7 }),
  'builtin:clap': () => drum('clap', { decay: 1.4 }),
  'builtin:hat': () => drum('hihat', {}, 0.3),
  'builtin:openhat': () => drum('openhat', {}, 0.8),
  'builtin:cowbell': () => drum('cowbell'),
  'builtin:crash': () => drum('crash', {}, 2.5),
  'builtin:sub808': () => render(1.8, (ctx) => {
    const o = ctx.createOscillator();
    o.frequency.setValueAtTime(130.8, 0);
    o.frequency.exponentialRampToValueAtTime(65.4, 0.06);
    const g = ctx.createGain();
    g.gain.setValueAtTime(1, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 1.75);
    const sh = ctx.createWaveShaper();
    sh.curve = Float32Array.from({ length: 512 }, (_, i) => Math.tanh(((i / 255.5) - 1) * 2.2));
    o.connect(sh).connect(g).connect(ctx.destination);
    o.start(0);
  }),
  'builtin:reese': () => render(2, (ctx) => {
    const lp = ctx.createBiquadFilter();
    lp.frequency.value = 700;
    lp.Q.value = 2;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.35, 0.02);
    g.gain.setValueAtTime(0.35, 1.7);
    g.gain.linearRampToValueAtTime(0, 2);
    [-14, 0, 14].forEach((d) => {
      const o = ctx.createOscillator();
      o.type = 'sawtooth';
      o.frequency.value = 65.4;
      o.detune.value = d;
      o.connect(lp);
      o.start(0);
    });
    lp.connect(g).connect(ctx.destination);
  }),
  'builtin:pluck': async () => {
    // Karplus-Strong string
    const len = SR * 2;
    const b = new AudioBuffer({ length: len, numberOfChannels: 2, sampleRate: SR });
    const period = Math.round(SR / 261.63);
    const ring = new Float32Array(period).map(() => Math.random() * 2 - 1);
    const L = b.getChannelData(0);
    const R = b.getChannelData(1);
    let idx = 0;
    for (let i = 0; i < len; i++) {
      const next = (idx + 1) % period;
      const v = ring[idx];
      ring[idx] = 0.996 * 0.5 * (ring[idx] + ring[next]);
      idx = next;
      L[i] = v * 0.6;
      R[i] = v * 0.6;
    }
    return b;
  },
  'builtin:pad': () => render(4, (ctx) => {
    const lp = ctx.createBiquadFilter();
    lp.frequency.value = 1600;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0, 0);
    g.gain.linearRampToValueAtTime(0.16, 0.8);
    g.gain.setValueAtTime(0.16, 3);
    g.gain.linearRampToValueAtTime(0, 4);
    [261.63, 329.63, 392, 523.25].forEach((f, i) => {
      [-8, 8].forEach((d) => {
        const o = ctx.createOscillator();
        o.type = 'sawtooth';
        o.frequency.value = f;
        o.detune.value = d + i;
        const p = ctx.createStereoPanner();
        p.pan.value = d > 0 ? 0.5 : -0.5;
        o.connect(p).connect(lp);
        o.start(0);
      });
    });
    lp.connect(g).connect(ctx.destination);
  }),
  'builtin:vox-ooh': () => render(2, (ctx) => vowel(ctx, 261.63, OOH, 0, 2)),
  'builtin:vox-aah': () => render(2, (ctx) => vowel(ctx, 261.63, AAH, 0, 2)),
  'builtin:vox-chop': () => render(1, (ctx) => {
    vowel(ctx, 220, AAH, 0, 0.18);
    vowel(ctx, 261.63, OOH, 0.25, 0.15);
    vowel(ctx, 329.63, AAH, 0.5, 0.4);
  }),
  'builtin:riser': () => render(4, (ctx) => {
    const n = noiseSource(ctx, 4);
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.Q.value = 3;
    bp.frequency.setValueAtTime(300, 0);
    bp.frequency.exponentialRampToValueAtTime(9000, 3.9);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.05, 0);
    g.gain.exponentialRampToValueAtTime(0.9, 3.9);
    g.gain.linearRampToValueAtTime(0, 4);
    n.connect(bp).connect(g).connect(ctx.destination);
    n.start(0);
  }),
  'builtin:downlifter': () => render(3, (ctx) => {
    const n = noiseSource(ctx, 3);
    const bp = ctx.createBiquadFilter();
    bp.type = 'bandpass';
    bp.Q.value = 2;
    bp.frequency.setValueAtTime(8000, 0);
    bp.frequency.exponentialRampToValueAtTime(200, 3);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.9, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 3);
    n.connect(bp).connect(g).connect(ctx.destination);
    n.start(0);
  }),
  'builtin:impact': () => render(3, (ctx) => {
    const o = ctx.createOscillator();
    o.frequency.setValueAtTime(120, 0);
    o.frequency.exponentialRampToValueAtTime(30, 1.5);
    const g = ctx.createGain();
    g.gain.setValueAtTime(1, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 2.8);
    o.connect(g).connect(ctx.destination);
    o.start(0);
    const n = noiseSource(ctx, 3);
    const lp = ctx.createBiquadFilter();
    lp.frequency.setValueAtTime(6000, 0);
    lp.frequency.exponentialRampToValueAtTime(200, 2);
    const ng = ctx.createGain();
    ng.gain.setValueAtTime(0.6, 0);
    ng.gain.exponentialRampToValueAtTime(0.001, 2.5);
    n.connect(lp).connect(ng).connect(ctx.destination);
    n.start(0);
  }),
  'builtin:zap': () => render(0.5, (ctx) => {
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.setValueAtTime(3000, 0);
    o.frequency.exponentialRampToValueAtTime(80, 0.4);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.4, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 0.45);
    o.connect(g).connect(ctx.destination);
    o.start(0);
  }),
  'builtin:boing': () => render(1, (ctx) => {
    const o = ctx.createOscillator();
    o.type = 'triangle';
    o.frequency.setValueAtTime(180, 0);
    o.frequency.exponentialRampToValueAtTime(520, 0.12);
    o.frequency.exponentialRampToValueAtTime(260, 0.9);
    const vib = ctx.createOscillator();
    vib.frequency.setValueAtTime(14, 0);
    vib.frequency.linearRampToValueAtTime(6, 0.9);
    const vg = ctx.createGain();
    vg.gain.value = 120;
    vib.connect(vg).connect(o.detune);
    const g = ctx.createGain();
    g.gain.setValueAtTime(0.8, 0);
    g.gain.exponentialRampToValueAtTime(0.001, 0.95);
    o.connect(g).connect(ctx.destination);
    o.start(0);
    vib.start(0);
  }),
  'builtin:vinyl': async () => {
    const len = SR * 4;
    const b = new AudioBuffer({ length: len, numberOfChannels: 2, sampleRate: SR });
    for (let c = 0; c < 2; c++) {
      const d = b.getChannelData(c);
      let lp = 0;
      for (let i = 0; i < len; i++) {
        lp = lp * 0.97 + (Math.random() * 2 - 1) * 0.03;
        d[i] = lp * 0.25 + (Math.random() < 0.0009 ? (Math.random() * 2 - 1) * 0.6 : 0);
      }
    }
    return b;
  },
  'builtin:beat': () => {
    const bpm = 90;
    const beat = 60 / bpm;
    return render(beat * 4, (ctx) => {
      const k = { ...defaultDrum('kick'), decay: 1.3 };
      const s = { ...defaultDrum('snare'), tone: 0.4 };
      const h = defaultDrum('hihat');
      const at = (step: number) => step * beat / 4;
      [0, 7, 10].forEach((st) => DRUM_BUILDERS.kick(ctx, ctx.destination, k, at(st), 1));
      [4, 12].forEach((st) => DRUM_BUILDERS.snare(ctx, ctx.destination, s, at(st), 0.9));
      for (let st = 0; st < 16; st += 2) DRUM_BUILDERS.hihat(ctx, ctx.destination, h, at(st), st % 4 === 0 ? 0.7 : 0.45);
    });
  },
};

/** Renders all built-in samples into the asset registry. */
export async function buildLibrary(onProgress?: (done: number, total: number) => void) {
  let done = 0;
  for (const s of LIBRARY) {
    if (assets.has(s.id)) continue;
    try {
      const buf = await GENERATORS[s.id]();
      assets.add(s.id, buf, s.name);
    } catch (err) {
      console.warn('[GoofyStudio] could not render', s.id, err);
    }
    onProgress?.(++done, LIBRARY.length);
  }
}
