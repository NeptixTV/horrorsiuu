// Canvas drawing for the playlist grid and clips.
import type { Clip, Pattern, Project } from '../../state/types';
import { PPQ, TICKS_PER_STEP } from '../../state/types';
import { patternLength } from '../../audio/sequencer';
import { assets } from '../../audio/assets';
import { rgba, shade, ticksPerBar } from '../../state/utils';
import type { ThemeVars } from '../../styles/themes';

export interface View {
  w: number;
  h: number;
  scrollX: number;
  scrollY: number;
  pxPerTick: number;
  trackH: number;
}

export const HEADER_W = 150;
export const RULER_H = 26;
export const CLIP_HEADER = 13;

export function drawGrid(ctx: CanvasRenderingContext2D, v: View, project: Project, t: ThemeVars) {
  ctx.fillStyle = t.grid;
  ctx.fillRect(0, 0, v.w, v.h);
  const first = Math.floor(v.scrollY / v.trackH);
  const last = Math.min(project.tracks.length, Math.ceil((v.scrollY + v.h) / v.trackH));
  for (let i = first; i < last; i++) {
    const y = i * v.trackH - v.scrollY;
    ctx.fillStyle = i % 2 ? t.gridAlt : t.grid;
    ctx.fillRect(0, y, v.w, v.trackH);
    if (project.tracks[i]?.mute) {
      ctx.fillStyle = 'rgba(0,0,0,0.25)';
      ctx.fillRect(0, y, v.w, v.trackH);
    }
    ctx.fillStyle = 'rgba(0,0,0,0.25)';
    ctx.fillRect(0, y + v.trackH - 1, v.w, 1);
  }
  // area below the last track
  const endY = project.tracks.length * v.trackH - v.scrollY;
  if (endY < v.h) {
    ctx.fillStyle = shade(t.grid, -0.25);
    ctx.fillRect(0, endY, v.w, v.h - endY);
  }
  const bar = ticksPerBar(project.timeSig);
  const beat = PPQ * (4 / project.timeSig[1]);
  const startTick = v.scrollX / v.pxPerTick;
  const endTick = (v.scrollX + v.w) / v.pxPerTick;
  const beatPx = beat * v.pxPerTick;
  const stepPx = TICKS_PER_STEP * v.pxPerTick;
  // alternate bar shading
  for (let b = Math.floor(startTick / bar); b * bar < endTick; b++) {
    if (b % 2 === 1) {
      ctx.fillStyle = 'rgba(0,0,0,0.07)';
      ctx.fillRect(b * bar * v.pxPerTick - v.scrollX, 0, bar * v.pxPerTick, Math.min(v.h, endY));
    }
  }
  const drawLines = (every: number, color: string) => {
    ctx.fillStyle = color;
    for (let tk = Math.floor(startTick / every) * every; tk <= endTick; tk += every) {
      const x = Math.round(tk * v.pxPerTick - v.scrollX);
      ctx.fillRect(x, 0, 1, Math.min(v.h, endY));
    }
  };
  if (stepPx >= 6) drawLines(TICKS_PER_STEP, rgba(t.gridLine, 0.6));
  if (beatPx >= 8) drawLines(beat, t.gridLine);
  drawLines(bar, t.gridBar);
}

export function drawLoop(ctx: CanvasRenderingContext2D, v: View, project: Project, t: ThemeVars) {
  if (!project.loop.enabled) return;
  const x0 = project.loop.start * v.pxPerTick - v.scrollX;
  const x1 = project.loop.end * v.pxPerTick - v.scrollX;
  ctx.fillStyle = rgba(t.accent, 0.06);
  ctx.fillRect(x0, 0, x1 - x0, v.h);
  ctx.fillStyle = rgba(t.accent, 0.5);
  ctx.fillRect(x0, 0, 1, v.h);
  ctx.fillRect(x1, 0, 1, v.h);
}

