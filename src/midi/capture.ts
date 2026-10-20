// "Capture": the studio quietly remembers what you played in the last few
// minutes (DreamChord, MIDI keyboard, computer keyboard) – even when you were
// not recording. One click turns that idea into a new pattern, so a great
// improvisation is never lost.
import type { Note, Pattern } from '../state/types';
import { PPQ, TICKS_PER_STEP } from '../state/types';
import { getProject, update } from '../state/store';
import { setUI, toast } from '../state/ui';
import { paletteColor, ticksPerBar, uid } from '../state/utils';

export interface PlayedNote {
  channelId: string;
  pitch: number;
  velocity: number;
  start: number; // seconds (AudioContext time)
  end: number | null;
}

const KEEP_SECONDS = 180;
/** a pause longer than this separates two ideas */
const PHRASE_GAP = 4;

type Listener = () => void;

class CaptureBuffer {
  notes: PlayedNote[] = [];
  private open = new Map<string, PlayedNote>();
  private listeners = new Set<Listener>();

  onChange(l: Listener): () => void {
    this.listeners.add(l);
    return () => { this.listeners.delete(l); };
  }

  private changed(now: number) {
    const cut = now - KEEP_SECONDS;
    if (this.notes.length && this.notes[0].start < cut) this.notes = this.notes.filter((n) => n.start >= cut);
    this.listeners.forEach((l) => l());
  }

  noteOn(channelId: string, pitch: number, velocity: number, time: number) {
    const key = `${channelId}:${pitch}`;
    const prev = this.open.get(key);
    if (prev && prev.end === null) prev.end = time;
    const n: PlayedNote = { channelId, pitch, velocity, start: time, end: null };
    this.notes.push(n);
    this.open.set(key, n);
    this.changed(time);
  }

  noteOff(channelId: string, pitch: number, time: number) {
    const key = `${channelId}:${pitch}`;
    const n = this.open.get(key);
    if (n && n.end === null) n.end = Math.max(n.start + 0.02, time);
    this.open.delete(key);
  }

  /** A note with a known length (arpeggiator, strum). */
  note(channelId: string, pitch: number, velocity: number, time: number, seconds: number) {
    this.notes.push({ channelId, pitch, velocity, start: time, end: time + seconds });
    this.changed(time);
  }

  /** The most recent musical idea: notes since the last long pause. */
  lastPhrase(): PlayedNote[] {
    const list = this.notes;
    if (!list.length) return [];
    // prefix maximum of note ends → linear scan from the back
    const maxEnd: number[] = [];
    let m = -Infinity;
    for (const n of list) {
      m = Math.max(m, n.end ?? n.start);
      maxEnd.push(m);
    }
    let i = list.length - 1;
    while (i > 0 && list[i].start - maxEnd[i - 1] <= PHRASE_GAP) i--;
    return list.slice(i);
  }

  clear() {
    this.notes = [];
    this.open.clear();
    this.listeners.forEach((l) => l());
  }
}

export const capture = new CaptureBuffer();

/** Turns the last played phrase into a new pattern (quantised to 1/16). Returns its id. */
export function captureToPattern(now: number): string | null {
  const phrase = capture.lastPhrase();
  if (!phrase.length) {
    toast('Nothing played yet – play a few chords or notes first', 'info', true);
    return null;
  }
  const p = getProject();
  const spt = 60 / (p.bpm * PPQ);
  const t0 = phrase[0].start;
  const q = TICKS_PER_STEP;
  const bar = ticksPerBar(p.timeSig);
  const notes: Record<string, Note[]> = {};
  let endTick = 0;
  for (const n of phrase) {
    if (!p.channels.some((c) => c.id === n.channelId)) continue;
    const start = Math.round((n.start - t0) / spt / q) * q;
    const len = Math.max(q / 2, Math.round(((n.end ?? now) - n.start) / spt / (q / 2)) * (q / 2));
    (notes[n.channelId] ??= []).push({ id: uid('n'), pitch: n.pitch, start, length: len, velocity: Math.max(0.05, Math.min(1, n.velocity)) });
    endTick = Math.max(endTick, start + len);
  }
  if (!Object.keys(notes).length) return null;
  const bars = Math.max(1, Math.min(64, Math.ceil(endTick / bar)));
  const id = uid('pt');
  const pattern: Pattern = {
    id, name: `Captured ${new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}`, color: paletteColor(p.patterns.length * 3 + 1),
    lengthSteps: (bars * bar) / TICKS_PER_STEP, steps: {}, notes,
  };
  update('Capture played notes', (d) => { d.patterns.push(pattern); });
  setUI({ selectedPatternId: id });
  const count = Object.values(notes).reduce((n, l) => n + l.length, 0);
  toast(`Captured ${count} notes (${bars} bar${bars === 1 ? '' : 's'}) into a new pattern`, 'ok');
  return id;
}
