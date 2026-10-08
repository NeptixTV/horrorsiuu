import { PPQ } from './types';

let counter = 0;
export function uid(prefix = ''): string {
  counter = (counter + 1) % 1e6;
  return `${prefix}${Date.now().toString(36)}${Math.random().toString(36).slice(2, 7)}${counter.toString(36)}`;
}

export const clamp = (v: number, min: number, max: number) => Math.min(max, Math.max(min, v));

export function ticksToSeconds(ticks: number, bpm: number): number {
  return (ticks / PPQ) * (60 / bpm);
}

export function secondsToTicks(seconds: number, bpm: number): number {
  return (seconds * bpm * PPQ) / 60;
}

export function ticksPerBar(timeSig: [number, number]): number {
  return PPQ * timeSig[0] * (4 / timeSig[1]);
}

export function snapTicks(t: number, snap: number, mode: 'round' | 'floor' = 'round'): number {
  if (snap <= 0) return Math.round(t);
  return (mode === 'floor' ? Math.floor(t / snap) : Math.round(t / snap)) * snap;
}

/** Formats a tick position as BAR:BEAT:STEP (1-based) */
export function formatBBS(ticks: number, timeSig: [number, number]): string {
  const bar = ticksPerBar(timeSig);
  const beatTicks = PPQ * (4 / timeSig[1]);
  const b = Math.floor(ticks / bar);
  const beat = Math.floor((ticks - b * bar) / beatTicks);
  const step = Math.floor((ticks - b * bar - beat * beatTicks) / (PPQ / 4));
  return `${b + 1}:${String(beat + 1).padStart(2, '0')}:${String(step + 1).padStart(2, '0')}`;
}

export function formatTime(seconds: number): string {
  const m = Math.floor(seconds / 60);
  const s = Math.floor(seconds % 60);
  const ms = Math.floor((seconds % 1) * 1000);
  return `${m}:${String(s).padStart(2, '0')}:${String(ms).padStart(3, '0')}`;
}

export const linToDb = (v: number) => (v <= 0.00001 ? -Infinity : 20 * Math.log10(v));
export const dbToLin = (db: number) => Math.pow(10, db / 20);

export function formatDb(v: number): string {
  const db = linToDb(v);
  if (!isFinite(db)) return '-∞ dB';
  return `${db > 0 ? '+' : ''}${db.toFixed(1)} dB`;
}

export function formatPan(p: number): string {
  if (Math.abs(p) < 0.01) return 'C';
  return `${Math.round(Math.abs(p) * 100)}% ${p < 0 ? 'L' : 'R'}`;
}

export function formatHz(hz: number): string {
  return hz >= 1000 ? `${(hz / 1000).toFixed(hz >= 10000 ? 1 : 2)} kHz` : `${Math.round(hz)} Hz`;
}

/** Track / channel colour palette (distinct hues that read well on dark UIs). */
export const PALETTE = [
  '#f28c28', '#ffbe3d', '#9be15d', '#4cd6a3', '#3fc7e8', '#5b8cff', '#8f6bff', '#d46bff',
  '#ff6bb3', '#ff5c5c', '#c0c7cf', '#7d8a96', '#e3a75e', '#6fcf97', '#56ccf2', '#bb6bd9',
];

export function paletteColor(i: number): string {
  return PALETTE[((i % PALETTE.length) + PALETTE.length) % PALETTE.length];
}

export function hexToRgb(hex: string): [number, number, number] {
  const h = hex.replace('#', '');
  const full = h.length === 3 ? h.split('').map((c) => c + c).join('') : h;
  const n = parseInt(full, 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

export function rgba(hex: string, a: number): string {
  const [r, g, b] = hexToRgb(hex);
  return `rgba(${r},${g},${b},${a})`;
}

export function shade(hex: string, amount: number): string {
  const [r, g, b] = hexToRgb(hex);
  const f = (c: number) => Math.round(clamp(amount < 0 ? c * (1 + amount) : c + (255 - c) * amount, 0, 255));
  return `#${[f(r), f(g), f(b)].map((c) => c.toString(16).padStart(2, '0')).join('')}`;
}

export function debounce<T extends (...args: never[]) => void>(fn: T, ms: number) {
  let t: ReturnType<typeof setTimeout> | undefined;
  return (...args: Parameters<T>) => {
    if (t) clearTimeout(t);
    t = setTimeout(() => fn(...args), ms);
  };
}
