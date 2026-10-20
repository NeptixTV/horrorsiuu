// Automation clips: envelopes in the playlist that move a mixer fader / pan,
// a channel's volume / pan / filter or any numeric effect parameter over time.
// Values are stored normalised (0..1). The same evaluation drives live
// playback (transport step hook) and the offline export.
import type { AutomationPoint, AutomationTarget, Clip, Project } from '../state/types';
import { MASTER_ID } from '../state/types';
import { EFFECTS, type NumberParamDef } from './effects/descriptors';
import { silentTracks } from './sequencer';
import { uid } from '../state/utils';
import type { MixGraph } from './mixgraph';

export interface TargetInfo {
  label: string;
  /** normalised 0..1 → real value */
  toValue(v: number): number;
  /** real value → normalised */
  toNorm(x: number): number;
  format(x: number): string;
}

const MAXV = 1.25; // fader law of the mixer / channel volume: v² · 1.25
const db = (g: number) => (g <= 0.00001 ? '−∞ dB' : `${(20 * Math.log10(g)).toFixed(1)} dB`);
const pan = (p: number) => (Math.abs(p) < 0.01 ? 'C' : `${Math.round(Math.abs(p) * 100)}${p < 0 ? 'L' : 'R'}`);

const VOLUME = { toValue: (v: number) => MAXV * v * v, toNorm: (x: number) => Math.sqrt(Math.max(0, x) / MAXV), format: db };
const PAN = { toValue: (v: number) => v * 2 - 1, toNorm: (x: number) => (x + 1) / 2, format: pan };
/** filter offset ±4 octaves (in cents) */
const FILTER = { toValue: (v: number) => (v - 0.5) * 9600, toNorm: (x: number) => x / 9600 + 0.5, format: (x: number) => `${x >= 0 ? '+' : ''}${(x / 1200).toFixed(1)} oct` };

function numberDef(project: Project, t: Extract<AutomationTarget, { kind: 'effect' }>): NumberParamDef | null {
  const fx = project.mixer.find((m) => m.id === t.trackId)?.inserts.find((e) => e.id === t.effectId);
  if (!fx) return null;
  const def = EFFECTS[fx.type].params.find((p) => p.key === t.param);
  return def && def.kind === 'number' ? def : null;
}

export function targetInfo(project: Project, t: AutomationTarget): TargetInfo | null {
  if (t.kind === 'mixer') {
    const m = project.mixer.find((x) => x.id === t.id);
    if (!m) return null;
    const name = m.id === MASTER_ID ? 'Master' : m.name;
    return t.param === 'volume' ? { label: `${name} · Volume`, ...VOLUME } : { label: `${name} · Pan`, ...PAN };
  }
  if (t.kind === 'channel') {
    const c = project.channels.find((x) => x.id === t.id);
    if (!c) return null;
    if (t.param === 'volume') return { label: `${c.name} · Volume`, ...VOLUME };
    if (t.param === 'pan') return { label: `${c.name} · Pan`, ...PAN };
    return { label: `${c.name} · Filter`, ...FILTER };
  }
  const def = numberDef(project, t);
  if (!def) return null;
  const fx = project.mixer.find((m) => m.id === t.trackId)!.inserts.find((e) => e.id === t.effectId)!;
  const strip = project.mixer.find((m) => m.id === t.trackId)!;
  const { min, max } = def;
  const log = def.log && min > 0;
  return {
    label: `${strip.id === MASTER_ID ? 'Master' : strip.name} · ${EFFECTS[fx.type].short} ${def.label}`,
    toValue: (v) => (log ? min * Math.pow(max / min, v) : min + v * (max - min)),
    toNorm: (x) => (log ? Math.log(Math.max(min, x) / min) / Math.log(max / min) : (x - min) / (max - min)),
    format: (x) => `${Math.abs(x) >= 100 ? Math.round(x) : x.toFixed(2)}${def.unit ? ` ${def.unit}` : ''}`,
  };
}

