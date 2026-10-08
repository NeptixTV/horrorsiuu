// The audio graph for one project: mixer strips with insert chains, sends and
// routing, plus one instrument host per channel. Works with any
// BaseAudioContext, so the same code drives live playback and offline export.
import type { Channel, EffectInstance, MixerTrack, Project } from '../state/types';
import { MASTER_ID } from '../state/types';
import { audibleTracks, wouldCycle } from '../state/routing';
import { createEffect } from './effects';
import { EffectSlot, setParam, type EffectEnv } from './effects/base';
import { createInstrument, type Instrument } from './instruments';

export interface Levels {
  peakL: number;
  peakR: number;
  rmsL: number;
  rmsR: number;
}

const meterBuf = new Float32Array(1024);

function measure(an: AnalyserNode): [number, number] {
  an.getFloatTimeDomainData(meterBuf);
  let peak = 0;
  let sum = 0;
  for (let i = 0; i < meterBuf.length; i++) {
    const v = meterBuf[i];
    const a = v < 0 ? -v : v;
    if (a > peak) peak = a;
    sum += v * v;
  }
  return [peak, Math.sqrt(sum / meterBuf.length)];
}

export class Strip {
  readonly input: GainNode;
  readonly panner: StereoPannerNode;
  readonly fader: GainNode;
  readonly muteGain: GainNode;
  readonly output: GainNode;
  readonly analyserL: AnalyserNode;
  readonly analyserR: AnalyserNode;
  slots: EffectSlot[] = [];
  sendGains = new Map<string, GainNode>();
  state: MixerTrack;

  constructor(private ctx: BaseAudioContext, track: MixerTrack, env: EffectEnv) {
    this.input = ctx.createGain();
    this.panner = ctx.createStereoPanner();
    this.fader = ctx.createGain();
    this.muteGain = ctx.createGain();
    this.output = ctx.createGain();
    const split = ctx.createChannelSplitter(2);
    this.analyserL = ctx.createAnalyser();
    this.analyserR = ctx.createAnalyser();
    this.analyserL.fftSize = 1024;
    this.analyserR.fftSize = 1024;
    this.panner.connect(this.fader).connect(this.muteGain).connect(this.output);
    this.output.connect(split);
    split.connect(this.analyserL, 0);
    split.connect(this.analyserR, 1);
    this.state = { ...track, inserts: [] };
    this.applyInserts(track.inserts, env);
    this.apply(track, env);
  }

  private rewire() {
    try { this.input.disconnect(); } catch { /* ignore */ }
    let prev: AudioNode = this.input;
    for (const s of this.slots) {
      try { s.output.disconnect(); } catch { /* ignore */ }
    }
    for (const s of this.slots) {
      prev.connect(s.input);
      prev = s.output;
    }
    prev.connect(this.panner);
  }

  private applyInserts(inserts: EffectInstance[], env: EffectEnv) {
    const existing = new Map(this.slots.map((s) => [s.instance.id, s]));
    const next: EffectSlot[] = [];
    for (const inst of inserts) {
      let slot = existing.get(inst.id);
      if (slot && slot.instance.type === inst.type) {
        slot.apply(inst, env);
        existing.delete(inst.id);
      } else {
        slot = new EffectSlot(this.ctx, createEffect(this.ctx, inst.type), inst, env);
      }
      next.push(slot);
    }
    for (const s of existing.values()) s.dispose();
    const orderChanged = next.length !== this.slots.length || next.some((s, i) => s !== this.slots[i]);
    this.slots = next;
    if (orderChanged || this.slots.length === 0) this.rewire();
  }

  apply(track: MixerTrack, env: EffectEnv) {
    if (track.inserts !== this.state.inserts) this.applyInserts(track.inserts, env);
    setParam(this.fader.gain, track.volume, this.ctx, 0.01);
    setParam(this.panner.pan, track.pan, this.ctx, 0.01);
    this.state = track;
  }

  setTempo(bpm: number) {
    for (const s of this.slots) s.effect.setTempo(bpm);
  }