function patternPreview(ctx: CanvasRenderingContext2D, pattern: Pattern, project: Project, x: number, y: number, _w: number, h: number, clip: Clip, pxPerTick: number, color: string) {
  const len = patternLength(pattern, project.timeSig);
  // collect notes and step hits
  let minP = 127;
  let maxP = 0;
  const items: { tick: number; len: number; pitch: number | null; row: number }[] = [];
  const channelRows = project.channels.filter((c) => (pattern.steps[c.id] ?? []).some((v) => v > 0));
  channelRows.forEach((c, row) => {
    const steps = pattern.steps[c.id];
    for (let i = 0; i < Math.min(steps.length, pattern.lengthSteps); i++) if (steps[i] > 0) items.push({ tick: i * TICKS_PER_STEP, len: TICKS_PER_STEP * 0.6, pitch: null, row });
  });
  for (const list of Object.values(pattern.notes)) {
    for (const n of list) {
      items.push({ tick: n.start, len: n.length, pitch: n.pitch, row: 0 });
      minP = Math.min(minP, n.pitch);
      maxP = Math.max(maxP, n.pitch);
    }
  }
  if (!items.length) return;
  const range = Math.max(6, maxP - minP + 1);
  const hasNotes = maxP >= minP;
  const stepRows = Math.max(1, channelRows.length);
  ctx.fillStyle = shade(color, 0.55);
  const origin = clip.start - clip.offset;
  for (let rep = Math.floor((clip.start - origin) / len); origin + rep * len < clip.start + clip.length; rep++) {
    for (const it of items) {
      const abs = origin + rep * len + it.tick;
      if (abs < clip.start || abs >= clip.start + clip.length) continue;
      const ix = x + (abs - clip.start) * pxPerTick;
      const iw = Math.max(1.5, Math.min(it.len, clip.start + clip.length - abs) * pxPerTick - 1);
      let iy: number;
      let ih: number;
      if (it.pitch !== null && hasNotes) {
        ih = Math.max(1.5, (h - 4) / range);
        iy = y + 2 + (maxP - it.pitch) * ((h - 4 - ih) / Math.max(1, range - 1));
      } else {
        ih = Math.max(1.5, (h - 4) / stepRows - 1);
        iy = y + 2 + it.row * ((h - 4) / stepRows);
      }
      ctx.fillRect(ix, iy, iw, ih);
    }
  }
}

function audioPreview(ctx: CanvasRenderingContext2D, clip: Clip, x: number, y: number, w: number, h: number, bpm: number, pxPerTick: number, color: string, viewW: number) {
  const peaks = clip.assetId ? assets.getPeaks(clip.assetId) : undefined;
  const buf = clip.assetId ? assets.get(clip.assetId) : undefined;
  if (!peaks || !buf) {
    ctx.fillStyle = rgba('#ffffff', 0.4);
    ctx.font = '10px sans-serif';
    ctx.fillText('missing audio', x + 4, y + h / 2);
    return;
  }
  const secPerPx = 60 / bpm / PPQ / pxPerTick;
  const mid = y + h / 2;
  const amp = (h / 2) * 0.92 * (clip.gain ?? 1);
  ctx.fillStyle = shade(color, 0.6);
  const x0 = Math.max(x, 0);
  const x1 = Math.min(x + w, viewW);
  const spp = peaks.samplesPerPeak;
  for (let px = x0; px < x1; px++) {
    const t0 = clip.offset + (px - x) * secPerPx;
    const t1 = t0 + secPerPx;
    if (t0 >= buf.duration) break;
    const p0 = Math.floor((t0 * buf.sampleRate) / spp);
    const p1 = Math.max(p0 + 1, Math.floor((t1 * buf.sampleRate) / spp));
    let mn = 0;
    let mx = 0;
    for (let p = p0; p < p1 && p * 2 + 1 < peaks.data.length; p++) {
      mn = Math.min(mn, peaks.data[p * 2]);
      mx = Math.max(mx, peaks.data[p * 2 + 1]);
    }
    ctx.fillRect(px, mid - mx * amp, 1, Math.max(1, (mx - mn) * amp));
  }
}

export function clipColor(clip: Clip, project: Project): string {
  if (clip.kind === 'audio') return '#3fa7a0';
  return project.patterns.find((p) => p.id === clip.patternId)?.color ?? '#888';
}

export function clipLabel(clip: Clip, project: Project): string {
  if (clip.kind === 'audio') return project.assets[clip.assetId ?? '']?.name ?? assets.name(clip.assetId ?? '');
  return project.patterns.find((p) => p.id === clip.patternId)?.name ?? 'Pattern';
}

