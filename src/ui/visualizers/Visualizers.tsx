import { memo, useEffect, useRef } from 'react';
import { onFrame } from '../components/frameLoop';
import { theme } from '../../styles/themes';
import { engine } from '../../audio/engine';
import { rgba } from '../../state/utils';

/** Keeps a canvas sized to its CSS box (with devicePixelRatio). */
export function useCanvas(draw: (ctx: CanvasRenderingContext2D, w: number, h: number, dt: number) => void, deps: unknown[] = []) {
  const ref = useRef<HTMLCanvasElement>(null);
  const drawRef = useRef(draw);
  drawRef.current = draw;
  useEffect(() => {
    const cv = ref.current;
    if (!cv) return;
    const ctx = cv.getContext('2d')!;
    let w = 0;
    let h = 0;
    const resize = () => {
      const r = cv.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      w = Math.max(1, r.width);
      h = Math.max(1, r.height);
      cv.width = Math.round(w * dpr);
      cv.height = Math.round(h * dpr);
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    };
    resize();
    const ro = new ResizeObserver(resize);
    ro.observe(cv);
    const off = onFrame((dt) => {
      if (w > 1) drawRef.current(ctx, w, h, dt);
    });
    return () => {
      ro.disconnect();
      off();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, deps);
  return ref;
}

const timeBuf = new Float32Array(2048);
const timeBufR = new Float32Array(2048);

export const Oscilloscope = memo(function Oscilloscope({ className, lineWidth = 1.5 }: { className?: string; lineWidth?: number }) {
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.fillRect(0, 0, w, h);
    ctx.strokeStyle = rgba(t.text, 0.08);
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.stroke();
    if (!engine.initialized) return;
    engine.scope.getFloatTimeDomainData(timeBuf);
    // trigger on rising zero crossing for a stable picture
    let start = 0;
    for (let i = 1; i < 1024; i++) {
      if (timeBuf[i - 1] < 0 && timeBuf[i] >= 0) { start = i; break; }
    }
    const n = 1024;
    ctx.strokeStyle = t.accent;
    ctx.shadowColor = t.accent;
    ctx.shadowBlur = 6;
    ctx.lineWidth = lineWidth;
    ctx.beginPath();
    for (let i = 0; i < n; i++) {
      const x = (i / (n - 1)) * w;
      const y = h / 2 - timeBuf[start + i] * h * 0.45;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    }
    ctx.stroke();
    ctx.shadowBlur = 0;
  });
  return <canvas ref={ref} className={className} />;
});

const freqBuf = new Float32Array(2048);

