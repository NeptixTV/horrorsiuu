// DreamChord performance engine: turns button presses and joystick moves into
// notes. Handles strumming, hold (latch), the arpeggiator (locked to the song
// grid while the transport runs, free-running otherwise), live recording into
// patterns and the "Capture" buffer.
import type { DreamParams } from '../../state/types';
import { PPQ } from '../../state/types';
import { getProject } from '../../state/store';
import { engine } from '../../audio/engine';
import { DreamSynth } from '../../audio/instruments/dream';
import {
  arpSequence, arpStepTicks, diatonicChord, joyDirection, JOY_MODS, voiceChord, type Chord, type JoyDir,
} from '../../midi/chords';
import { liveRecorder } from '../../midi/liveRecord';
import { capture } from '../../midi/capture';

export interface PerformerView {
  degree: number | null;
  chord: Chord | null;
  notes: number[];
  dir: JoyDir;
  joy: { x: number; y: number };
  latched: boolean;
  arpIndex: number;
  /** increments on every chord trigger (drives the display animation) */
  hits: number;
}

type Listener = () => void;

export class DreamPerformer {
  private held: number[] = [];
  private latchedDegree: number | null = null;
  private sounding = new Map<number, number>(); // pitch → velocity (sustained notes)
  private notes: number[] = [];
  private chord: Chord | null = null;
  private dir: JoyDir = 'c';
  private joy = { x: 0, y: 0 };
  private velocity = 0.85;
  private arpPos = 0;
  private arpLast = -1;
  private arpTimer: ReturnType<typeof setInterval> | null = null;
  private arpNext = 0;
  private hits = 0;
  private listeners = new Set<Listener>();
  view: PerformerView;

  constructor(readonly channelId: string) {
    this.view = this.snapshot();
  }

  // ---------- state ----------

  private params(): DreamParams | null {
    return getProject().channels.find((c) => c.id === this.channelId)?.dream ?? null;
  }

  private synth(): DreamSynth | null {
    if (!engine.initialized) return null;
    const inst = engine.graph.channels.get(this.channelId)?.instrument;
    return inst instanceof DreamSynth ? inst : null;
  }

  private snapshot(): PerformerView {
    return {
      degree: this.activeDegree(),
      chord: this.chord,
      notes: this.notes,
      dir: this.dir,
      joy: { ...this.joy },
      latched: this.latchedDegree !== null,
      arpIndex: this.arpLast,
      hits: this.hits,
    };
  }

  onChange(l: Listener): () => void {
    this.listeners.add(l);
    return () => { this.listeners.delete(l); };
  }

  private emit() {
    this.view = this.snapshot();
    this.listeners.forEach((l) => l());
  }

  private activeDegree(): number | null {
    return this.held.length ? this.held[this.held.length - 1] : this.latchedDegree;
  }

  get isArp() {
    return (this.params()?.arp ?? 'off') !== 'off';
  }

  // ---------- note output ----------

  private now() {
    return engine.ctx.currentTime + 0.005;
  }

  private on(pitch: number, vel: number, at: number) {
    engine.previewNote(this.channelId, pitch, vel, undefined, at);
    this.sounding.set(pitch, vel);
    liveRecorder.noteOn(this.channelId, pitch, vel, at);
    capture.noteOn(this.channelId, pitch, vel, at);
  }

  private off(pitch: number, at: number) {
    engine.previewNoteOff(this.channelId, pitch, at);
    this.sounding.delete(pitch);
    liveRecorder.noteOff(this.channelId, pitch, at);
    capture.noteOff(this.channelId, pitch, at);
  }

  private allOff(at = this.now()) {
    for (const p of [...this.sounding.keys()]) this.off(p, at);
  }

