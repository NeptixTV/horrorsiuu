import type { Channel, DrumParams } from '../../state/types';
import { defaultDrum } from '../../state/defaults';
import { noiseBuffer, type Instrument } from './types';

interface Hit {
  end: number;
  stop(time: number): void;
}

// Open hats are choked by closed hats (per audio context), like a real drum machine.
const openHats = new WeakMap<BaseAudioContext, Set<Hit>>();
function chokeGroup(ctx: BaseAudioContext) {
  let s = openHats.get(ctx);
  if (!s) openHats.set(ctx, (s = new Set()));
  return s;
}

type Builder = (ctx: BaseAudioContext, out: AudioNode, p: DrumParams, t: number, vel: number) => Hit;

function env(ctx: BaseAudioContext, t: number, peak: number, decay: number, attack = 0.001): GainNode {
  const g = ctx.createGain();
  g.gain.setValueAtTime(0, t);
  g.gain.linearRampToValueAtTime(peak, t + attack);
  g.gain.exponentialRampToValueAtTime(0.0001, t + attack + decay);
  return g;
}

function noise(ctx: BaseAudioContext, t: number, dur: number): AudioBufferSourceNode {
  const n = ctx.createBufferSource();
  n.buffer = noiseBuffer(ctx);
  n.loop = true;
  n.loopStart = Math.random() * 1.5;
  n.start(t, Math.random() * 1.5);
  n.stop(t + dur + 0.05);
  return n;
}

function makeHit(nodes: AudioScheduledSourceNode[], end: number, gains: GainNode[]): Hit {
  return {
    end,
    stop(time: number) {
      for (const g of gains) {
        g.gain.cancelScheduledValues(time);
        g.gain.setTargetAtTime(0, time, 0.008);
      }
      for (const n of nodes) { try { n.stop(time + 0.06); } catch { /* ignore */ } }
    },
  };
}

const ratio = (p: DrumParams) => Math.pow(2, p.tune / 12);

const kick: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 0.5 * p.decay;
  const osc = ctx.createOscillator();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(150 * r * (1 + p.tone), t);
  osc.frequency.exponentialRampToValueAtTime(48 * r, t + 0.06 + 0.04 * p.decay);
  osc.frequency.exponentialRampToValueAtTime(40 * r, t + dec);
  const g = env(ctx, t, vel, dec, 0.002);
  const shaper = ctx.createWaveShaper();
  const curve = new Float32Array(256);
  for (let i = 0; i < 256; i++) { const x = i / 127.5 - 1; curve[i] = Math.tanh(x * (1.2 + p.tone * 2)); }
  shaper.curve = curve;
  osc.connect(shaper).connect(g).connect(out);
  osc.start(t);
  osc.stop(t + dec + 0.1);
  // click transient
  const n = noise(ctx, t, 0.02);
  const hp = ctx.createBiquadFilter();
  hp.type = 'highpass';
  hp.frequency.value = 2500;
  const cg = env(ctx, t, vel * p.snap * 0.6, 0.012);
  n.connect(hp).connect(cg).connect(out);
  return makeHit([osc, n], t + dec, [g, cg]);
};

const snare: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 0.22 * p.decay;
  const osc = ctx.createOscillator();
  osc.type = 'triangle';
  osc.frequency.setValueAtTime(240 * r, t);
  osc.frequency.exponentialRampToValueAtTime(170 * r, t + 0.05);
  const og = env(ctx, t, vel * 0.7 * (1 - p.snap * 0.5), 0.12 * p.decay);
  osc.connect(og).connect(out);
  osc.start(t);
  osc.stop(t + 0.3 * p.decay + 0.1);
  const n = noise(ctx, t, dec);
  const hp = ctx.createBiquadFilter();
  hp.type = 'highpass';
  hp.frequency.value = 900 + p.tone * 2500;
  const ng = env(ctx, t, vel * (0.4 + p.snap * 0.6), dec);
  n.connect(hp).connect(ng).connect(out);
  return makeHit([osc, n], t + dec, [og, ng]);
};

const clap: Builder = (ctx, out, p, t, vel) => {
  const dec = 0.28 * p.decay;
  const n = noise(ctx, t, dec + 0.04);
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 900 + p.tone * 1400;
  bp.Q.value = 1.4;
  const g = ctx.createGain();
  g.gain.setValueAtTime(0, t);
  const bursts = [0, 0.011, 0.022];
  for (const b of bursts) {
    g.gain.setValueAtTime(vel, t + b);
    g.gain.exponentialRampToValueAtTime(0.08 * vel, t + b + 0.009);
  }
  g.gain.setValueAtTime(vel * 0.8, t + 0.033);
  g.gain.exponentialRampToValueAtTime(0.0001, t + 0.033 + dec);
  n.connect(bp).connect(g).connect(out);
  return makeHit([n], t + dec, [g]);
};

function metal(ctx: BaseAudioContext, t: number, dur: number, r: number, dest: AudioNode): OscillatorNode[] {
  const ratios = [2, 3, 4.16, 5.43, 6.79, 8.21];
  return ratios.map((m) => {
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.value = 40 * m * r * 1.0;
    o.connect(dest);
    o.start(t);
    o.stop(t + dur + 0.05);
    return o;
  });
}