/** Current (static) value of a target, normalised. */
export function currentNorm(project: Project, t: AutomationTarget): number {
  const info = targetInfo(project, t);
  if (!info) return 0.5;
  let x: number;
  if (t.kind === 'mixer') {
    const m = project.mixer.find((k) => k.id === t.id)!;
    x = t.param === 'volume' ? m.volume : m.pan;
  } else if (t.kind === 'channel') {
    const c = project.channels.find((k) => k.id === t.id)!;
    x = t.param === 'volume' ? c.volume : t.param === 'pan' ? c.pan : 0;
  } else {
    const fx = project.mixer.find((m) => m.id === t.trackId)?.inserts.find((e) => e.id === t.effectId);
    const raw = fx?.params[t.param];
    x = typeof raw === 'number' ? raw : 0;
  }
  return Math.max(0, Math.min(1, info.toNorm(x)));
}

/** Linear interpolation of an envelope at clip-relative tick `t`. */
export function valueAt(points: AutomationPoint[], t: number): number {
  if (!points.length) return 0.5;
  if (t <= points[0].t) return points[0].v;
  for (let i = 1; i < points.length; i++) {
    const b = points[i];
    if (t <= b.t) {
      const a = points[i - 1];
      return b.t > a.t ? a.v + ((t - a.t) / (b.t - a.t)) * (b.v - a.v) : b.v;
    }
  }
  return points[points.length - 1].v;
}

export function targetKey(t: AutomationTarget) {
  return t.kind === 'effect' ? `fx:${t.trackId}:${t.effectId}:${t.param}` : `${t.kind}:${t.id}:${t.param}`;
}

/** Automation clips that sound in [from, to) (song mode, unmuted tracks). */
export function activeAutomation(project: Project, from: number, to: number): Clip[] {
  const silent = silentTracks(project);
  return project.clips.filter((c) => c.kind === 'automation' && c.automation && !silent.has(c.trackId) && c.start < to && c.start + c.length > from);
}

/** The AudioParam an automation target moves directly (null = effect parameter). */
export function directParam(graph: MixGraph, t: AutomationTarget): AudioParam | null {
  if (t.kind === 'mixer') {
    const s = graph.strips.get(t.id);
    return s ? (t.param === 'volume' ? s.fader.gain : s.panner.pan) : null;
  }
  if (t.kind === 'channel') {
    const h = graph.channels.get(t.id);
    if (!h) return null;
    if (t.param === 'volume') return h.volume.gain;
    if (t.param === 'pan') return h.panner.pan;
    return h.instrument.filterOffset ?? null;
  }
  return null;
}

/** Applies an effect parameter value to the live effect (keeps the other params). */
export function applyEffectValue(project: Project, graph: MixGraph, t: Extract<AutomationTarget, { kind: 'effect' }>, value: number) {
  const slot = graph.strips.get(t.trackId)?.slot(t.effectId);
  if (!slot) return;
  slot.effect.update({ ...slot.instance.params, [t.param]: value }, { bpm: project.bpm });
}

/**
 * Schedules the automation of one step window [from, to) starting at `base`
 * seconds. Direct params get sample-accurate ramps; effect parameters are
 * handed to `later(time, fn)` (setTimeout live, suspend() offline).
 */
