// DreamChord sound engine: a warm polyphonic synth made for chords.
//
// Voice:  wave A + 2 detuned wave B + sub sine + breath noise → low-pass
//         filter (with envelope) → ADSR amp.
// Shared: pitch bend / vibrato and filter offsets (joystick), then the
//         built-in effects: tape (wow, flutter, saturation, tone, hiss) →
//         stereo chorus → ping-pong delay + reverb sends → volume.
// The channel output then goes through the normal mixer like every other
// instrument, so the sequencer, mixer and export treat it the same way.
import type { Channel, DreamParams, DreamWave } from '../../state/types';
import { defaultDream } from '../../state/defaults';
import { normalizeDream } from '../../state/normalize';
import { midiToFreq } from '../../midi/music';
import { makeImpulse } from '../effects/reverb';
import { clampFinite } from '../safety';
import { holdAt, noiseBuffer, type Instrument } from './types';

const MAX_VOICES = 18;

// additive tables (index = harmonic number) for the non-basic waves
const TABLES: Record<'glass' | 'piano' | 'organ' | 'choir', number[]> = {
  glass: [0, 1, 0.06, 0.45, 0.03, 0.28, 0, 0.14, 0, 0.08, 0, 0.04],
  piano: [0, 1, 0.6, 0.36, 0.25, 0.17, 0.11, 0.07, 0.05, 0.03, 0.02],
  organ: [0, 1, 0.85, 0.5, 0.7, 0, 0.38, 0, 0.32, 0, 0, 0, 0.15],
  choir: [0, 0.55, 1, 0.85, 0.5, 0.34, 0.2, 0.12, 0.07, 0.04],
};

const waveCache = new WeakMap<BaseAudioContext, Map<string, PeriodicWave>>();

function setWave(ctx: BaseAudioContext, osc: OscillatorNode, wave: DreamWave) {
  if (wave === 'sine' || wave === 'triangle' || wave === 'sawtooth' || wave === 'square') {
    osc.type = wave;
    return;
  }
  let m = waveCache.get(ctx);
  if (!m) waveCache.set(ctx, (m = new Map()));
  let pw = m.get(wave);
  if (!pw) {
    const t = TABLES[wave];
    const real = new Float32Array(t.length);
    const imag = Float32Array.from(t);
    pw = ctx.createPeriodicWave(real, imag);
    m.set(wave, pw);
  }
  osc.setPeriodicWave(pw);
}

function satCurve(drive: number): Float32Array<ArrayBuffer> {
  const n = 1024;
  const c = new Float32Array(n);
  const k = 0.6 + drive * 2.6;
  const norm = Math.tanh(k);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    c[i] = Math.tanh(k * x) / norm;
  }
  return c;
}

const DELAY_BEATS: Record<DreamParams['delayTime'], number> = { '1/4': 1, '1/8': 0.5, '1/8d': 0.75, '1/16': 0.25 };

interface DVoice {
  pitch: number;
  start: number;
  end: number;
  releaseTime: number;
  sources: AudioScheduledSourceNode[];
  release(t: number): void;
  cleanup(): void;
}

export class DreamSynth implements Instrument {
  readonly output: GainNode;
  private p: DreamParams = defaultDream();
  private voices: DVoice[] = [];
  private bpm = 120;
  // shared modulation
  private pitchMod: ConstantSourceNode;
  private cutMod: ConstantSourceNode;
  private vibLfo: OscillatorNode;
  private vibGain: GainNode;
  private extraVib = 0;
  // bus / effects
  private bus: GainNode;
  private tapeDelay: DelayNode;
  private wow: OscillatorNode;
  private wowGain: GainNode;
  private flutter: OscillatorNode;
  private flutterGain: GainNode;
  private sat: WaveShaperNode;
  private tapeTone: BiquadFilterNode;
  private hiss: AudioBufferSourceNode;
  private hissGain: GainNode;
  private chorusDry: GainNode;
  private chorusWet: GainNode;
  private chL: DelayNode;
  private chR: DelayNode;
  private chLfo: OscillatorNode;
  private chDepthL: GainNode;
  private chDepthR: GainNode;
  private post: GainNode;
  private delaySend: GainNode;
  private dl: DelayNode;
  private dr: DelayNode;
  private dfb: GainNode;
  private dTone: BiquadFilterNode;
  private reverbSend: GainNode;
  private convolver: ConvolverNode;
  private revTone: BiquadFilterNode;
  private irKey = '';
  private irTimer: ReturnType<typeof setTimeout> | null = null;
  private satKey = -1;
  private nodes: AudioNode[] = [];

