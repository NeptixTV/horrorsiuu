// Offline mixdown: rebuilds the project graph inside an OfflineAudioContext,
// schedules the whole song (or pattern) at once – notes, audio clips and
// automation – and encodes the result as WAV. Can also render every mixer
// insert separately ("stems") in the same pass.
import type { AutomationTarget, Project } from '../state/types';
import { MASTER_ID, PPQ, TICKS_PER_STEP } from '../state/types';
import { MixGraph } from './mixgraph';
import { loadWorklets, markWorkletsReady } from './worklets';
import { scheduleAudioClip } from './clipPlayback';
import { scheduleAutomation } from './automation';
import {
  collectAudioSpanning, collectAudioStarts, collectNoteEvents, patternLength, songLength, type PlayMode,
} from './sequencer';

export interface RenderOptions {
  mode: PlayMode;
  patternId?: string | null;
  sampleRate?: number;
  tail?: number; // seconds of release/reverb tail
  /** 'auto' = loop region if enabled, else whole song; 'song' = always whole song */
  range?: 'auto' | 'song' | 'loop';
  onProgress?: (p: number) => void;
}

export interface Stem {
  id: string;
  name: string;
  buffer: AudioBuffer;
}

function rangeOf(project: Project, opts: RenderOptions): [number, number] {
  if (opts.mode === 'pattern') {
    const p = project.patterns.find((x) => x.id === opts.patternId) ?? project.patterns[0];
    return [0, p ? patternLength(p, project.timeSig) : PPQ * 4];
  }
  const range = opts.range ?? 'auto';
  if (range !== 'song' && project.loop.end > project.loop.start && (range === 'loop' || project.loop.enabled)) return [project.loop.start, project.loop.end];
  return [0, songLength(project)];
}

/**
 * One OfflineAudioContext.suspend() per render quantum; everything that has
 * to happen at that moment (automation of effect params, progress) shares it.
 */
class SuspendQueue {
  private map = new Map<number, (() => void)[]>();
  constructor(private ctx: OfflineAudioContext) {}

  at(time: number, fn: () => void) {
    const ctx = this.ctx;
    const frame = Math.ceil((time * ctx.sampleRate) / 128) * 128;
    if (frame <= 0 || frame >= ctx.length) return;
    let list = this.map.get(frame);
    if (!list) {
      const l: (() => void)[] = [];
      this.map.set(frame, l);
      list = l;
      ctx.suspend(frame / ctx.sampleRate).then(() => {
        for (const f of l) {
          try { f(); } catch { /* keep rendering */ }
        }
        return ctx.resume();
      }).catch(() => undefined);
    }
    list.push(fn);
  }
}

/** Schedules notes, audio clips and automation of [fromTick, toTick) into an offline graph. */
function scheduleAll(ctx: OfflineAudioContext, graph: MixGraph, project: Project, opts: RenderOptions, fromTick: number, toTick: number, q: SuspendQueue) {
  const spt = 60 / (project.bpm * PPQ);
  const lead = 0.01;
  for (const e of collectNoteEvents(project, opts.mode, opts.patternId ?? null, fromTick, toTick)) {
    graph.noteOn(e.channelId, e.pitch, e.velocity, lead + (e.tick - fromTick) * spt, e.length * spt);
  }
  if (opts.mode !== 'song') return;
  for (const ev of collectAudioStarts(project, fromTick, toTick)) {
    scheduleAudioClip(ctx, graph, project, { ...ev, remaining: Math.min(ev.remaining, toTick - ev.tick) }, lead + (ev.tick - fromTick) * spt, spt);
  }
  // clips that already run at the start of the loop region
  for (const ev of collectAudioSpanning(project, fromTick, project.bpm)) {
    scheduleAudioClip(ctx, graph, project, { ...ev, remaining: Math.min(ev.remaining, toTick - ev.tick) }, lead, spt);
  }
  // automation: direct params as ramps, effect params at render-quantum suspends
  const touched = new Map<string, AutomationTarget>();
  for (let t = fromTick; t < toTick; t += TICKS_PER_STEP) {
    const to = Math.min(toTick, t + TICKS_PER_STEP);
    scheduleAutomation(project, graph, t, to, lead + (t - fromTick) * spt, spt, (time, fn) => q.at(time, fn), touched);
  }
}

