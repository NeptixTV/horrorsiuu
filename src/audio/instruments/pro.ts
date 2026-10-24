// Pro Sounds instrument: plays the built-in multi-sampled instruments and
// one-shots (piano, strings, 808s, drum kits …) with velocity layers, round
// robin, seamless sustain loops, 808-style mono glide and choke groups.
import type { Channel, ProParams } from '../../state/types';
import { defaultPro, proSound, proZones, type ProSound, type ProZone } from '../pro/catalog';
import { loadZone, midiVel, preloadSound, releaseUnused, zoneBuffer, zonesFor } from '../pro/loader';
import { clampFinite } from '../safety';
import { holdAt, type Instrument } from './types';

interface PVoice {
  pitch: number;
  zone: ProZone;
  src: AudioBufferSourceNode;
  gain: GainNode;
  peak: number;
  /** when the key is let go (Infinity = held) */
  noteEnd: number;
  /** when the voice is silent */
  end: number;
  oneShot: boolean;
  choke?: string;
}

const MAX_VOICES = 48;

// choke groups are shared by all Pro channels of one audio context
const chokeGroups = new WeakMap<BaseAudioContext, Map<string, Set<PVoice>>>();
function group(ctx: BaseAudioContext, name: string) {
  let m = chokeGroups.get(ctx);
  if (!m) chokeGroups.set(ctx, (m = new Map()));
  let s = m.get(name);
  if (!s) m.set(name, (s = new Set()));
  return s;
}

const reversed = new WeakMap<AudioBuffer, AudioBuffer>();
function reverseOf(b: AudioBuffer): AudioBuffer {
  let r = reversed.get(b);
  if (!r) {
    r = new AudioBuffer({ length: b.length, numberOfChannels: b.numberOfChannels, sampleRate: b.sampleRate });
    for (let c = 0; c < b.numberOfChannels; c++) {
      const s = b.getChannelData(c);
      const d = r.getChannelData(c);
      for (let i = 0, n = s.length; i < n; i++) d[i] = s[n - 1 - i];
    }
    reversed.set(b, r);
  }
  return r;
}

/** Live instruments, so unused sample memory can be released. */
const instances = new Set<ProInstrument>();
export function proSoundsInUse(): Set<string> {
  return new Set([...instances].map((i) => i.soundId));
}

/** Every 30 s: decoded samples of sounds that no channel uses any more are freed (exports keep theirs while they run). */
export function installProMemorySweep() {
  let lastUsed = new Set<string>();
  setInterval(() => {
    const used = proSoundsInUse();
    // a sound switched away from a moment ago (or undone) is kept for one more round
    releaseUnused(new Set([...used, ...lastUsed]));
    lastUsed = used;
  }, 30000);
}

export function driveCurve(amount: number): Float32Array<ArrayBuffer> {
  const k = 1 + clampFinite(amount, 0, 1, 0) * 9;
  const n = 1024;
  const c = new Float32Array(n);
  const norm = Math.tanh(k);
  for (let i = 0; i < n; i++) {
    const x = (i / (n - 1)) * 2 - 1;
    c[i] = Math.tanh(k * x) / norm;
  }
  return c;
}

export class ProInstrument implements Instrument {
  readonly output: GainNode;
  readonly filterOffset: AudioParam;
  private input: GainNode;
  private tone: BiquadFilterNode;
  private shaper: WaveShaperNode;
  private post: GainNode;
  private driveOn = false;
  private driveAmt = -1;
  private p: ProParams = defaultPro();
  private def: ProSound | undefined = proSound(this.p.sound);
  private zones: ProZone[] = [];
  private voices: PVoice[] = [];
  private rr = new Map<string, number>();
  /** notes that found no loaded sample (shown as "loading" by the UI) */
  missed = 0;

