// Dedicated editor views for every built-in effect: knobs plus a live
// visualisation that reflects the actual DSP state.
import { useRef } from 'react';
import type { EffectInstance, ParamValue } from '../../state/types';
import { EFFECTS, num, str } from '../../audio/effects/descriptors';
import type { Effect } from '../../audio/effects/base';
import { CompressorEffect, DelayEffect, DistortionEffect, EqEffect, FilterEffect, PitchEffect } from '../../audio/effects';
import { makeCurve } from '../../audio/effects/distortion';
import { engine } from '../../audio/engine';
import { beginGesture, endGesture } from '../../state/store';
import { clamp, formatHz, rgba } from '../../state/utils';
import { theme } from '../../styles/themes';
import { NOTE_NAMES, noteName } from '../../midi/music';
import { useCanvas } from '../visualizers/Visualizers';
import { ParamControl } from './ParamKnob';

export interface ViewProps {
  trackId: string;
  fx: EffectInstance;
  set: (key: string, v: ParamValue) => void;
  effect: () => Effect | undefined;
}

function Knobs({ fx, set, keys, size = 42 }: { fx: EffectInstance; set: ViewProps['set']; keys?: string[]; size?: number }) {
  const d = EFFECTS[fx.type];
  const defs = keys ? keys.map((k) => d.params.find((p) => p.key === k)!).filter(Boolean) : d.params;
  return (
    <div className="fx-knobs">
      {defs.map((p) => <ParamControl key={p.key} def={p} value={fx.params[p.key]} onChange={(v) => set(p.key, v)} size={size} color={d.color} />)}
    </div>
  );
}

const FMIN = 20;
const FMAX = 20000;
const xOfF = (f: number, w: number) => (Math.log(f / FMIN) / Math.log(FMAX / FMIN)) * w;
const fOfX = (x: number, w: number) => FMIN * Math.pow(FMAX / FMIN, x / w);

const freqs = new Float32Array(256).map((_, i) => FMIN * Math.pow(FMAX / FMIN, i / 255));
const specBuf = new Float32Array(512);

function drawFreqGrid(ctx: CanvasRenderingContext2D, w: number, h: number, dbRange: number) {
  const t = theme();
  ctx.fillStyle = '#08090b';
  ctx.fillRect(0, 0, w, h);
  ctx.font = '9px Inter, sans-serif';
  for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
    const x = xOfF(f, w);
    ctx.fillStyle = rgba(t.text, 0.07);
    ctx.fillRect(x, 0, 1, h);
    ctx.fillStyle = rgba(t.dim, 0.8);
    ctx.fillText(f >= 1000 ? `${f / 1000}k` : String(f), x + 2, h - 3);
  }
  for (let db = -dbRange; db <= dbRange; db += dbRange / 3) {
    const y = h / 2 - (db / dbRange) * (h / 2 - 8);
    ctx.fillStyle = rgba(t.text, db === 0 ? 0.15 : 0.05);
    ctx.fillRect(0, y, w, 1);
  }
}

function drawTrackSpectrum(ctx: CanvasRenderingContext2D, trackId: string, w: number, h: number, color: string) {
  const strip = engine.initialized ? engine.graph.strips.get(trackId) : undefined;
  if (!strip) return;
  const an = strip.analyserL;
  const bins = an.frequencyBinCount;
  an.getFloatFrequencyData(specBuf.subarray(0, bins));
  const sr = engine.ctx.sampleRate;
  ctx.beginPath();
  ctx.moveTo(0, h);
  for (let x = 0; x < w; x += 2) {
    const f = fOfX(x, w);
    const b = Math.min(bins - 1, Math.round((f / (sr / 2)) * bins));
    const v = clamp((specBuf[b] + 3 * Math.log2(f / 1000) + 90) / 90, 0, 1);
    ctx.lineTo(x, h - v * h);
  }
  ctx.lineTo(w, h);
  ctx.fillStyle = rgba(color, 0.12);
  ctx.fill();
}

