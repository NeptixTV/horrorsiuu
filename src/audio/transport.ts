// Live transport: a look-ahead scheduler (timer in a Web Worker so it keeps
// running in background tabs) that feeds sample-accurate events to the graph.
import type { Project } from '../state/types';
import { PPQ, TICKS_PER_STEP } from '../state/types';
import { ticksPerBar } from '../state/utils';
import { scheduleAudioClip } from './clipPlayback';
import type { MixGraph } from './mixgraph';
import { diagnostics } from './diagnostics';
import {
  collectAudioSpanning, collectAudioStarts, collectNoteEvents, patternLength, songLength, type AudioEvent, type PlayMode,
} from './sequencer';

interface Anchor {
  time: number;
  tick: number;
  spt: number;
}

interface ActiveSource {
  src: AudioBufferSourceNode;
  gain: GainNode;
  end: number;
}

/** Default scheduling window in seconds (Settings → Audio → Scheduling buffer). */
export const DEFAULT_LOOKAHEAD = 0.12;
/** Events must be scheduled at least this far ahead; later than that they are skipped. */
const SAFETY = 0.004;

function makeTimer(cb: () => void): { start(): void; stop(): void; dispose?(): void } {
  try {
    const code = `let id=null;onmessage=e=>{if(e.data==='start'){clearInterval(id);id=setInterval(()=>postMessage(0),25)}else{clearInterval(id);id=null}}`;
    const url = URL.createObjectURL(new Blob([code], { type: 'application/javascript' }));
    const w = new Worker(url);
    URL.revokeObjectURL(url);
    w.onmessage = cb;
    return { start: () => w.postMessage('start'), stop: () => w.postMessage('stop'), dispose: () => w.terminate() };
  } catch {
    let id: ReturnType<typeof setInterval> | null = null;
    return {
      start: () => { if (id) clearInterval(id); id = setInterval(cb, 25); },
      stop: () => { if (id) clearInterval(id); id = null; },
    };
  }
}

export class Transport {
  mode: PlayMode = 'pattern';
  patternId: string | null = null;
  playing = false;
  metronome = false;
  private positionTick = 0;
  private cursorTick = 0;
  private cursorTime = 0;
  private anchors: Anchor[] = [];
  private sources = new Set<ActiveSource>();
  private timer = makeTimer(() => this.schedule());
  private listeners = new Set<() => void>();
  /** scheduling window in seconds – bigger = more robust against UI stalls, smaller = snappier edits */
  lookahead = DEFAULT_LOOKAHEAD;
  /** extra hooks run for every scheduled step (automation …); shared with the engine */
  stepHooks = new Set<(from: number, to: number, time: number, spt: number) => void>();

  constructor(private ctx: AudioContext, public graph: MixGraph, private getProject: () => Project) {}

  /** Swaps the graph after an engine reset (playback continues on the new graph). */
  setGraph(graph: MixGraph) {
    const was = this.playing;
    const pos = this.getTick();
    if (was) this.halt();
    this.graph = graph;
    if (was) {
      this.positionTick = pos;
      this.play(pos);
    }
  }

  dispose() {
    this.halt();
    this.timer.dispose?.();
    this.listeners.clear();
  }

  onChange(l: () => void) {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }

  private emit() {
    this.listeners.forEach((l) => l());
  }

  loopRange(project = this.getProject()): [number, number] {
    if (this.mode === 'pattern') {
      const p = project.patterns.find((x) => x.id === this.patternId) ?? project.patterns[0];
      return [0, p ? patternLength(p, project.timeSig) : PPQ * 4];
    }
    if (project.loop.enabled && project.loop.end > project.loop.start) return [project.loop.start, project.loop.end];
    return [0, songLength(project)];
  }

