import { memo, useRef, useState, type PointerEvent as RPE, type WheelEvent as RWE } from 'react';
import { beginGesture, endGesture } from '../../state/store';
import { clamp } from '../../state/utils';
import './controls.css';

export interface KnobProps {
  value: number;
  min: number;
  max: number;
  defaultValue?: number;
  onChange: (v: number) => void;
  log?: boolean;
  step?: number;
  size?: number;
  label?: string;
  format?: (v: number) => string;
  color?: string;
  bipolar?: boolean;
  title?: string;
}

const toNorm = (v: number, min: number, max: number, log?: boolean) =>
  log ? Math.log(v / min) / Math.log(max / min) : (v - min) / (max - min);
const fromNorm = (n: number, min: number, max: number, log?: boolean) =>
  log ? min * Math.pow(max / min, n) : min + n * (max - min);

let wheelTimer: ReturnType<typeof setTimeout> | null = null;
/** Groups consecutive wheel changes into one undo step. */
export function wheelGesture() {
  beginGesture();
  if (wheelTimer) clearTimeout(wheelTimer);
  wheelTimer = setTimeout(endGesture, 450);
}

const START = -135;
const SWEEP = 270;

function arc(cx: number, cy: number, r: number, a0: number, a1: number) {
  const rad = (a: number) => ((a - 90) * Math.PI) / 180;
  const x0 = cx + r * Math.cos(rad(a0));
  const y0 = cy + r * Math.sin(rad(a0));
  const x1 = cx + r * Math.cos(rad(a1));
  const y1 = cy + r * Math.sin(rad(a1));
  const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
  const sweep = a1 > a0 ? 1 : 0;
  return `M${x0} ${y0}A${r} ${r} 0 ${large} ${sweep} ${x1} ${y1}`;
}

export const Knob = memo(function Knob({
  value, min, max, defaultValue, onChange, log, step, size = 34, label, format, color, bipolar, title,
}: KnobProps) {
  const drag = useRef<{ y: number; norm: number } | null>(null);
  const [active, setActive] = useState(false);
  const norm = clamp(toNorm(clamp(value, min, max), min, max, log), 0, 1);
  const angle = START + norm * SWEEP;
  const r = size / 2 - 3;
  const c = size / 2;
  const centerAngle = bipolar ? START + toNorm(log ? Math.sqrt(min * max) : (min + max) / 2, min, max, log) * SWEEP : START;

  const emit = (n: number) => {
    let v = fromNorm(clamp(n, 0, 1), min, max, log);
    if (step) v = Math.round(v / step) * step;
    onChange(clamp(v, min, max));
  };

  const onPointerDown = (e: RPE) => {
    if (e.button !== 0) return;
    e.preventDefault();
    (e.target as Element).setPointerCapture(e.pointerId);
    drag.current = { y: e.clientY, norm };
    setActive(true);
    beginGesture();
  };
  const onPointerMove = (e: RPE) => {
    const d = drag.current;
    if (!d) return;
    const sens = e.shiftKey ? 800 : 180;
    const n = d.norm + (d.y - e.clientY) / sens;
    d.norm = clamp(n, 0, 1);
    d.y = e.clientY;
    emit(d.norm);
  };
  const onPointerUp = () => {
    if (!drag.current) return;
    drag.current = null;
    setActive(false);
    endGesture();
  };
  const onWheel = (e: RWE) => {
    e.stopPropagation();
    const delta = (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? 0.005 : 0.03);
    wheelGesture();
    emit(norm + delta);
  };

  const text = format ? format(value) : value.toFixed(2);
  return (
    <div className={`knob ${active ? 'active' : ''}`} title={title ?? `${label ?? ''} ${text}`} style={{ width: Math.max(size, 40) }}>
      <svg
        width={size}
        height={size}
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onPointerCancel={onPointerUp}
        onDoubleClick={() => {
          if (defaultValue === undefined) return;
          onChange(defaultValue);
        }}
        onWheel={onWheel}
        style={{ ['--knob-color' as string]: color ?? 'var(--accent)' }}
      >
        <circle cx={c} cy={c} r={r - 3} className="knob-body" />
        <path d={arc(c, c, r, START, START + SWEEP)} className="knob-track" />
        {Math.abs(angle - centerAngle) > 0.5 && (
          <path d={arc(c, c, r, Math.min(centerAngle, angle), Math.max(centerAngle, angle))} className="knob-value" />
        )}
        <line
          x1={c + (r - 9) * Math.sin((angle * Math.PI) / 180) * 0.2}
          y1={c - (r - 9) * Math.cos((angle * Math.PI) / 180) * 0.2}
          x2={c + (r - 5) * Math.sin((angle * Math.PI) / 180)}
          y2={c - (r - 5) * Math.cos((angle * Math.PI) / 180)}
          className="knob-pointer"
        />
      </svg>
      {label && <div className="knob-label">{label}</div>}
      {active && <div className="knob-tip">{text}</div>}
    </div>
  );
});
