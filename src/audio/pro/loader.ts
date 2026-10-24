// Loads Pro Sounds on demand. Samples are only fetched and decoded when a
// channel uses them, and big multi-sampled instruments load progressively:
// first one velocity layer over the whole keyboard (it can play right away),
// the other layers follow in the background or when a note needs them.
// Decoded audio is shared by every channel and audio context.
import { create } from 'zustand';
import type { Project } from '../../state/types';
import { proZones, type ProZone } from './catalog';

const SR = 48000;
const MAX_PARALLEL = 6;

interface ZoneState {
  buffer: AudioBuffer | null;
  promise: Promise<AudioBuffer | null> | null;
  failed: boolean;
}

const zones = new Map<string, ZoneState>(); // by file
const queue: { z: ProZone; resolve: (b: AudioBuffer | null) => void }[] = [];
let running = 0;
let decoder: OfflineAudioContext | null = null;
let warned = false;

export const useProLoading = create<{ pending: number; failed: number }>(() => ({ pending: 0, failed: 0 }));
const listeners = new Set<() => void>();

/** Called whenever a zone finished loading (instruments use it to retry). */
export function onProLoaded(fn: () => void) {
  listeners.add(fn);
  return () => { listeners.delete(fn); };
}

function urlOf(file: string) {
  const base = typeof document !== 'undefined' ? document.baseURI : 'http://localhost/';
  return new URL(`sounds/${file}`, base).href;
}

function state(file: string): ZoneState {
  let s = zones.get(file);
  if (!s) zones.set(file, (s = { buffer: null, promise: null, failed: false }));
  return s;
}

async function fetchAndDecode(z: ProZone): Promise<AudioBuffer | null> {
  try {
    const res = await fetch(urlOf(z.f));
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.arrayBuffer();
    decoder ??= new OfflineAudioContext(2, 1, SR);
    return await decoder.decodeAudioData(data);
  } catch (err) {
    if (!warned) {
      warned = true;
      console.warn('[GoofyStudio] Pro sound could not be loaded:', z.f, err);
    }
    return null;
  }
}

function pump() {
  while (running < MAX_PARALLEL && queue.length) {
    const job = queue.shift()!;
    running++;
    void fetchAndDecode(job.z).then((b) => {
      running--;
      const s = state(job.z.f);
      s.buffer = b;
      s.failed = !b;
      useProLoading.setState((st) => ({ pending: Math.max(0, st.pending - 1), failed: st.failed + (b ? 0 : 1) }));
      job.resolve(b);
      for (const l of listeners) { try { l(); } catch { /* ignore */ } }
      pump();
    });
  }
}

/** Starts loading one zone (urgent ones jump the queue). */
export function loadZone(z: ProZone, urgent = false): Promise<AudioBuffer | null> {
  const s = state(z.f);
  if (s.buffer) return Promise.resolve(s.buffer);
  if (s.failed) return Promise.resolve(null);
  if (!s.promise) {
    s.promise = new Promise((resolve) => {
      const job = { z, resolve };
      if (urgent) queue.unshift(job);
      else queue.push(job);
    });
    useProLoading.setState((st) => ({ pending: st.pending + 1 }));
    pump();
  } else if (urgent) {
    // move a queued job to the front
    const i = queue.findIndex((j) => j.z.f === z.f);
    if (i > 0) queue.unshift(...queue.splice(i, 1));
  }
  return s.promise;
}

export function zoneBuffer(z: ProZone): AudioBuffer | null {
  return zones.get(z.f)?.buffer ?? null;
}

export function zoneFailed(z: ProZone): boolean {
  return zones.get(z.f)?.failed ?? false;
}

/** Velocity (0..1) → MIDI velocity 1..127. */
export const midiVel = (v: number) => Math.max(1, Math.min(127, Math.round(v * 127)));

/** Zones that cover this key and velocity (several = round robin). */
export function zonesFor(all: ProZone[], pitch: number, vel127: number): ProZone[] {
  const key = Math.max(0, Math.min(127, Math.round(pitch)));
  const out = all.filter((z) => key >= z.lo && key <= z.hi && vel127 >= z.vl && vel127 <= z.vh);
  if (out.length) return out;
  // outside the mapped range: the nearest key
  let best: ProZone[] = [];
  let dist = Infinity;
  for (const z of all) {
    if (vel127 < z.vl || vel127 > z.vh) continue;
    const d = key < z.lo ? z.lo - key : key - z.hi;
    if (d < dist) { dist = d; best = [z]; } else if (d === dist) best.push(z);
  }
  return best;
}

/** The layer that should be loaded first: the one a typical velocity (0.8) uses. */
export function coreZones(all: ProZone[]): ProZone[] {
  const v = midiVel(0.8);
  const core = all.filter((z) => v >= z.vl && v <= z.vh && !z.rr);
  return core.length ? core : all;
}

/** Loads the zones a sound needs: the core layer first, then (optionally) the rest. */
export function preloadSound(sound: string, everything = true): Promise<void> {
  const all = proZones(sound);
  const core = coreZones(all);
  const first = Promise.all(core.map((z) => loadZone(z))).then(() => undefined);
  if (everything) for (const z of all) void loadZone(z);
  return first;
}

/** Zones the notes/steps of a project need for every Pro channel. */
export function projectZones(project: Project): ProZone[] {
  const need = new Map<string, ProZone>();
  for (const ch of project.channels) {
    if (ch.kind !== 'pro' || !ch.pro) continue;
    const all = proZones(ch.pro.sound);
    const add = (pitch: number, vel: number) => { for (const z of zonesFor(all, pitch, midiVel(vel))) need.set(z.f, z); };
    for (const pt of project.patterns) {
      for (const v of pt.steps[ch.id] ?? []) if (v > 0) add(ch.rootNote, v);
      for (const n of pt.notes[ch.id] ?? []) add(n.pitch, n.velocity);
      for (const c of project.clips) {
        if (c.kind !== 'pattern' || c.patternId !== pt.id || !c.transpose) continue;
        for (const n of pt.notes[ch.id] ?? []) add(n.pitch + c.transpose, n.velocity);
      }
    }
    for (const z of coreZones(all)) need.set(z.f, z);
  }
  return [...need.values()];
}

/**
 * Waits until everything the project plays is loaded (or `timeoutMs` passed).
 * Returns the number of sounds that could not be loaded.
 */
export async function ensureProjectSounds(project: Project, timeoutMs = Infinity): Promise<number> {
  const list = projectZones(project).filter((z) => !zoneBuffer(z));
  if (!list.length) return 0;
  const all = Promise.all(list.map((z) => loadZone(z, true)));
  if (timeoutMs === Infinity) await all;
  else await Promise.race([all, new Promise((r) => setTimeout(r, timeoutMs))]);
  return list.filter((z) => zoneFailed(z)).length;
}

/** Frees decoded audio of sounds that no channel uses any more. */
export function releaseUnused(used: Set<string>) {
  const keep = new Set<string>();
  for (const id of used) for (const z of proZones(id)) keep.add(z.f);
  for (const [file, s] of zones) {
    if (keep.has(file) || !s.buffer) continue;
    zones.delete(file);
  }
}

/** Approximate memory of the decoded Pro audio in bytes. */
export function proMemory(): number {
  let n = 0;
  for (const s of zones.values()) if (s.buffer) n += s.buffer.length * s.buffer.numberOfChannels * 4;
  return n;
}
