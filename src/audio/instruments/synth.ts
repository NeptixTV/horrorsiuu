import type { Channel, OscParams, SynthParams } from '../../state/types';
import { midiToFreq } from '../../midi/music';
import { defaultSynth } from '../../state/defaults';
import { holdAt, type Instrument } from './types';

const MAX_VOICES = 24;

interface Voice {
  pitch: number;
  start: number;
  end: number;
  releaseTime: number;
  oscs: OscillatorNode[];
  filter: BiquadFilterNode;
  vca: GainNode;
  peak: number;
  release(time: number): void;
}

/** "Goofy Synth": 2 oscillators (+unison), multimode filter with envelope, ADSR and LFO. */
export class Synth implements Instrument {
  readonly output: GainNode;
  private tremolo: GainNode;
  private lfo: OscillatorNode;
  private lfoPitch: GainNode;
  private lfoFilter: GainNode;
  private lfoAmp: GainNode;
  private p: SynthParams = defaultSynth();
  private voices: Voice[] = [];
  private lastFreq = 0;

  constructor(private ctx: BaseAudioContext) {
    this.output = ctx.createGain();
    this.tremolo = ctx.createGain();
    this.tremolo.connect(this.output);
    this.lfo = ctx.createOscillator();
    this.lfoPitch = ctx.createGain();
    this.lfoFilter = ctx.createGain();
    this.lfoAmp = ctx.createGain();
    this.lfo.connect(this.lfoPitch);
    this.lfo.connect(this.lfoFilter);
    this.lfo.connect(this.lfoAmp);
    this.lfoAmp.connect(this.tremolo.gain);
    this.lfo.start();
  }

  update(ch: Channel) {
    if (!ch.synth) return;
    this.p = ch.synth;
    const p = this.p;
    this.lfo.type = p.lfoWave;
    this.lfo.frequency.value = p.lfoRate;
    this.lfoPitch.gain.value = p.lfoTarget === 'pitch' ? p.lfoDepth * 100 : 0;
    this.lfoFilter.gain.value = p.lfoTarget === 'filter' ? p.lfoDepth * 2400 : 0;
    this.lfoAmp.gain.value = p.lfoTarget === 'amp' ? p.lfoDepth * 0.5 : 0;
    this.tremolo.gain.value = p.lfoTarget === 'amp' ? 1 - p.lfoDepth * 0.5 : 1;
    this.output.gain.value = p.volume;
  }

  activeVoices() {
    const now = this.ctx.currentTime;
    this.voices = this.voices.filter((v) => v.end > now);
    return this.voices.length;
  }

  noteOn(pitch: number, velocity: number, time: number, duration?: number) {
    const ctx = this.ctx;
    const p = this.p;
    this.activeVoices();
    if (this.voices.length >= MAX_VOICES) {
      const oldest = this.voices.shift();
      oldest?.release(time);
    }
    const freq = midiToFreq(pitch);
    const filter = ctx.createBiquadFilter();
    filter.type = p.filterType;
    filter.frequency.value = p.cutoff;
    filter.Q.value = p.resonance;
    this.lfoFilter.connect(filter.detune);
    const vca = ctx.createGain();
    vca.gain.value = 0;
    filter.connect(vca).connect(this.tremolo);

    const oscs: OscillatorNode[] = [];
    const addOsc = (o: OscParams, extraDetune: number, level: number) => {
      if (o.level <= 0.001 || level <= 0) return;
      const osc = ctx.createOscillator();
      osc.type = o.wave;
      const f = freq * Math.pow(2, o.octave + o.semi / 12);
      if (p.glide > 0.001 && this.lastFreq > 0) {
        const from = this.lastFreq * Math.pow(2, o.octave + o.semi / 12);
        osc.frequency.setValueAtTime(from, time);
        osc.frequency.exponentialRampToValueAtTime(f, time + p.glide);
      } else {
        osc.frequency.value = f;
      }
      osc.detune.value = o.detune + extraDetune;
      this.lfoPitch.connect(osc.detune);
      const g = ctx.createGain();
      g.gain.value = o.level * level;
      osc.connect(g).connect(filter);
      osc.start(time);
      oscs.push(osc);
    };
    addOsc(p.osc1, 0, 1);
    if (p.unisonDetune > 0) {
      addOsc(p.osc1, p.unisonDetune, 0.55);
      addOsc(p.osc1, -p.unisonDetune, 0.55);
    }
    addOsc(p.osc2, 0, 1);
    this.lastFreq = freq;

    const peak = Math.max(0.0001, velocity) * 0.35;
    const a = Math.max(0.002, p.attack);
    const d = Math.max(0.005, p.decay);
    const s = p.sustain;
    const rel = Math.max(0.005, p.release);
    const envAmt = p.filterEnv * 4800;

    const g = vca.gain;
    g.setValueAtTime(0, time);
    filter.detune.setValueAtTime(0, time);
    const envAt = (dt: number) => (dt < a ? (peak * dt) / a : peak * s + (peak - peak * s) * Math.exp(-(dt - a) / (d / 4)));

    if (duration !== undefined && duration < a) {
      g.linearRampToValueAtTime(envAt(duration), time + duration);
      filter.detune.linearRampToValueAtTime((envAmt * duration) / a, time + duration);
    } else {
      g.linearRampToValueAtTime(peak, time + a);
      g.setTargetAtTime(peak * s, time + a, d / 4);
      filter.detune.linearRampToValueAtTime(envAmt, time + a);
      filter.detune.setTargetAtTime(envAmt * s, time + a, d / 4);
    }

    const voice: Voice = {
      pitch, start: time, end: Infinity, releaseTime: Infinity, oscs, filter, vca, peak,
      release: (rt: number) => {
        const t = Math.max(rt, voice.start);
        if (t >= voice.releaseTime) return;
        voice.releaseTime = t;
        holdAt(g, t, envAt(t - voice.start));
        g.setTargetAtTime(0, t, rel / 5);
        holdAt(filter.detune, t, 0);
        filter.detune.setTargetAtTime(0, t, rel / 5);
        voice.end = t + rel + 0.05;
        for (const o of oscs) { try { o.stop(voice.end); } catch { /* ignore */ } }
        if (oscs[0]) oscs[0].onended = () => {
          try { vca.disconnect(); filter.disconnect(); } catch { /* ignore */ }
        };
      },
    };
    if (oscs.length === 0) return;
    this.voices.push(voice);
    if (duration !== undefined) voice.release(time + duration);
  }

  noteOff(pitch: number, time: number) {
    for (const v of this.voices) if (v.pitch === pitch && v.releaseTime === Infinity) v.release(time);
  }

  allNotesOff(time = this.ctx.currentTime) {
    for (const v of this.voices) {
      if (v.start > time) {
        // never started: hard stop
        for (const o of v.oscs) { try { o.stop(time); } catch { /* ignore */ } }
      } else {
        v.release(time);
      }
    }
    this.voices = [];
  }

  dispose() {
    this.allNotesOff();
    this.lfo.stop();
    this.output.disconnect();
  }
}
