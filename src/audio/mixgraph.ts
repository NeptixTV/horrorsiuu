// The audio graph for one project: mixer strips with insert chains, sends and
// routing, plus one instrument host per channel. Works with any
// BaseAudioContext, so the same code drives live playback and offline export.
import type { Channel, Clip, EffectInstance, MixerTrack, Project } from '../state/types';
import { MASTER_ID } from '../state/types';
import { audibleTracks, wouldCycle } from '../state/routing';
import { createEffect } from './effects';
import { Effect, EffectSlot, setParam, type EffectEnv } from './effects/base';
import { createInstrument, type Instrument } from './instruments';
import { diagnostics } from './diagnostics';
import { clampFinite } from './safety';

export interface Levels {
  peakL: number;
  peakR: number;
  rmsL: number;
  rmsR: number;
}

// one scratch buffer per analyser size (a 1024 buffer read from a 512 point
// analyser used to keep stale samples in its upper half → phantom peaks)
const meterBufs = new Map<number, Float32Array<ArrayBuffer>>();

function measure(an: AnalyserNode): [number, number] {
  let buf = meterBufs.get(an.fftSize);
  if (!buf) meterBufs.set(an.fftSize, (buf = new Float32Array(an.fftSize)));
  an.getFloatTimeDomainData(buf);
  let peak = 0;
  let sum = 0;
  for (let i = 0; i < buf.length; i++) {
    const v = buf[i];
    const a = v < 0 ? -v : v;
    if (a > peak) peak = a;
    sum += v * v;
  }
  // a non-finite meter value means the signal itself is broken
  if (!Number.isFinite(peak)) return [0, 0];
  return [peak, Math.sqrt(sum / buf.length)];
}

/** Passthrough used when an effect cannot be created (keeps the chain intact). */
class PassEffect extends Effect {
  constructor(ctx: BaseAudioContext) {
    super(ctx);
    this.input.connect(this.output);
  }
  update() {}
}

function safeCreateEffect(ctx: BaseAudioContext, type: EffectInstance['type']): Effect {
  try {
    return createEffect(ctx, type);
  } catch (err) {
    diagnostics.log('error', 'effect-create', `${type}: ${err instanceof Error ? err.message : String(err)}`, 2000);
    return new PassEffect(ctx);
  }
}

/** An ordered chain of insert effects between `input` and `output`. */
export class InsertChain {
  readonly input: GainNode;
  readonly output: GainNode;
  slots: EffectSlot[] = [];
  private current: EffectInstance[] | null = null;

  constructor(private ctx: BaseAudioContext) {
    this.input = ctx.createGain();
    this.output = ctx.createGain();
    this.input.connect(this.output);
  }

  private rewire() {
    try { this.input.disconnect(); } catch { /* ignore */ }
    for (const s of this.slots) {
      try { s.output.disconnect(); } catch { /* ignore */ }
    }
    let prev: AudioNode = this.input;
    for (const s of this.slots) {
      prev.connect(s.input);
      prev = s.output;
    }
    prev.connect(this.output);
  }

  apply(inserts: EffectInstance[], env: EffectEnv) {
    if (inserts === this.current) return;
    this.current = inserts;
    const existing = new Map(this.slots.map((s) => [s.instance.id, s]));
    const next: EffectSlot[] = [];
    for (const inst of inserts) {
      let slot = existing.get(inst.id);
      if (slot && slot.instance.type === inst.type) {
        slot.apply(inst, env);
        existing.delete(inst.id);
      } else {
        slot = new EffectSlot(this.ctx, safeCreateEffect(this.ctx, inst.type), inst, env);
      }
      next.push(slot);
    }
    for (const s of existing.values()) s.dispose();
    const orderChanged = next.length !== this.slots.length || next.some((s, i) => s !== this.slots[i]);
    this.slots = next;
    if (orderChanged) this.rewire();
  }

  setTempo(bpm: number) {
    for (const s of this.slots) s.effect.setTempo(bpm);
  }

  slot(effectId: string) {
    return this.slots.find((s) => s.instance.id === effectId);
  }

  dispose() {
    for (const s of this.slots) s.dispose();
    try { this.input.disconnect(); } catch { /* ignore */ }
    try { this.output.disconnect(); } catch { /* ignore */ }
  }
}