// ---------------- EQ ----------------
export function EqView(p: ViewProps) {
  const color = EFFECTS.eq.color;
  const dragBand = useRef<number | null>(null);
  const DB = 18;
  const bandKeys: [string, string][] = [['low', 'lowFreq'], ['lowMid', 'lowMidFreq'], ['mid', 'midFreq'], ['highMid', 'highMidFreq'], ['high', 'highFreq']];
  const ref = useCanvas((ctx, w, h) => {
    drawFreqGrid(ctx, w, h, DB);
    drawTrackSpectrum(ctx, p.trackId, w, h, color);
    const eff = p.effect();
    if (!(eff instanceof EqEffect)) return;
    const resp = eff.response(freqs);
    ctx.beginPath();
    for (let i = 0; i < freqs.length; i++) {
      const x = xOfF(freqs[i], w);
      const y = h / 2 - (clamp(resp[i], -DB * 1.5, DB * 1.5) / DB) * (h / 2 - 8);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.shadowBlur = 0;
    ctx.lineTo(w, h / 2);
    ctx.lineTo(0, h / 2);
    ctx.fillStyle = rgba(color, 0.1);
    ctx.fill();
    bandKeys.forEach(([g, f], i) => {
      const x = xOfF(num(p.fx.params, f, 1000), w);
      const y = h / 2 - (num(p.fx.params, g) / DB) * (h / 2 - 8);
      ctx.beginPath();
      ctx.arc(x, y, dragBand.current === i ? 7 : 5.5, 0, Math.PI * 2);
      ctx.fillStyle = ['#ff5c5c', '#ffbe3d', '#9be15d', '#3fc7e8', '#d46bff'][i];
      ctx.fill();
      ctx.strokeStyle = '#000';
      ctx.stroke();
    });
  });
  const pos = (e: React.PointerEvent) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top, w: r.width, h: r.height };
  };
  return (
    <div className="fx-view">
      <canvas
        ref={ref}
        className="fx-canvas tall"
        title="Drag the band handles: horizontal = frequency, vertical = gain"
        onPointerDown={(e) => {
          const { x, y, w, h } = pos(e);
          let best = -1;
          let bd = 22;
          bandKeys.forEach(([g, f], i) => {
            const bx = xOfF(num(p.fx.params, f, 1000), w);
            const by = h / 2 - (num(p.fx.params, g) / DB) * (h / 2 - 8);
            const d = Math.hypot(bx - x, by - y);
            if (d < bd) { bd = d; best = i; }
          });
          if (best < 0) return;
          (e.currentTarget as Element).setPointerCapture(e.pointerId);
          dragBand.current = best;
          beginGesture();
        }}
        onPointerMove={(e) => {
          const b = dragBand.current;
          if (b === null) return;
          const { x, y, w, h } = pos(e);
          const [g, f] = bandKeys[b];
          const def = EFFECTS.eq.params.find((q) => q.key === f)!;
          if (def.kind === 'number') p.set(f, Math.round(clamp(fOfX(x, w), def.min, def.max)));
          p.set(g, Math.round(clamp(((h / 2 - y) / (h / 2 - 8)) * DB, -DB, DB) * 10) / 10);
        }}
        onPointerUp={() => { dragBand.current = null; endGesture(); }}
      />
      <Knobs fx={p.fx} set={p.set} size={36} />
    </div>
  );
}