  constructor(private ctx: BaseAudioContext) {
    const t = <T extends AudioNode>(n: T) => { this.nodes.push(n); return n; };
    this.output = t(ctx.createGain());
    this.bus = t(ctx.createGain());

    this.pitchMod = ctx.createConstantSource();
    this.pitchMod.offset.value = 0;
    this.cutMod = ctx.createConstantSource();
    this.cutMod.offset.value = 0;
    this.vibLfo = ctx.createOscillator();
    this.vibLfo.frequency.value = 5.2;
    this.vibGain = t(ctx.createGain());
    this.vibGain.gain.value = 0;
    this.vibLfo.connect(this.vibGain).connect(this.pitchMod.offset);

    // tape: modulated short delay (wow & flutter) → saturation → tone; hiss on top
    this.tapeDelay = t(ctx.createDelay(0.06));
    this.tapeDelay.delayTime.value = 0.012;
    this.wow = ctx.createOscillator();
    this.wow.frequency.value = 0.55;
    this.wowGain = t(ctx.createGain());
    this.flutter = ctx.createOscillator();
    this.flutter.frequency.value = 6.3;
    this.flutterGain = t(ctx.createGain());
    this.wow.connect(this.wowGain).connect(this.tapeDelay.delayTime);
    this.flutter.connect(this.flutterGain).connect(this.tapeDelay.delayTime);
    this.sat = t(ctx.createWaveShaper());
    this.sat.oversample = '2x';
    this.tapeTone = t(ctx.createBiquadFilter());
    this.tapeTone.type = 'lowpass';
    this.tapeTone.Q.value = -3.0103;
    this.hiss = ctx.createBufferSource();
    this.hiss.buffer = noiseBuffer(ctx);
    this.hiss.loop = true;
    const hissHp = t(ctx.createBiquadFilter());
    hissHp.type = 'highpass';
    hissHp.frequency.value = 2500;
    const hissLp = t(ctx.createBiquadFilter());
    hissLp.type = 'lowpass';
    hissLp.frequency.value = 9000;
    this.hissGain = t(ctx.createGain());
    this.hissGain.gain.value = 0;
    this.hiss.connect(hissHp).connect(hissLp).connect(this.hissGain);

    this.bus.connect(this.tapeDelay).connect(this.sat).connect(this.tapeTone);
    const tapeOut = t(ctx.createGain());
    this.tapeTone.connect(tapeOut);
    this.hissGain.connect(tapeOut);

    // chorus: two modulated delays, hard panned
    this.chorusDry = t(ctx.createGain());
    this.chorusWet = t(ctx.createGain());
    this.chL = t(ctx.createDelay(0.05));
    this.chR = t(ctx.createDelay(0.05));
    this.chL.delayTime.value = 0.013;
    this.chR.delayTime.value = 0.019;
    this.chLfo = ctx.createOscillator();
    this.chLfo.frequency.value = 0.42;
    this.chDepthL = t(ctx.createGain());
    this.chDepthR = t(ctx.createGain());
    this.chLfo.connect(this.chDepthL).connect(this.chL.delayTime);
    this.chLfo.connect(this.chDepthR).connect(this.chR.delayTime);
    const panL = t(ctx.createStereoPanner());
    panL.pan.value = -0.9;
    const panR = t(ctx.createStereoPanner());
    panR.pan.value = 0.9;
    this.post = t(ctx.createGain());
    tapeOut.connect(this.chorusDry).connect(this.post);
    tapeOut.connect(this.chL).connect(panL).connect(this.chorusWet);
    tapeOut.connect(this.chR).connect(panR).connect(this.chorusWet);
    this.chorusWet.connect(this.post);

    // ping-pong delay send (Butterworth tone filter → always stable)
    this.delaySend = t(ctx.createGain());
    this.dl = t(ctx.createDelay(3));
    this.dr = t(ctx.createDelay(3));
    this.dfb = t(ctx.createGain());
    this.dfb.gain.value = 0.42;
    const dfb2 = t(ctx.createGain());
    dfb2.gain.value = 0.42;
    this.dTone = t(ctx.createBiquadFilter());
    this.dTone.type = 'lowpass';
    this.dTone.frequency.value = 4200;
    this.dTone.Q.value = -3.0103;
    const dpanL = t(ctx.createStereoPanner());
    dpanL.pan.value = -0.7;
    const dpanR = t(ctx.createStereoPanner());
    dpanR.pan.value = 0.7;
    this.post.connect(this.delaySend).connect(this.dl);
    this.dl.connect(this.dTone).connect(this.dfb).connect(this.dr);
    this.dr.connect(dfb2).connect(this.dl);
    this.dl.connect(dpanL).connect(this.output);
    this.dr.connect(dpanR).connect(this.output);

    // reverb send
    this.reverbSend = t(ctx.createGain());
    this.convolver = t(ctx.createConvolver());
    this.revTone = t(ctx.createBiquadFilter());
    this.revTone.type = 'lowpass';
    this.revTone.frequency.value = 8500;
    this.post.connect(this.reverbSend).connect(this.convolver).connect(this.revTone).connect(this.output);
    this.post.connect(this.output);

    for (const s of [this.pitchMod, this.cutMod, this.vibLfo, this.wow, this.flutter, this.chLfo]) s.start();
    this.hiss.start(0, Math.random() * 1.5);
    this.apply();
  }