export class Strip {
  readonly input: GainNode;
  readonly chain: InsertChain;
  readonly panner: StereoPannerNode;
  readonly fader: GainNode;
  readonly muteGain: GainNode;
  readonly output: GainNode;
  readonly analyserL: AnalyserNode;
  readonly analyserR: AnalyserNode;
  readonly meterSplit: ChannelSplitterNode;
  sendGains = new Map<string, GainNode>();
  state: MixerTrack;

  constructor(private ctx: BaseAudioContext, track: MixerTrack, env: EffectEnv) {
    this.chain = new InsertChain(ctx);
    this.input = this.chain.input;
    this.panner = ctx.createStereoPanner();
    this.fader = ctx.createGain();
    this.muteGain = ctx.createGain();
    this.output = ctx.createGain();
    this.meterSplit = ctx.createChannelSplitter(2);
    this.analyserL = ctx.createAnalyser();
    this.analyserR = ctx.createAnalyser();
    this.analyserL.fftSize = 1024;
    this.analyserR.fftSize = 1024;
    this.chain.output.connect(this.panner);
    this.panner.connect(this.fader).connect(this.muteGain).connect(this.output);
    this.output.connect(this.meterSplit);
    this.meterSplit.connect(this.analyserL, 0);
    this.meterSplit.connect(this.analyserR, 1);
    this.state = track;
    this.chain.apply(track.inserts, env);
    this.apply(track, env);
  }

  get slots() {
    return this.chain.slots;
  }

  apply(track: MixerTrack, env: EffectEnv) {
    this.chain.apply(track.inserts, env);
    setParam(this.fader.gain, clampFinite(track.volume, 0, 2, 0.8), this.ctx, 0.01);
    setParam(this.panner.pan, clampFinite(track.pan, -1, 1, 0), this.ctx, 0.01);
    this.state = track;
  }

  setTempo(bpm: number) {
    this.chain.setTempo(bpm);
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
    return this.chain.slot(effectId);
  }

  dispose() {
    this.chain.dispose();
    for (const n of [this.panner, this.fader, this.muteGain, this.output, this.meterSplit]) {
      try { n.disconnect(); } catch { /* ignore */ }
    }
    for (const g of this.sendGains.values()) { try { g.disconnect(); } catch { /* ignore */ } }
    this.sendGains.clear();
  }
}

/** Per-clip effect chain + pan for audio clips (persistent, so it can be tweaked while playing). */
export class ClipHost {
  readonly chain: InsertChain;
  readonly panner: StereoPannerNode;
  state: Clip;
  connectedTo: AudioNode | null = null;

  constructor(private ctx: BaseAudioContext, clip: Clip, env: EffectEnv) {
    this.chain = new InsertChain(ctx);
    this.panner = ctx.createStereoPanner();
    this.chain.output.connect(this.panner);
    this.state = clip;
    this.apply(clip, env);
  }

  get input() {
    return this.chain.input;
  }

  apply(clip: Clip, env: EffectEnv) {
    this.chain.apply(clip.fx ?? [], env);
    setParam(this.panner.pan, clampFinite(clip.pan, -1, 1, 0), this.ctx, 0.01);
    this.state = clip;
  }

  connect(dest: AudioNode) {
    if (this.connectedTo === dest) return;
    try { this.panner.disconnect(); } catch { /* ignore */ }
    this.panner.connect(dest);
    this.connectedTo = dest;
  }