  /** Starts playback (optionally at a later context time); returns the start time. */
  play(fromTick?: number, at?: number): number {
    if (this.playing) return this.anchors[0]?.time ?? this.ctx.currentTime;
    const project = this.getProject();
    const [ls, le] = this.loopRange(project);
    let start = fromTick ?? this.positionTick;
    if (!Number.isFinite(start) || start >= le || start < 0) start = ls;
    const spt = 60 / (project.bpm * PPQ);
    const t0 = Math.max(this.ctx.currentTime + 0.06, at ?? 0);
    this.cursorTick = start;
    this.cursorTime = t0;
    this.anchors = [{ time: t0, tick: start, spt }];
    this.playing = true;
    if (this.mode === 'song') for (const ev of collectAudioSpanning(project, start, project.bpm)) this.startAudio(ev, t0, spt, project);
    this.schedule();
    this.timer.start();
    this.emit();
    return t0;
  }

  pause() {
    if (!this.playing) return;
    this.positionTick = this.getTick();
    this.halt();
  }

  stop() {
    const project = this.getProject();
    this.halt();
    this.positionTick = this.mode === 'song' && project.loop.enabled ? project.loop.start : 0;
    this.emit();
  }

  private halt() {
    this.timer.stop();
    this.playing = false;
    const now = this.ctx.currentTime;
    this.graph.allNotesOff(now);
    for (const s of this.sources) {
      s.gain.gain.cancelScheduledValues(now);
      s.gain.gain.setTargetAtTime(0, now, 0.008);
      try { s.src.stop(now + 0.05); } catch { /* ignore */ }
    }
    this.sources.clear();
    this.emit();
  }

  setPosition(tick: number) {
    const t = Math.max(0, Math.round(tick));
    if (this.playing) {
      this.halt();
      this.positionTick = t;
      this.play(t);
    } else {
      this.positionTick = t;
      this.emit();
    }
  }

  setMode(mode: PlayMode) {
    if (mode === this.mode) return;
    const was = this.playing;
    if (was) this.halt();
    this.mode = mode;
    this.positionTick = 0;
    if (was) this.play(0);
    this.emit();
  }

  /** Current playback position in ticks (UI thread, interpolated). */
  getTick(): number {
    if (!this.playing) return this.positionTick;
    const now = this.ctx.currentTime;
    let a = this.anchors[0];
    for (const x of this.anchors) {
      if (x.time <= now) a = x;
      else break;
    }
    if (!a) return this.positionTick;
    if (now < a.time) return a.tick;
    return a.tick + (now - a.time) / a.spt;
  }

  /** Tick at a given context time (used by the recorder). */
  tickAt(time: number): number {
    let a = this.anchors[0];
    for (const x of this.anchors) {
      if (x.time <= time) a = x;
      else break;
    }
    return a ? a.tick + Math.max(0, time - a.time) / a.spt : this.positionTick;
  }

  /** Runs one scheduled thing; an exception never stops the transport. */
  private safely(what: string, fn: () => void) {
    try {
      fn();
    } catch (err) {
      diagnostics.count('scheduleErrors');
      diagnostics.log('error', 'schedule', `${what}: ${err instanceof Error ? err.message : String(err)}`, 3000);
    }
  }