  constructor(private ctx: BaseAudioContext) {
    this.input = ctx.createGain();
    this.tone = ctx.createBiquadFilter();
    this.tone.type = 'lowpass';
    this.tone.Q.value = 0.5;
    this.tone.frequency.value = 22000;
    try { this.tone.frequency.automationRate = 'k-rate'; } catch { /* a-rate */ }
    this.filterOffset = this.tone.detune;
    this.shaper = ctx.createWaveShaper();
    this.shaper.oversample = '2x';
    this.post = ctx.createGain();
    this.output = ctx.createGain();
    this.input.connect(this.tone).connect(this.output);
    this.shaper.connect(this.post).connect(this.output);
    instances.add(this);
  }

  get soundId() {
    return this.p.sound;
  }

  update(ch: Channel) {
    const next = ch.pro ?? defaultPro();
    if (next.sound !== this.p.sound || !this.zones.length) {
      this.zones = proZones(next.sound);
      this.def = proSound(next.sound);
      this.rr.clear();
      void preloadSound(next.sound);
    }
    this.p = next;
    const now = this.ctx.currentTime;
    // tone 1 = open; lower values close a gentle low-pass down to 200 Hz
    const t = clampFinite(next.tone, 0, 1, 1);
    const f = t >= 0.999 ? 22000 : 200 * Math.pow(100, t);
    this.tone.frequency.setTargetAtTime(Math.min(f, this.ctx.sampleRate * 0.45), now, 0.02);
    const d = clampFinite(next.drive, 0, 1, 0);
    if (d !== this.driveAmt) {
      this.driveAmt = d;
      const on = d > 0.001;
      if (on) {
        this.shaper.curve = driveCurve(d);
        // keep the level roughly the same with more drive
        this.post.gain.value = Math.pow(Math.tanh(1 + d * 9) / (1 + d * 9), 0.75);
      }
      if (on !== this.driveOn) {
        this.driveOn = on;
        try { this.tone.disconnect(); } catch { /* ignore */ }
        this.tone.connect(on ? this.shaper : this.output);
      }
    }
    this.output.gain.setTargetAtTime(clampFinite(next.level, 0, 1.5, 0.9), now, 0.01);
  }

  activeVoices() {
    const now = this.ctx.currentTime;
    this.voices = this.voices.filter((v) => v.end > now);
    return this.voices.length;
  }

  private rateFor(z: ProZone, pitch: number) {
    return clampFinite(Math.pow(2, (pitch - z.r + this.p.tune) / 12), 1 / 16, 16, 1);
  }

  /** A loaded zone for the note: the exact one (round robin) or the nearest loaded one meanwhile. */
  private pick(pitch: number, vel127: number): ProZone | null {
    const key = pitch + Math.round(this.p.tune);
    const exact = zonesFor(this.zones, key, vel127);
    const ready = exact.filter((z) => zoneBuffer(z));
    if (exact.length > ready.length) for (const z of exact) if (!zoneBuffer(z)) void loadZone(z, true);
    if (ready.length) {
      const k = `${ready[0].r}:${ready[0].vh}`;
      const n = this.rr.get(k) ?? 0;
      this.rr.set(k, n + 1);
      return ready[n % ready.length];
    }
    // fall back to the nearest loaded sample (prefer the same velocity layer)
    let best: ProZone | null = null;
    let score = Infinity;
    for (const z of this.zones) {
      if (!zoneBuffer(z)) continue;
      const d = Math.abs(z.r - key) + (vel127 >= z.vl && vel127 <= z.vh ? 0 : 6);
      if (d < score) { score = d; best = z; }
    }
    return best && Math.abs(best.r - key) <= 12 ? best : null;
  }

  private kill(v: PVoice, time: number) {
    holdAt(v.gain.gain, time, v.peak);
    v.gain.gain.setTargetAtTime(0, time, 0.005);
    try { v.src.stop(time + 0.05); } catch { /* already stopped */ }
    v.end = Math.min(v.end, time + 0.05);
    v.noteEnd = Math.min(v.noteEnd, time);
  }

  private release(v: PVoice, time: number) {
    const rel = Math.max(0.005, this.p.release);
    holdAt(v.gain.gain, time, v.peak);
    v.gain.gain.setTargetAtTime(0, time, rel / 5);
    const stopAt = time + rel + 0.05;
    try { v.src.stop(stopAt); } catch { /* already stopped */ }
    v.noteEnd = time;
    v.end = Math.min(v.end, stopAt);
  }