const hat = (open: boolean): Builder => (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = (open ? 0.42 : 0.055) * p.decay;
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 9000 + p.tone * 3000;
  bp.Q.value = 0.8;
  const hp = ctx.createBiquadFilter();
  hp.type = 'highpass';
  hp.frequency.value = 6500;
  const mg = ctx.createGain();
  mg.gain.value = 0.18 * (1 - p.snap * 0.6);
  const oscs = metal(ctx, t, dec, r, mg);
  mg.connect(bp);
  const n = noise(ctx, t, dec);
  const ng = ctx.createGain();
  ng.gain.value = 0.25 + p.snap * 0.6;
  n.connect(ng).connect(bp);
  const g = env(ctx, t, vel * 0.7, dec);
  bp.connect(hp).connect(g).connect(out);
  const hit = makeHit([...oscs, n], t + dec, [g]);
  const group = chokeGroup(ctx);
  if (open) {
    group.add(hit);
  } else {
    for (const h of group) if (h.end > t) h.stop(t);
    group.clear();
  }
  return hit;
};

const tom: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 0.4 * p.decay;
  const osc = ctx.createOscillator();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(210 * r * (1 + p.tone * 0.5), t);
  osc.frequency.exponentialRampToValueAtTime(125 * r, t + dec);
  const g = env(ctx, t, vel * 0.9, dec);
  osc.connect(g).connect(out);
  osc.start(t);
  osc.stop(t + dec + 0.1);
  const n = noise(ctx, t, 0.03);
  const ng = env(ctx, t, vel * p.snap * 0.3, 0.025);
  const lp = ctx.createBiquadFilter();
  lp.type = 'lowpass';
  lp.frequency.value = 3000;
  n.connect(lp).connect(ng).connect(out);
  return makeHit([osc, n], t + dec, [g, ng]);
};

const perc: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 0.2 * p.decay;
  const osc = ctx.createOscillator();
  osc.type = 'sine';
  osc.frequency.setValueAtTime(420 * r * (1 + p.tone * 0.3), t);
  osc.frequency.exponentialRampToValueAtTime(330 * r, t + 0.04);
  const g = env(ctx, t, vel * 0.8, dec);
  osc.connect(g).connect(out);
  osc.start(t);
  osc.stop(t + dec + 0.1);
  const n = noise(ctx, t, 0.02);
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 3500;
  const ng = env(ctx, t, vel * p.snap * 0.5, 0.015);
  n.connect(bp).connect(ng).connect(out);
  return makeHit([osc, n], t + dec, [g, ng]);
};

const rim: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const osc = ctx.createOscillator();
  osc.type = 'triangle';
  osc.frequency.value = 1700 * r;
  const g = env(ctx, t, vel * 0.7, 0.03 * p.decay);
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 2000 + p.tone * 2000;
  osc.connect(bp).connect(g).connect(out);
  osc.start(t);
  osc.stop(t + 0.1 * p.decay + 0.05);
  return makeHit([osc], t + 0.05, [g]);
};

const cowbell: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 0.35 * p.decay;
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 2640 * r;
  bp.Q.value = 1.5 + p.tone * 3;
  const g = env(ctx, t, vel * 0.5, dec);
  const oscs = [540, 800].map((f) => {
    const o = ctx.createOscillator();
    o.type = 'square';
    o.frequency.value = f * r;
    o.connect(bp);
    o.start(t);
    o.stop(t + dec + 0.1);
    return o;
  });
  bp.connect(g).connect(out);
  return makeHit(oscs, t + dec, [g]);
};

const shaker: Builder = (ctx, out, p, t, vel) => {
  const dec = 0.09 * p.decay;
  const n = noise(ctx, t, dec + 0.02);
  const bp = ctx.createBiquadFilter();
  bp.type = 'bandpass';
  bp.frequency.value = 5000 + p.tone * 5000;
  bp.Q.value = 2;
  const g = env(ctx, t, vel * 0.6, dec, 0.012);
  n.connect(bp).connect(g).connect(out);
  return makeHit([n], t + dec, [g]);
};

const crash: Builder = (ctx, out, p, t, vel) => {
  const r = ratio(p);
  const dec = 1.6 * p.decay;
  const hp = ctx.createBiquadFilter();
  hp.type = 'highpass';
  hp.frequency.value = 4000 + p.tone * 3000;
  const mg = ctx.createGain();
  mg.gain.value = 0.12;
  const oscs = metal(ctx, t, dec, r * 1.3, mg);
  mg.connect(hp);
  const n = noise(ctx, t, dec);
  n.connect(hp);
  const g = env(ctx, t, vel * 0.55, dec, 0.003);
  hp.connect(g).connect(out);
  return makeHit([...oscs, n], t + dec, [g]);
};

export const DRUM_BUILDERS: Record<DrumParams['type'], Builder> = {
  kick, snare, clap, hihat: hat(false), openhat: hat(true), tom, perc, rim, cowbell, shaker, crash,
};

/** A synthesized drum voice (one per channel). Pitch shifts the drum relative to C5 (60). */
export class DrumVoice implements Instrument {
  readonly output: GainNode;
  private p: DrumParams = defaultDrum('kick');
  private hits: Hit[] = [];

  constructor(private ctx: BaseAudioContext) {
    this.output = ctx.createGain();
  }

  update(ch: Channel) {
    if (ch.drum) this.p = ch.drum;
    this.output.gain.value = this.p.level;
  }

  activeVoices() {
    const now = this.ctx.currentTime;
    this.hits = this.hits.filter((h) => h.end > now);
    return this.hits.length;
  }

  noteOn(pitch: number, velocity: number, time: number) {
    const params = pitch === 60 ? this.p : { ...this.p, tune: this.p.tune + (pitch - 60) };
    const hit = DRUM_BUILDERS[params.type](this.ctx, this.output, params, Math.max(time, this.ctx.currentTime), Math.max(0.05, velocity));
    this.activeVoices();
    this.hits.push(hit);
  }

  noteOff() {}

  allNotesOff(time = this.ctx.currentTime) {
    for (const h of this.hits) h.stop(time);
    this.hits = [];
  }

  dispose() {
    this.allNotesOff();
    this.output.disconnect();
  }
}