export function scheduleAutomation(
  project: Project, graph: MixGraph, from: number, to: number, base: number, spt: number,
  later: (time: number, fn: () => void) => void, touched?: Map<string, AutomationTarget>,
) {
  for (const clip of activeAutomation(project, from, to)) {
    const a = clip.automation!;
    const info = targetInfo(project, a.target);
    if (!info) continue;
    const key = targetKey(a.target);
    const first = !touched?.has(key);
    touched?.set(key, a.target);
    const t0 = Math.max(from, clip.start);
    const t1 = Math.min(to, clip.start + clip.length);
    // points are relative to the clip content (offset moves when the left edge is trimmed)
    const v0 = info.toValue(valueAt(a.points, t0 - clip.start + clip.offset));
    const v1 = info.toValue(valueAt(a.points, t1 - clip.start + clip.offset));
    const time0 = base + (t0 - from) * spt;
    const time1 = base + (t1 - from) * spt;
    const p = directParam(graph, a.target);
    if (p) {
      try {
        // cancel only once per playback: later steps continue the previous ramp
        if (first) p.cancelScheduledValues(time0);
        p.setValueAtTime(v0, time0);
        p.linearRampToValueAtTime(v1, time1);
      } catch { /* out of range for this param */ }
    } else if (a.target.kind === 'effect') {
      const target = a.target;
      later(time0, () => applyEffectValue(project, graph, target, v0));
    }
  }
}

/** Puts every automated target back to its value in the project (after stop). */
export function restoreAutomated(project: Project, graph: MixGraph, touched: Map<string, AutomationTarget>) {
  const now = graph.ctx.currentTime;
  for (const t of touched.values()) {
    const p = directParam(graph, t);
    if (p) {
      const info = targetInfo(project, t);
      if (!info) continue;
      const x = t.kind === 'channel' && t.param === 'cutoff' ? 0 : info.toValue(currentNorm(project, t));
      try {
        p.cancelScheduledValues(now);
        p.setTargetAtTime(x, now, 0.02);
      } catch { /* ignore */ }
    } else if (t.kind === 'effect') {
      const slot = graph.strips.get(t.trackId)?.slot(t.effectId);
      if (slot) slot.effect.update(slot.instance.params, { bpm: project.bpm });
    }
  }
  touched.clear();
}

/** Envelope shapes for the automation editor (normalised values, `len` ticks, `cycle` ticks per period). */
export function shapePoints(shape: 'flat' | 'rampUp' | 'rampDown' | 'sine' | 'saw' | 'square' | 'random', len: number, cycle: number, current = 0.5): AutomationPoint[] {
  const pts: AutomationPoint[] = [];
  const cyc = Math.max(24, cycle);
  switch (shape) {
    case 'flat': return [{ t: 0, v: current }, { t: len, v: current }];
    case 'rampUp': return [{ t: 0, v: 0 }, { t: len, v: 1 }];
    case 'rampDown': return [{ t: 0, v: 1 }, { t: len, v: 0 }];
    case 'sine': {
      const step = cyc / 16;
      for (let t = 0; t <= len + 0.01; t += step) pts.push({ t: Math.min(len, t), v: 0.5 + 0.5 * Math.sin((t / cyc) * Math.PI * 2) });
      return pts;
    }
    case 'saw':
      for (let t = 0; t < len; t += cyc) {
        pts.push({ t, v: 0 }, { t: Math.min(len, t + cyc - 1), v: 1 });
      }
      return pts;
    case 'square':
      for (let t = 0; t < len; t += cyc) {
        const h = Math.min(len, t + cyc / 2);
        pts.push({ t, v: 1 }, { t: Math.max(t, h - 1), v: 1 }, { t: h, v: 0 }, { t: Math.min(len, t + cyc - 1), v: 0 });
      }
      return pts;
    case 'random': {
      let seed = 1234;
      for (let t = 0; t < len; t += cyc) {
        seed = (seed * 1664525 + 1013904223) >>> 0;
        const v = seed / 4294967296;
        pts.push({ t, v }, { t: Math.min(len, t + cyc - 1), v });
      }
      return pts;
    }
  }
}

/** A new automation clip for a target (two points at the current value). */
export function makeAutomationClip(project: Project, target: AutomationTarget, trackId: string, start: number, length: number): Clip {
  const v = currentNorm(project, target);
  return {
    id: uid('c'), trackId, kind: 'automation', start, length, offset: 0,
    name: targetInfo(project, target)?.label ?? 'Automation',
    automation: { target, points: [{ t: 0, v }, { t: length, v }] },
  };
}