// ---------------- Compressor ----------------
export function CompressorView(p: ViewProps) {
  const color = EFFECTS.compressor.color;
  const grState = useRef(0);
  const ref = useCanvas((ctx, w, h, dt) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const gw = h; // square transfer graph
    const thr = num(p.fx.params, 'threshold', -18);
    const ratio = num(p.fx.params, 'ratio', 4);
    const knee = num(p.fx.params, 'knee', 6);
    const mk = num(p.fx.params, 'makeup', 0);
    const toX = (db: number) => ((db + 60) / 60) * gw;
    const toY = (db: number) => h - ((db + 60) / 60) * h;
    ctx.strokeStyle = rgba(t.text, 0.08);
    for (let db = -60; db <= 0; db += 12) {
      ctx.beginPath(); ctx.moveTo(toX(db), 0); ctx.lineTo(toX(db), h); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(0, toY(db)); ctx.lineTo(gw, toY(db)); ctx.stroke();
    }
    ctx.strokeStyle = rgba(t.text, 0.2);
    ctx.setLineDash([3, 3]);
    ctx.beginPath(); ctx.moveTo(0, h); ctx.lineTo(gw, 0); ctx.stroke();
    ctx.setLineDash([]);
    ctx.beginPath();
    for (let x = 0; x <= gw; x++) {
      const inDb = (x / gw) * 60 - 60;
      let out: number;
      if (inDb < thr - knee / 2) out = inDb;
      else if (inDb > thr + knee / 2) out = thr + (inDb - thr) / ratio;
      else {
        const k = inDb - thr + knee / 2;
        out = inDb + ((1 / ratio - 1) * k * k) / (2 * Math.max(0.01, knee));
      }
      const y = toY(Math.min(0, out + mk));
      if (x === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.fillStyle = rgba(color, 0.7);
    ctx.fillRect(toX(thr), 0, 1, h);
    // gain reduction meter
    const eff = p.effect();
    const gr = eff instanceof CompressorEffect ? eff.reduction : 0;
    grState.current = Math.min(gr, grState.current + dt * 20);
    const mx = gw + 20;
    const mw = 22;
    ctx.fillStyle = '#15181c';
    ctx.fillRect(mx, 4, mw, h - 8);
    const grH = clamp(-grState.current / 24, 0, 1) * (h - 8);
    ctx.fillStyle = color;
    ctx.fillRect(mx, 4, mw, grH);
    ctx.fillStyle = t.text;
    ctx.font = '600 10px Inter, sans-serif';
    ctx.fillText('GR', mx + 3, h - 10);
    ctx.fillText(`${grState.current.toFixed(1)} dB`, mx + mw + 8, 16);
    ctx.fillStyle = t.dim;
    ctx.fillText(`Threshold ${thr.toFixed(1)} dB`, mx + mw + 8, 34);
    ctx.fillText(`Ratio ${ratio.toFixed(1)}:1`, mx + mw + 8, 50);
    ctx.fillText(`Makeup +${mk.toFixed(1)} dB`, mx + mw + 8, 66);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Reverb ----------------
export function ReverbView(p: ViewProps) {
  const color = EFFECTS.reverb.color;
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const decay = num(p.fx.params, 'decay', 2);
    const size = num(p.fx.params, 'size', 0.5);
    const pre = num(p.fx.params, 'predelay', 10) / 1000;
    const wet = num(p.fx.params, 'wet', 0.3);
    const span = Math.max(1, decay * 1.4 + pre);
    const xOf = (s: number) => (s / span) * w;
    ctx.fillStyle = rgba(t.text, 0.5);
    ctx.fillRect(2, h * 0.1, 3, h * 0.85);
    // early reflections
    const er = 0.005 + size * 0.06;
    for (let i = 0; i < 6 + size * 10; i++) {
      const tt = pre + er * ((i * 0.618) % 1);
      ctx.fillStyle = rgba(color, 0.9);
      ctx.fillRect(xOf(tt), h - h * 0.75 * wet * (1 - i / 20) - 6, 2, h * 0.75 * wet * (1 - i / 20));
    }
    // tail
    ctx.beginPath();
    ctx.moveTo(xOf(pre), h - 4);
    for (let x = xOf(pre); x < w; x += 2) {
      const tt = x / w * span - pre;
      const env = Math.exp((-6.9 * tt) / decay) * wet;
      ctx.lineTo(x, h - 4 - env * (h * 0.8));
    }
    ctx.lineTo(w, h - 4);
    ctx.closePath();
    ctx.fillStyle = rgba(color, 0.3);
    ctx.fill();
    ctx.strokeStyle = color;
    ctx.stroke();
    ctx.fillStyle = t.dim;
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(`RT60 ${decay.toFixed(2)} s · size ${Math.round(size * 100)}% · pre ${Math.round(pre * 1000)} ms`, 10, 14);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Delay ----------------
export function DelayView(p: ViewProps) {
  const color = EFFECTS.delay.color;
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const eff = p.effect();
    const time = eff instanceof DelayEffect ? eff.time : num(p.fx.params, 'time', 375) / 1000;
    const fb = num(p.fx.params, 'feedback', 0.4);
    const pp = num(p.fx.params, 'pingpong', 0.5);
    const mix = num(p.fx.params, 'mix', 0.3);
    const span = Math.max(time * 8, 1);
    const mid = h / 2;
    ctx.fillStyle = rgba(t.text, 0.08);
    ctx.fillRect(0, mid, w, 1);
    ctx.fillStyle = t.text;
    ctx.fillRect(6, 8, 4, h - 16);
    let g = mix;
    for (let i = 1; i < 30; i++) {
      const x = 6 + (i * time / span) * (w - 12);
      if (x > w) break;
      const side = i % 2 === 1 ? -1 : 1;
      const hh = g * (h / 2 - 10);
      ctx.fillStyle = rgba(color, 0.35 + g * 0.65);
      ctx.fillRect(x, mid - hh + side * pp * hh * 0.5, 4, hh * 2 * (1 - pp * 0.5));
      g *= fb;
      if (g < 0.01) break;
    }
    ctx.fillStyle = t.dim;
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(`${Math.round(time * 1000)} ms · feedback ${Math.round(fb * 100)}%`, 14, 14);
    ctx.fillText('L', w - 12, 14);
    ctx.fillText('R', w - 12, h - 6);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Filter ----------------
export function FilterView(p: ViewProps) {
  const color = EFFECTS.filter.color;
  const DB = 24;
  const ref = useCanvas((ctx, w, h) => {
    drawFreqGrid(ctx, w, h, DB);
    drawTrackSpectrum(ctx, p.trackId, w, h, color);
    const eff = p.effect();
    if (!(eff instanceof FilterEffect)) return;
    const resp = eff.response(freqs);
    ctx.beginPath();
    for (let i = 0; i < freqs.length; i++) {
      const x = xOfF(freqs[i], w);
      const y = h / 2 - (clamp(resp[i], -DB * 2, DB) / DB) * (h / 2 - 8);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.shadowBlur = 0;
    ctx.fillStyle = theme().dim;
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(`${str(p.fx.params, 'mode')} · ${formatHz(eff.filter.frequency.value)}`, 8, 14);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Distortion ----------------
export function DistortionView(p: ViewProps) {
  const color = EFFECTS.distortion.color;
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const eff = p.effect();
    const curve = eff instanceof DistortionEffect ? eff.curve : makeCurve(str(p.fx.params, 'shape', 'soft'), num(p.fx.params, 'drive', 0.4));
    const gw = h;
    ctx.strokeStyle = rgba(t.text, 0.1);
    ctx.beginPath(); ctx.moveTo(gw / 2, 0); ctx.lineTo(gw / 2, h); ctx.moveTo(0, h / 2); ctx.lineTo(gw, h / 2); ctx.stroke();
    ctx.beginPath();
    for (let i = 0; i < curve.length; i += 4) {
      const x = (i / (curve.length - 1)) * gw;
      const y = h / 2 - curve[i] * (h / 2 - 6);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = color;
    ctx.lineWidth = 2;
    ctx.shadowColor = color;
    ctx.shadowBlur = 8;
    ctx.stroke();
    ctx.shadowBlur = 0;
    // sine through the shaper preview
    const ox = gw + 16;
    const ow = w - ox - 6;
    ctx.strokeStyle = rgba(t.text, 0.25);
    ctx.beginPath();
    for (let x = 0; x < ow; x++) {
      const s = Math.sin((x / ow) * Math.PI * 4);
      const y = h / 2 - s * (h / 2 - 10);
      if (x === 0) ctx.moveTo(ox + x, y); else ctx.lineTo(ox + x, y);
    }
    ctx.stroke();
    ctx.strokeStyle = color;
    ctx.beginPath();
    for (let x = 0; x < ow; x++) {
      const s = Math.sin((x / ow) * Math.PI * 4) * (1 + num(p.fx.params, 'drive') * 2) * 0.5;
      const idx = clamp(Math.round(((clamp(s, -1, 1) + 1) / 2) * (curve.length - 1)), 0, curve.length - 1);
      const y = h / 2 - curve[idx] * (h / 2 - 10);
      if (x === 0) ctx.moveTo(ox + x, y); else ctx.lineTo(ox + x, y);
    }
    ctx.stroke();
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Chorus ----------------
export function ChorusView(p: ViewProps) {
  const color = EFFECTS.chorus.color;
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const rate = num(p.fx.params, 'rate', 0.8);
    const depth = num(p.fx.params, 'depth', 0.5);
    const now = performance.now() / 1000;
    for (const [phase, col, label] of [[0, color, 'L'], [Math.PI, '#56ccf2', 'R']] as const) {
      ctx.beginPath();
      for (let x = 0; x < w; x++) {
        const tt = now - (1 - x / w) * 3;
        const y = h / 2 - Math.sin(tt * rate * Math.PI * 2 + phase) * depth * (h / 2 - 10);
        if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.strokeStyle = col;
      ctx.lineWidth = 2;
      ctx.stroke();
      ctx.fillStyle = col;
      ctx.fillText(label, w - 12, label === 'L' ? 14 : h - 6);
    }
    ctx.fillStyle = t.dim;
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(`LFO ${rate.toFixed(2)} Hz · depth ${Math.round(depth * 100)}%`, 8, 14);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas" />
      <Knobs fx={p.fx} set={p.set} />
    </div>
  );
}

// ---------------- Pitch correction ----------------
export function PitchView(p: ViewProps) {
  const color = EFFECTS.pitch.color;
  const hist = useRef<{ d: number; t: number }[]>([]);
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const eff = p.effect();
    const info = eff instanceof PitchEffect ? eff.info : null;
    const fresh = info && performance.now() - info.time < 300;
    const H = hist.current;
    H.push({ d: fresh && info!.detected > 0 ? info!.detected : -1, t: fresh && info!.target > 0 ? info!.target : -1 });
    if (H.length > w / 2) H.shift();
    // centre area: tuner
    const cw = 170;
    const note = fresh && info!.detected > 0 ? info!.detected : -1;
    ctx.textAlign = 'center';
    ctx.fillStyle = note > 0 ? t.text : t.dim;
    ctx.font = '700 34px Inter, sans-serif';
    ctx.fillText(note > 0 ? noteName(Math.round(note)) : '—', cw / 2, 44);
    ctx.font = '10px Inter, sans-serif';
    ctx.fillStyle = t.dim;
    ctx.fillText(note > 0 ? `${(440 * Math.pow(2, (note - 69) / 12)).toFixed(1)} Hz` : eff instanceof PitchEffect && !eff.available ? 'AudioWorklet unavailable' : 'no pitch detected', cw / 2, 60);
    if (note <= 0) {
      ctx.fillStyle = rgba(t.dim, 0.7);
      ctx.fillText('sing with Monitor on', cw / 2, 132);
      ctx.fillText('or play a vocal clip', cw / 2, 145);
    }
    // cents meter
    const cents = note > 0 ? (note - Math.round(note)) * 100 : 0;
    const mw = cw - 24;
    ctx.fillStyle = '#15181c';
    ctx.fillRect(12, 70, mw, 10);
    ctx.fillStyle = rgba(t.led, 0.7);
    ctx.fillRect(12 + mw / 2 - 1, 68, 2, 14);
    if (note > 0) {
      ctx.fillStyle = Math.abs(cents) < 10 ? t.led : color;
      ctx.fillRect(12 + mw / 2 + (cents / 50) * (mw / 2) - 3, 70, 6, 10);
    }
    ctx.fillStyle = t.dim;
    ctx.fillText(`${cents > 0 ? '+' : ''}${Math.round(cents)} ct`, cw / 2, 96);
    if (info && fresh && info.target > 0) {
      ctx.fillStyle = color;
      ctx.font = '600 11px Inter, sans-serif';
      ctx.fillText(`→ ${noteName(info.target)}  (${info.shift > 0 ? '+' : ''}${info.shift.toFixed(2)} st)`, cw / 2, 114);
    }
    ctx.textAlign = 'left';
    // history graph
    const gx = cw;
    const gw = w - cw;
    ctx.fillStyle = '#0c0e11';
    ctx.fillRect(gx, 0, gw, h);
    const valid = H.filter((x) => x.d > 0).map((x) => x.d);
    const center = valid.length ? valid.reduce((a, b) => a + b, 0) / valid.length : 60;
    const range = 7;
    const yOf = (m: number) => h / 2 - ((m - center) / range) * (h / 2);
    for (let m = Math.ceil(center - range); m <= center + range; m++) {
      const y = yOf(m);
      ctx.fillStyle = rgba(t.text, m % 12 === 0 ? 0.14 : 0.05);
      ctx.fillRect(gx, y, gw, 1);
      ctx.fillStyle = rgba(t.dim, 0.7);
      ctx.fillText(NOTE_NAMES[((m % 12) + 12) % 12], gx + 3, y - 2);
    }
    const plot = (key: 'd' | 't', col: string, lw: number) => {
      ctx.strokeStyle = col;
      ctx.lineWidth = lw;
      ctx.beginPath();
      let pen = false;
      H.forEach((pt, i) => {
        const v = pt[key];
        const x = gx + 24 + i * 2;
        if (v <= 0) { pen = false; return; }
        const y = yOf(v);
        if (!pen) { ctx.moveTo(x, y); pen = true; } else ctx.lineTo(x, y);
      });
      ctx.stroke();
    };
    plot('t', rgba(color, 0.9), 3);
    plot('d', t.text, 1.2);
    ctx.fillStyle = t.dim;
    ctx.fillText('— input', gx + gw - 110, 14);
    ctx.fillStyle = color;
    ctx.fillText('— corrected', gx + gw - 60, 14);
  });
  return (
    <div className="fx-view">
      <canvas ref={ref} className="fx-canvas tall" />
      <Knobs fx={p.fx} set={p.set} size={44} />
    </div>
  );
}

export const VIEWS: Record<EffectInstance['type'], (p: ViewProps) => React.ReactElement> = {
  eq: EqView,
  compressor: CompressorView,
  reverb: ReverbView,
  delay: DelayView,
  filter: FilterView,
  distortion: DistortionView,
  chorus: ChorusView,
  pitch: PitchView,
};