function progressSuspends(ctx: OfflineAudioContext, q: SuspendQueue, onProgress?: (p: number) => void) {
  if (!onProgress) return;
  const total = ctx.length / ctx.sampleRate;
  for (let i = 1; i < 20; i++) q.at((total * i) / 20, () => onProgress(i / 20));
}

export async function renderProject(project: Project, opts: RenderOptions): Promise<AudioBuffer> {
  const sr = opts.sampleRate ?? 44100;
  const spt = 60 / (project.bpm * PPQ);
  const [fromTick, toTick] = rangeOf(project, opts);
  const duration = (toTick - fromTick) * spt + (opts.tail ?? 2.5);
  const ctx = new OfflineAudioContext(2, Math.max(1, Math.ceil(duration * sr)), sr);
  if (await loadWorklets(ctx)) markWorkletsReady(ctx);
  const graph = new MixGraph(ctx);
  graph.sync(project);
  const q = new SuspendQueue(ctx);
  scheduleAll(ctx, graph, project, opts, fromTick, toTick, q);
  progressSuspends(ctx, q, opts.onProgress);
  const out = await ctx.startRendering();
  opts.onProgress?.(1);
  return out;
}

/** Mixer inserts that receive any signal (instruments, tracks, sends or routing). */
export function stemSources(project: Project) {
  const used = new Set<string>();
  for (const c of project.channels) used.add(c.mixerTrackId);
  for (const t of project.tracks) if (project.clips.some((c) => c.trackId === t.id && c.kind === 'audio')) used.add(t.mixerTrackId);
  for (const m of project.mixer) {
    for (const s of m.sends) used.add(s.target);
    if (m.output && used.has(m.id)) used.add(m.output);
  }
  return project.mixer.filter((m) => m.id !== MASTER_ID && used.has(m.id));
}

/**
 * Renders the post-fader output of every used mixer insert (as you hear it in
 * the mix, effect returns included) plus the master, in one pass per 15 stems.
 */
export async function renderStems(project: Project, opts: RenderOptions): Promise<Stem[]> {
  const sr = opts.sampleRate ?? 44100;
  const spt = 60 / (project.bpm * PPQ);
  const [fromTick, toTick] = rangeOf(project, opts);
  const duration = (toTick - fromTick) * spt + (opts.tail ?? 2.5);
  const sources = [project.mixer[0], ...stemSources(project)];
  const out: Stem[] = [];
  const PER_PASS = 15;
  for (let i = 0; i < sources.length; i += PER_PASS) {
    const group = sources.slice(i, i + PER_PASS);
    const ctx = new OfflineAudioContext(group.length * 2, Math.max(1, Math.ceil(duration * sr)), sr);
    if (await loadWorklets(ctx)) markWorkletsReady(ctx);
    // the normal master output goes nowhere; each stem gets its own channel pair
    const sink = ctx.createGain();
    const graph = new MixGraph(ctx, sink);
    graph.sync(project);
    const merger = ctx.createChannelMerger(group.length * 2);
    ctx.destination.channelCount = group.length * 2;
    ctx.destination.channelCountMode = 'explicit';
    ctx.destination.channelInterpretation = 'discrete';
    merger.connect(ctx.destination);
    group.forEach((m, k) => {
      const strip = graph.strips.get(m.id);
      if (!strip) return;
      const split = ctx.createChannelSplitter(2);
      strip.output.connect(split);
      split.connect(merger, 0, k * 2);
      split.connect(merger, 1, k * 2 + 1);
    });
    const q = new SuspendQueue(ctx);
    scheduleAll(ctx, graph, project, opts, fromTick, toTick, q);
    const base = i / sources.length;
    progressSuspends(ctx, q, opts.onProgress ? (p) => opts.onProgress!(base + p * (group.length / sources.length)) : undefined);
    const res = await ctx.startRendering();
    group.forEach((m, k) => {
      const b = new AudioBuffer({ length: res.length, numberOfChannels: 2, sampleRate: sr });
      b.copyToChannel(res.getChannelData(k * 2), 0);
      b.copyToChannel(res.getChannelData(k * 2 + 1), 1);
      out.push({ id: m.id, name: m.id === MASTER_ID ? 'Master (full mix)' : m.name, buffer: b });
    });
  }
  opts.onProgress?.(1);
  return out;
}