  setAudible(on: boolean) {
    setParam(this.muteGain.gain, on ? 1 : 0, this.ctx, 0.005);
  }

  levels(): Levels {
    const [peakL, rmsL] = measure(this.analyserL);
    const [peakR, rmsR] = measure(this.analyserR);
    return { peakL, peakR, rmsL, rmsR };
  }

  slot(effectId: string) {
    return this.slots.find((s) => s.instance.id === effectId);
  }

  dispose() {
    for (const s of this.slots) s.dispose();
    for (const n of [this.input, this.panner, this.fader, this.muteGain, this.output]) {
      try { n.disconnect(); } catch { /* ignore */ }
    }
    for (const g of this.sendGains.values()) g.disconnect();
  }
}

export class ChannelHost {
  readonly instrument: Instrument;
  readonly volume: GainNode;
  readonly panner: StereoPannerNode;
  readonly mute: GainNode;
  readonly analyser: AnalyserNode;
  state: Channel;
  connectedTo: Strip | null = null;

  constructor(private ctx: BaseAudioContext, channel: Channel) {
    this.instrument = createInstrument(ctx, channel.kind);
    this.volume = ctx.createGain();
    this.panner = ctx.createStereoPanner();
    this.mute = ctx.createGain();
    this.analyser = ctx.createAnalyser();
    this.analyser.fftSize = 512;
    this.instrument.output.connect(this.volume).connect(this.panner).connect(this.mute);
    this.mute.connect(this.analyser);
    this.state = channel;
    this.apply(channel);
  }

  apply(ch: Channel) {
    this.instrument.update(ch);
    setParam(this.volume.gain, ch.volume, this.ctx, 0.01);
    setParam(this.panner.pan, ch.pan, this.ctx, 0.01);
    this.state = ch;
  }

  connect(strip: Strip) {
    if (this.connectedTo === strip) return;
    try { this.mute.disconnect(strip.input); } catch { /* ignore */ }
    if (this.connectedTo) {
      try { this.mute.disconnect(this.connectedTo.input); } catch { /* ignore */ }
    }
    this.mute.connect(strip.input);
    this.connectedTo = strip;
  }

  level(): number {
    return measure(this.analyser)[0];
  }

  dispose() {
    this.instrument.dispose();
    for (const n of [this.volume, this.panner, this.mute]) {
      try { n.disconnect(); } catch { /* ignore */ }
    }
  }
}

export class MixGraph {
  readonly strips = new Map<string, Strip>();
  readonly channels = new Map<string, ChannelHost>();
  readonly out: GainNode;
  private project: Project | null = null;
  private routingKey = '';
  private bpm = 120;

  constructor(readonly ctx: BaseAudioContext, destination: AudioNode = ctx.destination) {
    this.out = ctx.createGain();
    this.out.connect(destination);
  }

  get master(): Strip {
    return this.strips.get(MASTER_ID)!;
  }

  stripInput(id: string): AudioNode {
    return (this.strips.get(id) ?? this.master).input;
  }

  sync(project: Project) {
    const prev = this.project;
    if (prev === project) return;
    this.project = project;
    const env: EffectEnv = { bpm: project.bpm };

    if (!prev || prev.mixer !== project.mixer) this.syncMixer(project.mixer, env);
    if (prev && prev.bpm !== project.bpm) for (const s of this.strips.values()) s.setTempo(project.bpm);
    this.bpm = project.bpm;
    if (!prev || prev.channels !== project.channels || prev.mixer !== project.mixer) this.syncChannels(project.channels);
  }