  private schedule() {
    if (!this.playing) return;
    const project = this.getProject();
    const spt = 60 / (project.bpm * PPQ);
    const now = this.ctx.currentTime;
    const horizon = now + this.lookahead;
    const [ls, le] = this.loopRange(project);
    if (this.cursorTick >= le || this.cursorTick < ls - TICKS_PER_STEP * 64) {
      this.cursorTick = ls;
      this.anchors.push({ time: this.cursorTime, tick: ls, spt });
    }

    // The UI thread was blocked (big save, garbage collection, a long render …)
    // and the scheduling window has already passed. Scheduling the missed
    // events now would fire all of them at the same instant – a burst of
    // notes that sounds like a crash. Jump to "now" and carry on in time.
    if (this.cursorTime < now + SAFETY) {
      const lateMs = (now + SAFETY - this.cursorTime) * 1000;
      const jump = now + SAFETY + 0.01 - this.cursorTime;
      let tick = this.cursorTick + jump / spt;
      const len = le - ls;
      if (tick >= le && len > 0) tick = ls + ((tick - ls) % len);
      diagnostics.late(lateMs);
      if (lateMs > 25) diagnostics.log('warn', 'late', `Scheduler ${lateMs.toFixed(0)} ms late (UI was busy) – skipped instead of bursting`, 4000);
      this.cursorTick = tick;
      this.cursorTime = now + SAFETY + 0.01;
      this.anchors.push({ time: this.cursorTime, tick, spt });
      // audio clips that should be sounding right now are restarted in place
      if (this.mode === 'song') {
        for (const s of this.sources) {
          try {
            s.gain.gain.cancelScheduledValues(now);
            s.gain.gain.setTargetAtTime(0, now, 0.005);
            s.src.stop(now + 0.03);
          } catch { /* ignore */ }
        }
        this.sources.clear();
        for (const ev of collectAudioSpanning(project, tick, project.bpm)) this.safely('audio clip', () => this.startAudio(ev, this.cursorTime, spt, project));
      }
    }

    let guard = 0;
    while (this.cursorTime < horizon && guard++ < 512) {
      const last = this.anchors[this.anchors.length - 1];
      if (!last || last.spt !== spt) this.anchors.push({ time: this.cursorTime, tick: this.cursorTick, spt });
      const from = this.cursorTick;
      const to = Math.min(from + TICKS_PER_STEP, le);
      const base = this.cursorTime;
      let events: ReturnType<typeof collectNoteEvents> = [];
      this.safely('collect notes', () => { events = collectNoteEvents(project, this.mode, this.patternId, from, to); });
      for (const e of events) {
        this.safely('note', () => this.graph.noteOn(e.channelId, e.pitch, e.velocity, base + (e.tick - from) * spt, e.length * spt));
      }
      if (this.mode === 'song') {
        for (const ev of collectAudioStarts(project, from, to)) this.safely('audio clip', () => this.startAudio(ev, base + (ev.tick - from) * spt, spt, project));
      }
      for (const hook of this.stepHooks) this.safely('step hook', () => hook(from, to, base, spt));
      if (this.metronome) this.safely('metronome', () => this.clicks(project, from, to, base, spt));
      // the cursor always advances, even if something above failed
      this.cursorTime += Math.max(1, to - from) * spt;
      this.cursorTick = to;
      if (this.cursorTick >= le) {
        this.cursorTick = ls;
        this.anchors.push({ time: this.cursorTime, tick: ls, spt });
        if (this.mode === 'song') for (const ev of collectAudioSpanning(project, ls, project.bpm)) this.safely('audio clip', () => this.startAudio(ev, this.cursorTime, spt, project));
      }
    }
    while (this.anchors.length > 2 && this.anchors[1].time < now - 0.5) this.anchors.shift();
    for (const s of this.sources) if (s.end < now) this.sources.delete(s);
  }

  private startAudio(ev: AudioEvent, time: number, spt: number, project: Project) {
    const entry = scheduleAudioClip(this.ctx, this.graph, project, ev, time, spt);
    if (!entry) return;
    entry.src.onended = () => {
      try { entry.gain.disconnect(); } catch { /* ignore */ }
      this.sources.delete(entry);
    };
    this.sources.add(entry);
  }

  /** metronome level 0..1 (Settings → Audio) */
  metronomeVolume = 0.7;

  private clicks(project: Project, from: number, to: number, base: number, spt: number) {
    const beat = PPQ * (4 / project.timeSig[1]);
    const bar = ticksPerBar(project.timeSig);
    let t = Math.ceil(from / beat) * beat;
    for (; t < to; t += beat) {
      const time = base + (t - from) * spt;
      const accent = t % bar === 0;
      const osc = this.ctx.createOscillator();
      osc.frequency.value = accent ? 1760 : 1175;
      const g = this.ctx.createGain();
      g.gain.setValueAtTime(0, time);
      g.gain.linearRampToValueAtTime((accent ? 0.5 : 0.31) * this.metronomeVolume, time + 0.001);
      g.gain.exponentialRampToValueAtTime(0.0001, time + 0.05);
      osc.connect(g).connect(this.graph.out);
      osc.start(time);
      osc.stop(time + 0.06);
    }
  }
}