/** Scales the buffer so its highest peak sits at `targetDb` dBFS (in place). Returns the gain. */
export function normalizeBuffer(buffer: AudioBuffer, targetDb = -1): number {
  let peak = 0;
  for (let c = 0; c < buffer.numberOfChannels; c++) {
    const d = buffer.getChannelData(c);
    for (let i = 0; i < d.length; i++) {
      const a = Math.abs(d[i]);
      if (a > peak) peak = a;
    }
  }
  if (peak < 1e-6) return 1;
  const gain = Math.pow(10, targetDb / 20) / peak;
  for (let c = 0; c < buffer.numberOfChannels; c++) {
    const d = buffer.getChannelData(c);
    for (let i = 0; i < d.length; i++) d[i] *= gain;
  }
  return gain;
}

export type WavBits = 16 | 24 | 32;

/** PCM WAV encoder: 16 bit (with TPDF dither), 24 bit, or 32 bit float. */
export function encodeWav(buffer: AudioBuffer, bits: WavBits = 16): Blob {
  const ch = buffer.numberOfChannels;
  const len = buffer.length;
  const bytesPer = bits / 8;
  const dataBytes = len * ch * bytesPer;
  const view = new DataView(new ArrayBuffer(44 + dataBytes));
  const w = (o: number, s: string) => { for (let i = 0; i < s.length; i++) view.setUint8(o + i, s.charCodeAt(i)); };
  w(0, 'RIFF');
  view.setUint32(4, 36 + dataBytes, true);
  w(8, 'WAVE');
  w(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, bits === 32 ? 3 : 1, true); // 3 = IEEE float
  view.setUint16(22, ch, true);
  view.setUint32(24, buffer.sampleRate, true);
  view.setUint32(28, buffer.sampleRate * ch * bytesPer, true);
  view.setUint16(32, ch * bytesPer, true);
  view.setUint16(34, bits, true);
  w(36, 'data');
  view.setUint32(40, dataBytes, true);
  const chans = Array.from({ length: ch }, (_, c) => buffer.getChannelData(c));
  let o = 44;
  let seed = 22222;
  const rnd = () => {
    seed = (seed * 1664525 + 1013904223) >>> 0;
    return seed / 4294967296;
  };
  for (let i = 0; i < len; i++) {
    for (let c = 0; c < ch; c++) {
      let s = chans[c][i];
      if (!Number.isFinite(s)) s = 0;
      if (bits === 32) {
        view.setFloat32(o, s, true);
      } else if (bits === 24) {
        const v = Math.round(Math.max(-1, Math.min(1, s)) * 8388607);
        view.setUint8(o, v & 0xff);
        view.setUint8(o + 1, (v >> 8) & 0xff);
        view.setUint8(o + 2, (v >> 16) & 0xff);
      } else {
        // triangular dither (±1 LSB) hides the quantisation distortion of quiet passages
        const d = (rnd() - rnd()) / 32768;
        const v = Math.max(-1, Math.min(1, s + d));
        view.setInt16(o, Math.round(v < 0 ? v * 0x8000 : v * 0x7fff), true);
      }
      o += bytesPer;
    }
  }
  return new Blob([view.buffer], { type: 'audio/wav' });
}

export function downloadBlob(blob: Blob, filename: string) {
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 5000);
}