  /** Plays `next` – retriggering everything (new chord) or only what changed (joystick). */
  private voice(next: number[], retrigger: boolean) {
    const p = this.params();
    if (!p || !engine.initialized) return;
    const t0 = this.now();
    if (this.isArp) {
      // the arpeggiator plays the notes; only sustained leftovers need to stop
      this.allOff(t0);
      this.notes = next;
      this.ensureArp();
      return;
    }
    const strum = p.strum / 1000;
    const order = p.strumDir === 'down' ? [...next].sort((a, b) => b - a) : [...next].sort((a, b) => a - b);
    if (retrigger) {
      this.allOff(t0);
      order.forEach((pitch, i) => this.on(pitch, this.velocity * (1 - i * 0.03), t0 + 0.002 + i * strum));
    } else {
      const keep = new Set(next);
      for (const pitch of [...this.sounding.keys()]) if (!keep.has(pitch)) this.off(pitch, t0);
      let i = 0;
      for (const pitch of order) {
        if (this.sounding.has(pitch)) continue;
        this.on(pitch, this.velocity * 0.9, t0 + i * strum * 0.5);
        i++;
      }
    }
    this.notes = next;
  }

  private computeNotes(degree: number): number[] {
    const p = this.params();
    if (!p) return [];
    const chord = diatonicChord(p.key, p.mode, degree - 1, JOY_MODS[this.dir]);
    this.chord = chord;
    return voiceChord(chord, p.octave, p.voicing, this.notes.length ? this.notes : null);
  }

  // ---------- buttons ----------

  press(degree: number, velocity = 0.85) {
    const p = this.params();
    if (!p || !engine.initialized) return;
    void engine.resume();
    this.velocity = Math.max(0.1, Math.min(1, velocity));
    if (p.hold && this.latchedDegree === degree && this.held.length === 0) {
      // pressing the latched chord again stops it
      this.latchedDegree = null;
      this.stopSound();
      this.emit();
      return;
    }
    this.held = this.held.filter((d) => d !== degree);
    this.held.push(degree);
    if (p.hold) this.latchedDegree = degree;
    this.hits++;
    this.arpPos = 0;
    this.voice(this.computeNotes(degree), true);
    this.emit();
  }

  release(degree: number) {
    if (!this.held.includes(degree)) return;
    this.held = this.held.filter((d) => d !== degree);
    const p = this.params();
    const next = this.activeDegree();
    if (next !== null && (this.held.length || p?.hold)) {
      if (next !== degree) this.voice(this.computeNotes(next), true);
    } else {
      this.stopSound();
    }
    this.emit();
  }

  private stopSound() {
    this.allOff();
    this.stopArp();
    this.notes = [];
    this.chord = null;
  }

  /** Hold switched off: release a latched chord that is not physically held. */
  unlatch() {
    this.latchedDegree = null;
    if (!this.held.length) this.stopSound();
    this.emit();
  }

  panic() {
    this.held = [];
    this.latchedDegree = null;
    this.stopSound();
    this.synth()?.setBend(0);
    this.synth()?.setFilterMod(0);
    this.emit();
  }

  /** Re-voices the chord that sounds (key / voicing / octave changed). */
  refresh() {
    const d = this.activeDegree();
    if (d === null) return;
    this.voice(this.computeNotes(d), false);
    this.emit();
  }

  // ---------- joystick ----------

  setJoystick(x: number, y: number) {
    const p = this.params();
    if (!p) return;
    this.joy = { x: Math.max(-1, Math.min(1, x)), y: Math.max(-1, Math.min(1, y)) };
    const s = this.synth();
    if (p.joystick === 'chord') {
      const dir = joyDirection(this.joy.x, this.joy.y);
      if (dir !== this.dir) {
        this.dir = dir;
        const d = this.activeDegree();
        if (d !== null) this.voice(this.computeNotes(d), false);
      }
      // a little life on top: pushing further opens the filter slightly
      s?.setFilterMod(Math.hypot(this.joy.x, this.joy.y) * 0.4);
    } else if (p.joystick === 'bend') {
      s?.setBend(this.joy.x * 2);
      s?.setExpression(Math.max(0, this.joy.y));
      s?.setFilterMod(Math.min(0, this.joy.y) * 2);
    } else {
      s?.setFilterMod(this.joy.y * 2.5);
      s?.setExpression(Math.abs(this.joy.x));
    }
    this.emit();
  }