  private syncMixer(mixer: MixerTrack[], env: EffectEnv) {
    const ids = new Set(mixer.map((t) => t.id));
    for (const [id, s] of this.strips) {
      if (!ids.has(id)) {
        s.dispose();
        this.strips.delete(id);
        this.routingKey = '';
        for (const h of this.channels.values()) if (h.connectedTo === s) h.connectedTo = null;
      }
    }
    for (const t of mixer) {
      const s = this.strips.get(t.id);
      if (!s) {
        this.strips.set(t.id, new Strip(this.ctx, t, env));
        this.routingKey = '';
      } else if (s.state !== t) {
        s.apply(t, env);
      }
    }
    const key = mixer.map((t) => `${t.id}>${t.output}|${t.sends.map((s) => `${s.id}:${s.target}`).join(',')}`).join(';');
    if (key !== this.routingKey) {
      this.routingKey = key;
      this.reroute(mixer);
    }
    for (const t of mixer) {
      const s = this.strips.get(t.id)!;
      for (const send of t.sends) {
        const g = s.sendGains.get(send.id);
        if (g) setParam(g.gain, send.amount, this.ctx, 0.01);
      }
    }
    const audible = audibleTracks(mixer);
    for (const t of mixer) this.strips.get(t.id)!.setAudible(!t.mute && audible.has(t.id));
  }

  private reroute(mixer: MixerTrack[]) {
    for (const s of this.strips.values()) {
      try { s.output.disconnect(); } catch { /* ignore */ }
      for (const g of s.sendGains.values()) g.disconnect();
      s.sendGains.clear();
      // keep meter connection
      const split = this.ctx.createChannelSplitter(2);
      s.output.connect(split);
      split.connect(s.analyserL, 0);
      split.connect(s.analyserR, 1);
    }
    for (const t of mixer) {
      const s = this.strips.get(t.id)!;
      if (t.id === MASTER_ID) {
        s.output.connect(this.out);
        continue;
      }
      const target = t.output && this.strips.has(t.output) && !wouldCycle(mixer.map((m) => (m.id === t.id ? { ...m, output: null } : m)), t.id, t.output)
        ? this.strips.get(t.output)!
        : this.master;
      s.output.connect(target.input);
      for (const send of t.sends) {
        const ts = this.strips.get(send.target);
        if (!ts || ts === s) continue;
        const g = this.ctx.createGain();
        g.gain.value = send.amount;
        s.output.connect(g).connect(ts.input);
        s.sendGains.set(send.id, g);
      }
    }
  }

  private syncChannels(channels: Channel[]) {
    const ids = new Set(channels.map((c) => c.id));
    for (const [id, h] of this.channels) {
      if (!ids.has(id)) {
        h.dispose();
        this.channels.delete(id);
      }
    }
    const anySolo = channels.some((c) => c.solo);
    for (const ch of channels) {
      let h = this.channels.get(ch.id);
      if (h && h.state.kind !== ch.kind) {
        h.dispose();
        this.channels.delete(ch.id);
        h = undefined;
      }
      if (!h) {
        h = new ChannelHost(this.ctx, ch);
        this.channels.set(ch.id, h);
      } else if (h.state !== ch) {
        h.apply(ch);
      }
      const strip = this.strips.get(ch.mixerTrackId) ?? this.master;
      if (h.connectedTo !== strip) h.connect(strip);
      setParam(h.mute.gain, ch.mute || (anySolo && !ch.solo) ? 0 : 1, this.ctx, 0.005);
    }
  }

  noteOn(channelId: string, pitch: number, velocity: number, time: number, duration?: number) {
    this.channels.get(channelId)?.instrument.noteOn(pitch, velocity, time, duration);
  }

  noteOff(channelId: string, pitch: number, time: number) {
    this.channels.get(channelId)?.instrument.noteOff(pitch, time);
  }

  allNotesOff(time?: number) {
    for (const h of this.channels.values()) h.instrument.allNotesOff(time);
  }

  voiceCount() {
    let n = 0;
    for (const h of this.channels.values()) n += h.instrument.activeVoices();
    return n;
  }

  get tempo() {
    return this.bpm;
  }

  dispose() {
    for (const h of this.channels.values()) h.dispose();
    for (const s of this.strips.values()) s.dispose();
    this.channels.clear();
    this.strips.clear();
    this.out.disconnect();
  }
}