  noteOn(pitch: number, velocity: number, time: number, duration?: number) {
    if (!Number.isFinite(pitch) || !Number.isFinite(time) || !this.zones.length) return;
    const ctx = this.ctx;
    const t = Math.max(time, ctx.currentTime);
    velocity = clampFinite(velocity, 0, 1, 0.8);
    if (duration !== undefined && !(duration > 0 && Number.isFinite(duration))) duration = 0.05;
    const p = this.p;
    const oneShot = !!this.def?.oneShot;
    const vel127 = midiVel(velocity);

    // 808 slide: a new note that overlaps the held one glides there
    if (p.mono && p.glide > 0 && !oneShot) {
      const held = [...this.voices].reverse().find((v) => v.noteEnd > t + 0.001 && v.end > t);
      if (held) {
        const rate = this.rateFor(held.zone, pitch);
        const pr = held.src.playbackRate;
        holdAt(pr, t, pr.value);
        pr.setTargetAtTime(rate, t, Math.max(0.005, p.glide / 3));
        held.pitch = pitch;
        holdAt(held.gain.gain, t, held.peak);
        held.gain.gain.setValueAtTime(held.peak, t);
        held.noteEnd = Infinity;
        held.end = Infinity;
        try { held.src.stop(t + 3600); } catch { /* ignore */ }
        if (duration !== undefined) this.release(held, t + duration);
        return;
      }
    }

    const zone = this.pick(pitch, vel127);
    if (!zone) {
      this.missed++;
      return;
    }
    const raw = zoneBuffer(zone)!;
    const buffer = p.reverse ? reverseOf(raw) : raw;

    if (p.mono) for (const v of this.voices) if (v.end > t) this.kill(v, t);
    if (this.def?.chokes) for (const v of group(ctx, this.def.chokes)) if (v.end > t) this.kill(v, t);
    // voice limit: steal the oldest (released ones first)
    while (this.activeVoices() >= MAX_VOICES) {
      let i = this.voices.findIndex((v) => v.noteEnd <= t);
      if (i < 0) i = 0;
      const [v] = this.voices.splice(i, 1);
      this.kill(v, t);
    }

    const src = ctx.createBufferSource();
    src.buffer = buffer;
    src.playbackRate.value = this.rateFor(zone, pitch);
    if (zone.ls !== undefined && zone.le !== undefined && !p.reverse) {
      src.loop = true;
      src.loopStart = zone.ls;
      src.loopEnd = zone.le;
    }
    const g = ctx.createGain();
    const peak = Math.pow(velocity, 2 * clampFinite(p.velocity, 0, 1, 0.5)) * (zone.g ?? 1);
    const a = Math.max(0.001, p.attack);
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(peak, t + a);
    src.connect(g).connect(this.input);
    src.start(t);
    const natural = src.loop ? Infinity : buffer.duration / src.playbackRate.value;
    const v: PVoice = { pitch, zone, src, gain: g, peak, noteEnd: Infinity, end: t + natural, oneShot, choke: this.def?.choke };
    src.onended = () => {
      try { g.disconnect(); } catch { /* ignore */ }
      if (v.choke) group(ctx, v.choke).delete(v);
    };
    if (v.choke) group(ctx, v.choke).add(v);
    if (oneShot) {
      v.noteEnd = t;
    } else if (duration !== undefined) {
      this.release(v, t + Math.max(a, duration));
    }
    this.voices.push(v);
  }

  noteOff(pitch: number, time: number) {
    if (this.def?.oneShot) return;
    for (const v of this.voices) {
      if (v.pitch === pitch && v.noteEnd > time && v.end > time) this.release(v, Math.max(time, this.ctx.currentTime));
    }
  }

  allNotesOff(time = this.ctx.currentTime) {
    for (const v of this.voices) this.kill(v, time);
    this.voices = [];
  }

  dispose() {
    this.allNotesOff();
    instances.delete(this);
    for (const n of [this.input, this.tone, this.shaper, this.post, this.output]) {
      try { n.disconnect(); } catch { /* ignore */ }
    }
  }
}