export const Spectrum = memo(function Spectrum({ className, grid = true, analyser }: { className?: string; grid?: boolean; analyser?: () => AnalyserNode | null }) {
  const peaks = useRef<Float32Array>(new Float32Array(256));
  const ref = useCanvas((ctx, w, h, dt) => {
    const t = theme();
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = 'rgba(0,0,0,0.35)';
    ctx.fillRect(0, 0, w, h);
    const an = analyser ? analyser() : engine.initialized ? engine.spectrum : null;
    const sr = engine.initialized ? engine.ctx.sampleRate : 44100;
    const fmin = 20;
    const fmax = 20000;
    const xOf = (f: number) => (Math.log(f / fmin) / Math.log(fmax / fmin)) * w;
    if (grid) {
      ctx.fillStyle = rgba(t.dim, 0.9);
      ctx.font = '9px sans-serif';
      for (const f of [50, 100, 200, 500, 1000, 2000, 5000, 10000]) {
        const x = xOf(f);
        ctx.fillStyle = rgba(t.text, 0.06);
        ctx.fillRect(x, 0, 1, h);
        ctx.fillStyle = rgba(t.dim, 0.9);
        ctx.fillText(f >= 1000 ? `${f / 1000}k` : String(f), x + 2, h - 3);
      }
      for (const db of [-12, -24, -36, -48, -60]) {
        const y = (-db / 80) * h;
        ctx.fillStyle = rgba(t.text, 0.05);
        ctx.fillRect(0, y, w, 1);
      }
    }
    if (!an) return;
    const bins = an.frequencyBinCount;
    const data = freqBuf.length >= bins ? freqBuf : new Float32Array(bins);
    an.getFloatFrequencyData(data.subarray(0, bins));
    const N = Math.min(256, Math.max(32, Math.floor(w / 3)));
    if (peaks.current.length !== N) peaks.current = new Float32Array(N);
    const pk = peaks.current;
    const grad = ctx.createLinearGradient(0, 0, 0, h);
    grad.addColorStop(0, t.accent2);
    grad.addColorStop(1, rgba(t.accent, 0.15));
    ctx.beginPath();
    ctx.moveTo(0, h);
    for (let i = 0; i < N; i++) {
      const f0 = fmin * Math.pow(fmax / fmin, i / N);
      const f1 = fmin * Math.pow(fmax / fmin, (i + 1) / N);
      const b0 = Math.max(1, Math.floor((f0 / (sr / 2)) * bins));
      const b1 = Math.max(b0 + 1, Math.ceil((f1 / (sr / 2)) * bins));
      let m = -140;
      for (let b = b0; b < b1 && b < bins; b++) if (data[b] > m) m = data[b];
      // pink-ish tilt so the display looks balanced
      const tilt = 3 * Math.log2(f0 / 1000);
      const v = Math.max(0, Math.min(1, (m + tilt + 80) / 80));
      pk[i] = Math.max(v, pk[i] - dt * 0.35);
      const x = (i / N) * w;
      ctx.lineTo(x, h - v * h);
    }
    ctx.lineTo(w, h);
    ctx.closePath();
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.strokeStyle = t.accent;
    ctx.lineWidth = 1.2;
    ctx.stroke();
    ctx.fillStyle = rgba(t.text, 0.6);
    for (let i = 0; i < N; i++) ctx.fillRect((i / N) * w, h - pk[i] * h, Math.max(1, w / N - 1), 1);
  });
  return <canvas ref={ref} className={className} />;
});

export const Spectrogram = memo(function Spectrogram({ className }: { className?: string }) {
  const img = useRef<ImageData | null>(null);
  const ref = useCanvas((ctx, w, h) => {
    if (!engine.initialized) return;
    const an = engine.spectrum;
    const bins = an.frequencyBinCount;
    const data = new Uint8Array(bins);
    an.getByteFrequencyData(data);
    const t = theme();
    // scroll left by 1px
    const dpr = window.devicePixelRatio || 1;
    const cw = ctx.canvas.width;
    const ch = ctx.canvas.height;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.drawImage(ctx.canvas, -2, 0);
    if (!img.current || img.current.height !== ch) img.current = ctx.createImageData(2, ch);
    const id = img.current;
    const [ar, ag, ab] = [parseInt(t.accent.slice(1, 3), 16), parseInt(t.accent.slice(3, 5), 16), parseInt(t.accent.slice(5, 7), 16)];
    const sr = engine.ctx.sampleRate;
    for (let y = 0; y < ch; y++) {
      const f = 20 * Math.pow(1000, 1 - y / ch);
      const b = Math.min(bins - 1, Math.floor((f / (sr / 2)) * bins));
      const v = data[b] / 255;
      const e = v * v;
      for (let x = 0; x < 2; x++) {
        const o = (y * 2 + x) * 4;
        id.data[o] = Math.min(255, ar * e + 255 * e * e * 0.6);
        id.data[o + 1] = Math.min(255, ag * e + 255 * e * e * 0.4);
        id.data[o + 2] = Math.min(255, ab * e + 80 * e);
        id.data[o + 3] = 255;
      }
    }
    ctx.putImageData(id, cw - 2, 0);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    void w; void h;
  });
  return <canvas ref={ref} className={className} style={{ background: '#000' }} />;
});

