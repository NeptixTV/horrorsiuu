import { memo, useEffect, useRef } from 'react';
import { onFrame } from './frameLoop';
import { theme } from '../../styles/themes';

export interface MeterSource {
  (): { l: number; r: number } | null;
}

const DB_MIN = -60;
const toPos = (v: number) => {
  if (v <= 0) return 0;
  const db = 20 * Math.log10(v);
  return Math.max(0, Math.min(1, (db - DB_MIN) / (6 - DB_MIN)));
};

/** Stereo peak meter with peak-hold and clip indicator, drawn on canvas. */
export const Meter = memo(function Meter({ source, width = 14, height = 140, stereo = true }: { source: MeterSource; width?: number; height?: number; stereo?: boolean }) {
  const ref = useRef<HTMLCanvasElement>(null);
  const srcRef = useRef(source);
  srcRef.current = source;
  useEffect(() => {
    const cv = ref.current!;
    const dpr = window.devicePixelRatio || 1;
    cv.width = width * dpr;
    cv.height = height * dpr;
    const ctx = cv.getContext('2d')!;
    const state = { l: 0, r: 0, hl: 0, hr: 0, ht: 0, clip: 0 };
    return onFrame((dt) => {
      const v = srcRef.current();
      const l = v ? toPos(v.l) : 0;
      const r = v ? toPos(v.r) : 0;
      const fall = dt * 1.6;
      state.l = Math.max(l, state.l - fall);
      state.r = Math.max(r, state.r - fall);
      if (l >= state.hl || r >= state.hr) state.ht = 1.2;
      state.ht -= dt;
      if (state.ht <= 0) {
        state.hl = Math.max(l, state.hl - fall * 0.5);
        state.hr = Math.max(r, state.hr - fall * 0.5);
      }
      state.hl = Math.max(state.hl, l);
      state.hr = Math.max(state.hr, r);
      if (v && (v.l >= 1 || v.r >= 1)) state.clip = 1.5;
      state.clip -= dt;
      const t = theme();
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      ctx.fillStyle = '#07080a';
      ctx.fillRect(0, 0, width, height);
      const bars = stereo ? [state.l, state.r] : [Math.max(state.l, state.r)];
      const holds = stereo ? [state.hl, state.hr] : [Math.max(state.hl, state.hr)];
      const bw = (width - 3) / bars.length;
      const top = 4;
      const h = height - top - 1;
      const grad = ctx.createLinearGradient(0, top + h, 0, top);
      grad.addColorStop(0, t.led);
      grad.addColorStop(0.72, '#d7e84a');
      grad.addColorStop(0.88, '#ffb340');
      grad.addColorStop(1, t.rec);
      bars.forEach((b, i) => {
        const x = 1 + i * (bw + 1);
        ctx.fillStyle = '#15181c';
        ctx.fillRect(x, top, bw, h);
        ctx.fillStyle = grad;
        ctx.fillRect(x, top + h * (1 - b), bw, h * b);
        ctx.fillStyle = '#fff';
        ctx.globalAlpha = 0.85;
        ctx.fillRect(x, top + h * (1 - holds[i]), bw, 1.5);
        ctx.globalAlpha = 1;
      });
      ctx.fillStyle = state.clip > 0 ? t.rec : '#2a2f35';
      ctx.fillRect(1, 0, width - 2, 3);
      // 0 dB tick
      const zero = top + h * (1 - toPos(1));
      ctx.fillStyle = 'rgba(255,255,255,0.25)';
      ctx.fillRect(0, zero, width, 1);
    });
  }, [width, height, stereo]);
  return <canvas ref={ref} style={{ width, height, display: 'block', borderRadius: 2 }} />;
});