export function drawClips(ctx: CanvasRenderingContext2D, v: View, project: Project, selected: Set<string>, t: ThemeVars) {
  const trackIndex = new Map(project.tracks.map((tr, i) => [tr.id, i]));
  const patterns = new Map(project.patterns.map((p) => [p.id, p]));
  ctx.font = '600 10px Inter, sans-serif';
  ctx.textBaseline = 'middle';
  for (const clip of project.clips) {
    const ti = trackIndex.get(clip.trackId);
    if (ti === undefined) continue;
    const x = clip.start * v.pxPerTick - v.scrollX;
    const w = clip.length * v.pxPerTick;
    const y = ti * v.trackH - v.scrollY + 1;
    const h = v.trackH - 3;
    if (x > v.w || x + w < 0 || y > v.h || y + h < 0) continue;
    const color = clipColor(clip, project);
    const sel = selected.has(clip.id);
    const muted = project.tracks[ti]?.mute;
    ctx.globalAlpha = muted ? 0.45 : 1;
    // body
    ctx.fillStyle = shade(color, -0.45);
    ctx.fillRect(x, y, w, h);
    ctx.fillStyle = rgba(color, 0.35);
    ctx.fillRect(x, y, w, h);
    // header
    const headH = Math.min(CLIP_HEADER, h * 0.4);
    ctx.fillStyle = color;
    ctx.fillRect(x, y, w, headH);
    ctx.save();
    ctx.beginPath();
    ctx.rect(Math.max(0, x), y, Math.min(w, v.w - Math.max(0, x)), h);
    ctx.clip();
    ctx.fillStyle = '#111';
    if (w > 18) ctx.fillText(clipLabel(clip, project), Math.max(x, 0) + 4, y + headH / 2 + 0.5);
    const body = { x, y: y + headH, w, h: h - headH };
    if (clip.kind === 'pattern') {
      const pat = patterns.get(clip.patternId ?? '');
      if (pat) patternPreview(ctx, pat, project, body.x, body.y, body.w, body.h, clip, v.pxPerTick, color);
    } else {
      audioPreview(ctx, clip, body.x, body.y, body.w, body.h, project.bpm, v.pxPerTick, color, v.w);
    }
    ctx.restore();
    // border
    ctx.strokeStyle = sel ? '#fff' : shade(color, -0.6);
    ctx.lineWidth = sel ? 1.5 : 1;
    ctx.strokeRect(x + 0.5, y + 0.5, w - 1, h - 1);
    if (sel) {
      ctx.fillStyle = 'rgba(255,255,255,0.10)';
      ctx.fillRect(x, y, w, h);
    }
    ctx.globalAlpha = 1;
  }
  void t;
}

export function drawRuler(ctx: CanvasRenderingContext2D, w: number, h: number, scrollX: number, pxPerTick: number, project: Project, t: ThemeVars) {
  ctx.fillStyle = t.panel2;
  ctx.fillRect(0, 0, w, h);
  const bar = ticksPerBar(project.timeSig);
  const beat = PPQ * (4 / project.timeSig[1]);
  const barPx = bar * pxPerTick;
  const every = barPx < 26 ? (barPx < 13 ? 4 : 2) : 1;
  const startBar = Math.floor(scrollX / pxPerTick / bar);
  const endBar = Math.ceil((scrollX + w) / pxPerTick / bar);
  // loop region
  if (project.loop.end > project.loop.start) {
    const x0 = project.loop.start * pxPerTick - scrollX;
    const x1 = project.loop.end * pxPerTick - scrollX;
    ctx.fillStyle = project.loop.enabled ? rgba(t.accent, 0.85) : rgba(t.dim, 0.35);
    ctx.fillRect(x0, 0, x1 - x0, 6);
  }
  ctx.font = '10px Inter, sans-serif';
  ctx.textBaseline = 'top';
  for (let b = startBar; b <= endBar; b++) {
    const x = Math.round(b * barPx - scrollX);
    ctx.fillStyle = t.gridBar;
    ctx.fillRect(x, h - 10, 1, 10);
    if (b % every === 0) {
      ctx.fillStyle = t.text;
      ctx.fillText(String(b + 1), x + 3, 8);
    }
    if (barPx > 60) {
      for (let k = 1; k < project.timeSig[0]; k++) {
        ctx.fillStyle = t.gridLine;
        ctx.fillRect(Math.round(x + k * beat * pxPerTick), h - 5, 1, 5);
      }
    }
  }
  ctx.fillStyle = t.border;
  ctx.fillRect(0, h - 1, w, 1);
}
