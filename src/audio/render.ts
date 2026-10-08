// Offline mixdown: rebuilds the project graph inside an OfflineAudioContext,
// schedules the whole song (or pattern) at once and encodes the result as WAV.
import type { Project } from '../state/types';
import { PPQ } from '../state/types';
import { MixGraph } from './mixgraph';
import { loadWorklets, markWorkletsReady } from './worklets';
import { assets } from './assets';
import {
  collectAudioStarts, collectNoteEvents, patternLength, songLength, type PlayMode,
} from './sequencer';

export interface RenderOptions {
  mode: PlayMode;
  patternId?: string | null;
  sampleRate?: number;
  tail?: number; // seconds of release/reverb tail
}

export async function renderProject(project: Project, opts: RenderOptions): Promise<AudioBuffer> {
  const sr = opts.sampleRate ?? 44100;
  const spt = 60 / (project.bpm * PPQ);
  let fromTick = 0;
  let toTick: number;
  if (opts.mode === 'pattern') {
    const p = project.patterns.find((x) => x.id === opts.patternId) ?? project.patterns[0];
    toTick = p ? patternLength(p, project.timeSig) : PPQ * 4;
  } else if (project.loop.enabled && project.loop.end > project.loop.start) {
    fromTick = project.loop.start;
    toTick = project.loop.end;
  } else {
    toTick = songLength(project);
  }
  const duration = (toTick - fromTick) * spt + (opts.tail ?? 2.5);
  const ctx = new OfflineAudioContext(2, Math.ceil(duration * sr), sr);
  if (await loadWorklets(ctx)) markWorkletsReady(ctx);
  const graph = new MixGraph(ctx);
  graph.sync(project);

  const lead = 0.01;
  for (const e of collectNoteEvents(project, opts.mode, opts.patternId ?? null, fromTick, toTick)) {
    graph.noteOn(e.channelId, e.pitch, e.velocity, lead + (e.tick - fromTick) * spt, e.length * spt);
  }
  if (opts.mode === 'song') {
    for (const ev of collectAudioStarts(project, fromTick, toTick)) {
      const buffer = ev.clip.assetId ? assets.get(ev.clip.assetId) : undefined;
      if (!buffer) continue;
      const track = project.tracks.find((t) => t.id === ev.clip.trackId);
      const src = ctx.createBufferSource();
      src.buffer = buffer;
      const g = ctx.createGain();
      g.gain.value = ev.clip.gain ?? 1;
      src.connect(g).connect(graph.stripInput(track?.mixerTrackId ?? 'master'));
      const dur = Math.min(buffer.duration - ev.sourceOffset, Math.min(ev.remaining, toTick - ev.tick) * spt);
      if (dur > 0) src.start(lead + (ev.tick - fromTick) * spt, ev.sourceOffset, dur);
    }
  }
  return ctx.startRendering();
}

/** 16 bit PCM WAV encoder. */
export function encodeWav(buffer: AudioBuffer): Blob {
  const ch = buffer.numberOfChannels;
  const len = buffer.length;
  const bytes = 44 + len * ch * 2;
  const view = new DataView(new ArrayBuffer(bytes));
  const w = (o: number, s: string) => { for (let i = 0; i < s.length; i++) view.setUint8(o + i, s.charCodeAt(i)); };
  w(0, 'RIFF');
  view.setUint32(4, bytes - 8, true);
  w(8, 'WAVE');
  w(12, 'fmt ');
  view.setUint32(16, 16, true);
  view.setUint16(20, 1, true);
  view.setUint16(22, ch, true);
  view.setUint32(24, buffer.sampleRate, true);
  view.setUint32(28, buffer.sampleRate * ch * 2, true);
  view.setUint16(32, ch * 2, true);
  view.setUint16(34, 16, true);
  w(36, 'data');
  view.setUint32(40, len * ch * 2, true);
  const chans = Array.from({ length: ch }, (_, c) => buffer.getChannelData(c));
  let o = 44;
  for (let i = 0; i < len; i++) {
    for (let c = 0; c < ch; c++) {
      const s = Math.max(-1, Math.min(1, chans[c][i]));
      view.setInt16(o, s < 0 ? s * 0x8000 : s * 0x7fff, true);
      o += 2;
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