  update(ch: Channel) {
    if (!ch.dream) return;
    this.p = normalizeDream(ch.dream);
    this.apply();
  }

  setTempo(bpm: number) {
    if (!Number.isFinite(bpm) || bpm <= 0) return;
    this.bpm = bpm;
    this.applyDelayTime();
  }

  /** Joystick: pitch bend in semitones. */
  setBend(semis: number, glide = 0.04) {
    const v = clampFinite(semis, -12, 12, 0) * 100;
    this.pitchMod.offset.setTargetAtTime(v, this.ctx.currentTime, glide);
  }

  /** Joystick: filter offset in octaves (−3..3). */
  setFilterMod(octaves: number) {
    this.cutMod.offset.setTargetAtTime(clampFinite(octaves, -4, 4, 0) * 1200, this.ctx.currentTime, 0.03);
  }

  /** Joystick: extra vibrato 0..1 on top of the preset. */
  setExpression(v: number) {
    this.extraVib = clampFinite(v, 0, 1, 0);
    this.applyVibrato();
  }

  private applyVibrato() {
    const depth = Math.min(1, this.p.vibrato + this.extraVib) * 28; // cents
    this.vibGain.gain.setTargetAtTime(depth, this.ctx.currentTime, 0.08);
  }

  private applyDelayTime() {
    const t = Math.min(2.9, (60 / this.bpm) * DELAY_BEATS[this.p.delayTime]);
    this.dl.delayTime.setTargetAtTime(t, this.ctx.currentTime, 0.05);
    this.dr.delayTime.setTargetAtTime(t, this.ctx.currentTime, 0.05);
  }

  private apply() {
    const p = this.p;
    const now = this.ctx.currentTime;
    const set = (a: AudioParam, v: number, tc = 0.03) => {
      if (this.ctx instanceof OfflineAudioContext && now === 0) a.value = v;
      else a.setTargetAtTime(v, now, tc);
    };
    set(this.output.gain, p.volume);
    this.applyVibrato();
    // tape
    set(this.wowGain.gain, p.tape * 0.0028);
    set(this.flutterGain.gain, p.tape * 0.00035);
    set(this.tapeTone.frequency, 18500 - p.tape * 12500);
    const satKey = Math.round(p.tape * 50);
    if (satKey !== this.satKey) {
      this.satKey = satKey;
      this.sat.curve = satCurve(p.tape);
    }
    // chorus
    set(this.chDepthL.gain, p.chorus * 0.0035);
    set(this.chDepthR.gain, -p.chorus * 0.0035);
    set(this.chorusWet.gain, p.chorus * 0.75);
    set(this.chorusDry.gain, 1 - p.chorus * 0.35);
    // sends
    set(this.delaySend.gain, p.delay * 0.6);
    set(this.reverbSend.gain, p.reverb * 0.9);
    this.applyDelayTime();
    const key = `${p.space.toFixed(2)}`;
    if (key !== this.irKey) {
      this.irKey = key;
      const build = () => {
        this.convolver.buffer = makeImpulse(this.ctx, 0.3 + p.space * 0.7, 1.4 + p.space * 6.5);
      };
      if (this.irTimer) clearTimeout(this.irTimer);
      if (!this.convolver.buffer || this.ctx instanceof OfflineAudioContext) build();
      else this.irTimer = setTimeout(build, 150);
    }
  }

  activeVoices() {
    const now = this.ctx.currentTime;
    this.voices = this.voices.filter((v) => v.end > now);
    return this.voices.length;
  }

  private updateHiss(at: number) {
    const active = this.voices.some((v) => v.end > at);
    const target = active ? this.p.tape * this.p.tape * 0.014 : 0;
    this.hissGain.gain.setTargetAtTime(target, Math.max(at, this.ctx.currentTime), active ? 0.05 : 0.6);
  }

