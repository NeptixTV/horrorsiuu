// Live note recording into a pattern (DreamChord, MIDI keyboard, computer
// keyboard). Notes are overdubbed (nothing is deleted), optionally quantised
// to 1/16, written while you play and undone with a single Ctrl+Z.
import type { Note, Project } from '../state/types';
import { PPQ, TICKS_PER_STEP } from '../state/types';
import { getProject, pushHistory, update } from '../state/store';
import { setUI, toast, ui } from '../state/ui';
import { engine } from '../audio/engine';
import { patternLength } from '../audio/sequencer';
import { uid } from '../state/utils';

interface Pending {
  channelId: string;
  pitch: number;
  velocity: number;
  start: number; // pattern tick
}

type Listener = () => void;

class LiveRecorder {
  active = false;
  patternId: string | null = null;
  quantize = true;
  count = 0;
  private base: Project | null = null;
  private pending = new Map<string, Pending>();
  private done: { channelId: string; note: Note }[] = [];
  private timer: ReturnType<typeof setInterval> | null = null;
  private warned = false;
  private listeners = new Set<Listener>();
  private unsub: (() => void) | null = null;

  onChange(l: Listener): () => void {
    this.listeners.add(l);
    return () => { this.listeners.delete(l); };
  }

  private emit() {
    this.listeners.forEach((l) => l());
  }

  /** Arms recording into the selected pattern; starts the pattern loop if nothing plays. */
  async start(): Promise<boolean> {
    if (this.active) return true;
    if (!engine.initialized) return false;
    await engine.resume();
    const p = getProject();
    const pat = p.patterns.find((x) => x.id === ui().selectedPatternId) ?? p.patterns[0];
    if (!pat) return false;
    this.patternId = pat.id;
    this.base = p;
    this.pending.clear();
    this.done = [];
    this.count = 0;
    this.warned = false;
    const t = engine.transport;
    if (!t.playing) {
      t.patternId = pat.id;
      if (t.mode !== 'pattern') t.setMode('pattern');
      setUI({ selectedPatternId: pat.id });
      t.play(0, countInEnd());
    }
    this.active = true;
    this.timer = setInterval(() => this.flush(), 400);
    // stopping the transport ends the take
    this.unsub = engine.onChange(() => {
      if (this.active && !engine.transport.playing) this.stop();
    });
    this.emit();
    return true;
  }

  stop() {
    if (!this.active) return;
    // close notes that are still held
    const now = engine.ctx.currentTime;
    for (const [key, n] of this.pending) {
      this.finish(n, this.tickOf(now) ?? n.start + TICKS_PER_STEP);
      this.pending.delete(key);
    }
    this.flush();
    this.active = false;
    if (this.timer) clearInterval(this.timer);
    this.timer = null;
    this.unsub?.();
    this.unsub = null;
    if (this.base) pushHistory(this.base, 'Record notes');
    this.base = null;
    const pat = getProject().patterns.find((x) => x.id === this.patternId);
    if (this.count > 0) toast(`Recorded ${this.count} note${this.count === 1 ? '' : 's'} into “${pat?.name ?? 'pattern'}”`, 'ok');
    this.emit();
  }

  toggle() {
    if (this.active) this.stop();
    else void this.start();
  }

  /** Position of a context time inside the recorded pattern (null = not inside it). */
  private tickOf(time: number): number | null {
    const t = engine.transport;
    if (!t?.playing || !this.patternId) return null;
    const p = getProject();
    const pat = p.patterns.find((x) => x.id === this.patternId);
    if (!pat) return null;
    const len = patternLength(pat, p.timeSig);
    const abs = t.tickAt(time);
    if (t.mode === 'pattern') return ((abs % len) + len) % len;
    const clip = p.clips.find((c) => c.kind === 'pattern' && c.patternId === pat.id && abs >= c.start && abs < c.start + c.length);
    if (!clip) {
      if (!this.warned) {
        this.warned = true;
        toast('Recording goes into the selected pattern – it is not under the playhead in the playlist', 'info', true);
      }
      return null;
    }
    const rel = abs - (clip.start - clip.offset);
    return ((rel % len) + len) % len;
  }

  noteOn(channelId: string, pitch: number, velocity: number, time: number) {
    if (!this.active) return;
    const tick = this.tickOf(time);
    if (tick === null) return;
    const key = `${channelId}:${pitch}`;
    const prev = this.pending.get(key);
    if (prev) this.finish(prev, tick);
    this.pending.set(key, { channelId, pitch, velocity, start: tick });
  }

  noteOff(channelId: string, pitch: number, time: number) {
    if (!this.active) return;
    const key = `${channelId}:${pitch}`;
    const n = this.pending.get(key);
    if (!n) return;
    this.pending.delete(key);
    this.finish(n, this.tickOf(time) ?? n.start + TICKS_PER_STEP);
  }

  /** A note with a known length (arpeggiator). */
  note(channelId: string, pitch: number, velocity: number, time: number, seconds: number) {
    if (!this.active) return;
    const start = this.tickOf(time);
    if (start === null) return;
    const spt = 60 / (getProject().bpm * PPQ);
    this.finish({ channelId, pitch, velocity, start }, start + seconds / spt);
  }

  private finish(n: Pending, endTick: number) {
    const p = getProject();
    const pat = p.patterns.find((x) => x.id === this.patternId);
    if (!pat) return;
    const len = patternLength(pat, p.timeSig);
    let start = n.start;
    let end = endTick;
    if (end < start) end += len; // held across the loop point
    let dur = end - start;
    if (this.quantize) {
      const q = TICKS_PER_STEP;
      start = Math.round(start / q) * q;
      if (start >= len) start -= len;
      dur = Math.round(dur / (q / 2)) * (q / 2);
    }
    const length = Math.round(Math.max(PPQ / 8, Math.min(len, dur)));
    this.done.push({ channelId: n.channelId, note: { id: uid('n'), pitch: n.pitch, start: Math.round(start), length, velocity: Math.max(0.05, Math.min(1, n.velocity)) } });
    this.count++;
  }

  private flush() {
    if (!this.done.length || !this.patternId) return;
    const batch = this.done;
    this.done = [];
    const pid = this.patternId;
    update('Record notes', (d) => {
      const pat = d.patterns.find((x) => x.id === pid);
      if (!pat) return;
      for (const { channelId, note } of batch) {
        if (!d.channels.some((c) => c.id === channelId)) continue;
        (pat.notes[channelId] ??= []).push(note);
      }
    }, { history: false });
    this.emit();
  }
}

/** End time of the count-in (Settings → Audio → Count-in) or undefined. */
function countInEnd(): number | undefined {
  const bars = ui().countInBars;
  if (!(bars > 0) || !engine.initialized) return undefined;
  const p = getProject();
  const beat = (60 / p.bpm) * (4 / p.timeSig[1]);
  const beats = Math.round(bars * p.timeSig[0]);
  const ctx = engine.ctx;
  const t0 = ctx.currentTime + 0.1;
  for (let b = 0; b < beats; b++) {
    const time = t0 + b * beat;
    const osc = ctx.createOscillator();
    osc.frequency.value = b % p.timeSig[0] === 0 ? 1760 : 1175;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0, time);
    g.gain.linearRampToValueAtTime(0.4 * Math.max(0.2, ui().metronomeVolume), time + 0.001);
    g.gain.exponentialRampToValueAtTime(0.0001, time + 0.06);
    osc.connect(g).connect(engine.masterOut);
    osc.start(time);
    osc.stop(time + 0.07);
  }
  return t0 + beats * beat;
}

export const liveRecorder = new LiveRecorder();