  dispose() {
    this.chain.dispose();
    try { this.panner.disconnect(); } catch { /* ignore */ }
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
    try {
      this.instrument.update(ch);
    } catch (err) {
      diagnostics.log('error', 'instrument-update', `${ch.name}: ${err instanceof Error ? err.message : String(err)}`, 2000);
    }
    setParam(this.volume.gain, clampFinite(ch.volume, 0, 2, 0.8), this.ctx, 0.01);
    setParam(this.panner.pan, clampFinite(ch.pan, -1, 1, 0), this.ctx, 0.01);
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
  readonly clips = new Map<string, ClipHost>();
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
    let failed = false;
    const phase = (name: string, fn: () => void) => {
      try {
        fn();
      } catch (err) {
        failed = true;
        diagnostics.log('error', 'graph-sync', `${name}: ${err instanceof Error ? err.message : String(err)}`, 2000);
      }
    };

    if (!prev || prev.mixer !== project.mixer) phase('mixer', () => this.syncMixer(project.mixer, env));
    if (prev && prev.bpm !== project.bpm) {
      phase('tempo', () => {
        for (const s of this.strips.values()) s.setTempo(project.bpm);
        for (const c of this.clips.values()) c.chain.setTempo(project.bpm);
      });
    }
    this.bpm = project.bpm;
    if (!prev || prev.channels !== project.channels || prev.mixer !== project.mixer) phase('channels', () => this.syncChannels(project.channels));
    if (!prev || prev.clips !== project.clips || prev.tracks !== project.tracks || prev.mixer !== project.mixer) phase('clips', () => this.syncClips(project, env));
    // a failed phase is retried in full with the next change instead of being skipped for good
    if (failed) this.project = null;
  }

  /** Destination for an audio clip: its own effect chain, or the mixer insert of its track. */
  clipInput(clip: Clip, project: Project): AudioNode {
    const host = this.clips.get(clip.id);
    if (host) return host.input;
    const track = project.tracks.find((t) => t.id === clip.trackId);
    return this.stripInput(track?.mixerTrackId ?? MASTER_ID);
  }

  clipSlot(clipId: string, effectId: string) {
    return this.clips.get(clipId)?.chain.slot(effectId);
  }

  private syncClips(project: Project, env: EffectEnv) {
    const need = new Set<string>();
    const tracks = new Map(project.tracks.map((t) => [t.id, t]));
    for (const clip of project.clips) {
      if (clip.kind !== 'audio') continue;
      if (!(clip.fx?.length) && !(clip.pan && Math.abs(clip.pan) > 0.001)) continue;
      need.add(clip.id);
      let host = this.clips.get(clip.id);
      if (!host) {
        host = new ClipHost(this.ctx, clip, env);
        this.clips.set(clip.id, host);
      } else if (host.state !== clip) {
        host.apply(clip, env);
      }
      host.connect(this.stripInput(tracks.get(clip.trackId)?.mixerTrackId ?? MASTER_ID));
    }
    for (const [id, host] of this.clips) {
      if (!need.has(id)) {
        host.dispose();
        this.clips.delete(id);
      }
    }
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
        if (g) setParam(g.gain, clampFinite(send.amount, 0, 1, 0), this.ctx, 0.01);
      }
    }
    const audible = audibleTracks(mixer);
    for (const t of mixer) this.strips.get(t.id)!.setAudible(!t.mute && audible.has(t.id));
  }

  private reroute(mixer: MixerTrack[]) {
    for (const s of this.strips.values()) {
      try { s.output.disconnect(); } catch { /* ignore */ }
      for (const g of s.sendGains.values()) { try { g.disconnect(); } catch { /* ignore */ } }
      s.sendGains.clear();
      // keep the meter connection (same splitter, no new nodes per reroute)
      s.output.connect(s.meterSplit);
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
        // never build a feedback loop, whatever the project says
        if (send.target === MASTER_ID || wouldCycle(mixer.map((m) => (m.id === t.id ? { ...m, sends: m.sends.filter((x) => x.id !== send.id) } : m)), t.id, send.target)) {
          diagnostics.log('warn', 'send-loop', `Send from “${t.name}” skipped (would create a feedback loop)`, 5000);
          continue;
        }
        const g = this.ctx.createGain();
        g.gain.value = clampFinite(send.amount, 0, 1, 0);
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
    for (const h of this.channels.values()) {
      try { h.instrument.allNotesOff(time); } catch { /* keep stopping the others */ }
    }
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
    for (const h of this.channels.values()) { try { h.dispose(); } catch { /* ignore */ } }
    for (const c of this.clips.values()) { try { c.dispose(); } catch { /* ignore */ } }
    for (const s of this.strips.values()) { try { s.dispose(); } catch { /* ignore */ } }
    this.channels.clear();
    this.clips.clear();
    this.strips.clear();
    this.project = null;
    try { this.out.disconnect(); } catch { /* ignore */ }
  }
}