  /** The stick springs back to the centre. */
  releaseJoystick() {
    const p = this.params();
    this.joy = { x: 0, y: 0 };
    const s = this.synth();
    s?.setBend(0, 0.08);
    s?.setFilterMod(0);
    s?.setExpression(0);
    if (p?.joystick === 'chord' && this.dir !== 'c') {
      this.dir = 'c';
      const d = this.activeDegree();
      if (d !== null) this.voice(this.computeNotes(d), false);
    }
    this.emit();
  }

  // ---------- arpeggiator ----------

  private ensureArp() {
    if (this.arpTimer || !this.notes.length) return;
    // locked to the song while the transport runs (see installArpClock), free-running otherwise
    this.arpNext = engine.ctx.currentTime + 0.01;
    this.arpTimer = setInterval(() => this.freeRun(), 20);
  }

  private stopArp() {
    if (this.arpTimer) clearInterval(this.arpTimer);
    this.arpTimer = null;
    this.arpLast = -1;
  }

  private freeRun() {
    if (!this.notes.length || !this.isArp) {
      this.stopArp();
      return;
    }
    if (engine.transport?.playing) return; // the song clock drives it
    const p = getProject();
    const dp = this.params()!;
    const step = arpStepTicks(dp.arpRate, PPQ) * (60 / (p.bpm * PPQ));
    const horizon = engine.ctx.currentTime + 0.1;
    if (this.arpNext < engine.ctx.currentTime) this.arpNext = engine.ctx.currentTime + 0.005;
    while (this.arpNext < horizon) {
      this.arpStep(this.arpNext, step);
      this.arpNext += step;
    }
  }

  /** One arpeggiator step at context time `time` lasting `stepSeconds`. */
  arpStep(time: number, stepSeconds: number) {
    const dp = this.params();
    if (!dp || !this.notes.length) return;
    const dur = Math.max(0.02, stepSeconds * dp.arpGate);
    const vel = this.velocity * (this.arpPos % 4 === 0 ? 1 : 0.82);
    const play = (pitch: number) => {
      engine.previewNote(this.channelId, pitch, vel, dur, time);
      liveRecorder.note(this.channelId, pitch, vel, time, dur);
      capture.note(this.channelId, pitch, vel, time, dur);
    };
    if (dp.arp === 'chord') {
      for (const n of this.notes) play(n);
      this.arpLast = 0;
    } else {
      const seq = arpSequence(this.notes, dp.arp === 'random' || dp.arp === 'off' ? 'up' : dp.arp, dp.arpOctaves);
      let idx: number;
      if (dp.arp === 'random') {
        idx = Math.floor(Math.random() * seq.length);
        if (seq.length > 1 && idx === this.arpLast) idx = (idx + 1) % seq.length;
      } else {
        idx = this.arpPos % seq.length;
      }
      play(seq[idx]);
      this.arpLast = idx;
    }
    this.arpPos++;
    setTimeout(() => this.emit(), Math.max(0, (time - engine.ctx.currentTime) * 1000));
  }

  /** Called by the transport for every 16th window while the song plays. */
  onTransportStep(from: number, to: number, base: number, spt: number) {
    if (!this.notes.length || !this.isArp) return;
    const dp = this.params()!;
    const stepTicks = arpStepTicks(dp.arpRate, PPQ);
    let k = Math.ceil(from / stepTicks - 1e-9);
    for (; k * stepTicks < to; k++) {
      this.arpStep(base + (k * stepTicks - from) * spt, stepTicks * spt);
    }
  }
}

const performers = new Map<string, DreamPerformer>();

export function getPerformer(channelId: string): DreamPerformer {
  let p = performers.get(channelId);
  if (!p) performers.set(channelId, (p = new DreamPerformer(channelId)));
  return p;
}

let clockInstalled = false;
/** Lets the transport drive every active arpeggiator (sample-accurate, in time with the song). */
export function installArpClock() {
  if (clockInstalled) return;
  clockInstalled = true;
  engine.stepHooks.add((from, to, base, spt) => {
    for (const p of performers.values()) p.onTransportStep(from, to, base, spt);
  });
}
