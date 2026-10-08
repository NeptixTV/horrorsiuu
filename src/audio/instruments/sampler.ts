import type { Channel, SamplerParams } from '../../state/types';
import { defaultSampler } from '../../state/defaults';
import { assets } from '../assets';
import type { Instrument } from './types';

interface SVoice {
  pitch: number;
  src: AudioBufferSourceNode;
  gain: GainNode;
  end: number;
}

/** Plays an audio asset chromatically (root note = original pitch). */
export class Sampler implements Instrument {
  readonly output: GainNode;
  private p: SamplerParams = defaultSampler();
  private voices: SVoice[] = [];

  constructor(private ctx: BaseAudioContext) {
    this.output = ctx.createGain();
  }

  update(ch: Channel) {
    if (ch.sampler) this.p = ch.sampler;
    this.output.gain.value = this.p.level;
  }

  activeVoices() {
    const now = this.ctx.currentTime;
    this.voices = this.voices.filter((v) => v.end > now);
    return this.voices.length;
  }

  noteOn(pitch: number, velocity: number, time: number, duration?: number) {
    const p = this.p;
    if (!p.assetId) return;
    const buffer = p.reverse ? assets.getReversed(p.assetId) : assets.get(p.assetId);
    if (!buffer) return;
    const src = this.ctx.createBufferSource();
    src.buffer = buffer;
    const rate = Math.pow(2, (pitch - p.rootNote + p.tune) / 12);
    src.playbackRate.value = rate;
    const g = this.ctx.createGain();
    const a = Math.max(0.001, p.attack);
    g.gain.setValueAtTime(0, time);
    g.gain.linearRampToValueAtTime(velocity, time + a);
    src.connect(g).connect(this.output);
    src.start(time);
    const natural = buffer.duration / rate;
    let end = time + natural;
    if (duration !== undefined && duration < natural) {
      const rel = Math.max(0.005, p.release);
      g.gain.setValueAtTime(velocity, time + Math.max(a, duration));
      g.gain.exponentialRampToValueAtTime(0.0001, time + Math.max(a, duration) + rel);
      end = time + Math.max(a, duration) + rel;
      src.stop(end + 0.02);
    }
    src.onended = () => { try { g.disconnect(); } catch { /* ignore */ } };
    this.activeVoices();
    this.voices.push({ pitch, src, gain: g, end });
  }

  noteOff(pitch: number, time: number) {
    const rel = Math.max(0.005, this.p.release);
    for (const v of this.voices) {
      if (v.pitch !== pitch || v.end <= time) continue;
      v.gain.gain.cancelScheduledValues(time);
      v.gain.gain.setTargetAtTime(0, time, rel / 5);
      try { v.src.stop(time + rel + 0.05); } catch { /* ignore */ }
      v.end = time + rel;
    }
  }

  allNotesOff(time = this.ctx.currentTime) {
    for (const v of this.voices) {
      v.gain.gain.cancelScheduledValues(time);
      v.gain.gain.setTargetAtTime(0, time, 0.01);
      try { v.src.stop(time + 0.05); } catch { /* ignore */ }
    }
    this.voices = [];
  }

  dispose() {
    this.allNotesOff();
    this.output.disconnect();
  }
}