/** Stereo vectorscope (goniometer). */
export const Goniometer = memo(function Goniometer({ className }: { className?: string }) {
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = 'rgba(0,0,0,0.28)';
    ctx.fillRect(0, 0, w, h);
    const cx = w / 2;
    const cy = h / 2;
    const r = Math.min(w, h) * 0.45;
    ctx.strokeStyle = rgba(t.text, 0.08);
    ctx.beginPath();
    ctx.moveTo(cx - r, cy); ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r); ctx.lineTo(cx, cy + r);
    ctx.stroke();
    if (!engine.initialized) return;
    engine.scope.getFloatTimeDomainData(timeBuf);
    engine.scopeR.getFloatTimeDomainData(timeBufR);
    ctx.fillStyle = rgba(t.accent2, 0.55);
    for (let i = 0; i < 1024; i += 2) {
      const l = timeBuf[i];
      const rr = timeBufR[i];
      const x = cx + ((rr - l) / Math.SQRT2) * r;
      const y = cy - ((l + rr) / Math.SQRT2) * r;
      ctx.fillRect(x, y, 1.2, 1.2);
    }
  });
  return <canvas ref={ref} className={className} />;
});

/** Analogue style VU meter with needle ballistics (300 ms). */
export const VUMeter = memo(function VUMeter({ className, channel }: { className?: string; channel: 0 | 1 }) {
  const state = useRef({ v: 0 });
  const ref = useCanvas((ctx, w, h, dt) => {
    const t = theme();
    ctx.clearRect(0, 0, w, h);
    const g = ctx.createLinearGradient(0, 0, 0, h);
    g.addColorStop(0, '#f3e5c0');
    g.addColorStop(1, '#d9c592');
    ctx.fillStyle = g;
    ctx.fillRect(0, 0, w, h);
    let rms = 0;
    if (engine.initialized) {
      const an = channel === 0 ? engine.scope : engine.scopeR;
      an.getFloatTimeDomainData(timeBuf);
      let s = 0;
      for (let i = 0; i < 2048; i++) s += timeBuf[i] * timeBuf[i];
      rms = Math.sqrt(s / 2048);
    }
    // 0 VU = -18 dBFS RMS
    const vu = Math.max(-20, Math.min(3, 20 * Math.log10(Math.max(1e-6, rms)) + 18));
    const target = (vu + 20) / 23;
    const st = state.current;
    st.v += (target - st.v) * Math.min(1, dt / 0.3 * 2.2);
    const cx = w / 2;
    const cy = h * 1.05;
    const r = h * 0.85;
    const ang = (n: number) => (-50 + n * 100) * (Math.PI / 180);
    ctx.lineWidth = 1;
    ctx.font = '8px sans-serif';
    ctx.textAlign = 'center';
    for (const mark of [-20, -10, -7, -5, -3, -1, 0, 1, 2, 3]) {
      const n = (mark + 20) / 23;
      const a = ang(n);
      ctx.strokeStyle = mark >= 0 ? '#c0392b' : '#3b3326';
      ctx.beginPath();
      ctx.moveTo(cx + Math.sin(a) * r * 0.9, cy - Math.cos(a) * r * 0.9);
      ctx.lineTo(cx + Math.sin(a) * r, cy - Math.cos(a) * r);
      ctx.stroke();
      ctx.fillStyle = mark >= 0 ? '#c0392b' : '#3b3326';
      ctx.fillText(String(mark), cx + Math.sin(a) * r * 0.78, cy - Math.cos(a) * r * 0.78 + 3);
    }
    ctx.strokeStyle = '#c0392b';
    ctx.lineWidth = 3;
    ctx.beginPath();
    ctx.arc(cx, cy, r, ang((0 + 20) / 23) - Math.PI / 2, ang(1) - Math.PI / 2);
    ctx.stroke();
    const a = ang(Math.max(0, Math.min(1.05, st.v)));
    ctx.strokeStyle = '#1b1b1b';
    ctx.lineWidth = 1.5;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.sin(a) * r, cy - Math.cos(a) * r);
    ctx.stroke();
    ctx.fillStyle = '#3b3326';
    ctx.font = 'bold 10px sans-serif';
    ctx.fillText(channel === 0 ? 'VU  L' : 'VU  R', cx, h * 0.62);
    void t;
  });
  return <canvas ref={ref} className={className} />;
});