  noteOn(pitch: number, velocity: number, time: number, duration?: number) {
    if (!Number.isFinite(pitch) || !Number.isFinite(time)) return;
    const ctx = this.ctx;
    const p = this.p;
    velocity = clampFinite(velocity, 0, 1, 0.8);
    if (duration !== undefined && !(duration > 0 && Number.isFinite(duration))) duration = 0.05;
    this.activeVoices();
    while (this.voices.length >= MAX_VOICES) this.voices.shift()?.release(time);

    const freq = midiToFreq(Math.max(0, Math.min(127, pitch)));
    const filter = ctx.createBiquadFilter();
    filter.type = 'lowpass';
    filter.frequency.value = Math.min(18000, p.cutoff);
    filter.Q.value = p.resonance;
    this.cutMod.connect(filter.detune);
    const vca = ctx.createGain();
    vca.gain.value = 0;
    filter.connect(vca).connect(this.bus);

    const sources: AudioScheduledSourceNode[] = [];
    const detunes: AudioParam[] = [];
    const osc = (wave: DreamWave, f: number, cents: number, level: number) => {
      if (level <= 0.001) return;
      const o = ctx.createOscillator();
      setWave(ctx, o, wave);
      o.frequency.value = f;
      o.detune.value = cents;
      this.pitchMod.connect(o.detune);
      detunes.push(o.detune);
      const g = ctx.createGain();
      g.gain.value = level;
      o.connect(g).connect(filter);
      o.start(time);
      sources.push(o);
    };
    osc(p.waveA, freq, 0, (1 - p.mixB) * 0.85);
    osc(p.waveB, freq, p.detune, p.mixB * 0.5);
    osc(p.waveB, freq, -p.detune, p.mixB * 0.5);
    osc('sine', freq / 2, 0, p.sub * 0.5);
    if (p.air > 0.01) {
      const n = ctx.createBufferSource();
      n.buffer = noiseBuffer(ctx);
      n.loop = true;
      const bp = ctx.createBiquadFilter();
      bp.type = 'bandpass';
      bp.frequency.value = Math.min(9000, Math.max(700, freq * 3));
      bp.Q.value = 1.1;
      const ng = ctx.createGain();
      ng.gain.value = p.air * 0.35;
      n.connect(bp).connect(ng).connect(filter);
      n.start(time, Math.random() * 1.5);
      sources.push(n);
    }
    if (sources.length === 0) return;

    const peak = Math.max(0.0001, velocity) * 0.15;
    const a = Math.max(0.003, p.attack);
    const d = Math.max(0.01, p.decay);
    const s = p.sustain;
    const rel = Math.max(0.02, p.release);
    const envAmt = p.filterEnv * 3600;
    const g = vca.gain;
    g.setValueAtTime(0, time);
    filter.detune.setValueAtTime(0, time);
    const envAt = (dt: number) => (dt < a ? (peak * dt) / a : peak * s + (peak - peak * s) * Math.exp(-(dt - a) / (d / 4)));
    if (duration !== undefined && duration < a) {
      g.linearRampToValueAtTime(envAt(duration), time + duration);
    } else {
      g.linearRampToValueAtTime(peak, time + a);
      g.setTargetAtTime(peak * s, time + a, d / 4);
      filter.detune.linearRampToValueAtTime(envAmt, time + a);
      filter.detune.setTargetAtTime(envAmt * s * 0.6, time + a, d / 3);
    }

    let cleaned = false;
    const cleanup = () => {
      if (cleaned) return;
      cleaned = true;
      try { this.cutMod.disconnect(filter.detune); } catch { /* ignore */ }
      for (const dt of detunes) { try { this.pitchMod.disconnect(dt); } catch { /* ignore */ } }
      try { vca.disconnect(); } catch { /* ignore */ }
      try { filter.disconnect(); } catch { /* ignore */ }
      this.updateHiss(this.ctx.currentTime);
    };
    sources[0].onended = cleanup;

    const voice: DVoice = {
      pitch, start: time, end: Infinity, releaseTime: Infinity, sources, cleanup,
      release: (rt: number) => {
        const t = Math.max(rt, voice.start);
        if (t >= voice.releaseTime) return;
        voice.releaseTime = t;
        holdAt(g, t, envAt(t - voice.start));
        g.setTargetAtTime(0, t, rel / 5);
        voice.end = t + rel + 0.1;
        for (const src of sources) { try { src.stop(voice.end); } catch { /* ignore */ } }
      },
    };
    this.voices.push(voice);
    this.updateHiss(time);
    if (duration !== undefined) voice.release(time + duration);
  }

  noteOff(pitch: number, time: number) {
    for (const v of this.voices) if (v.pitch === pitch && v.releaseTime === Infinity) v.release(time);
  }

  allNotesOff(time = this.ctx.currentTime) {
    for (const v of this.voices) {
      if (v.start > time) {
        for (const s of v.sources) { try { s.stop(time); } catch { /* ignore */ } }
        v.cleanup();
      } else {
        v.release(time);
      }
    }
    this.voices = [];
  }

  dispose() {
    this.allNotesOff();
    if (this.irTimer) clearTimeout(this.irTimer);
    for (const s of [this.pitchMod, this.cutMod, this.vibLfo, this.wow, this.flutter, this.chLfo, this.hiss]) {
      try { s.stop(); } catch { /* ignore */ }
      try { s.disconnect(); } catch { /* ignore */ }
    }
    for (const n of this.nodes) { try { n.disconnect(); } catch { /* ignore */ } }
  }
}
