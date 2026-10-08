import { memo, useRef, type PointerEvent as RPE } from 'react';
import { beginGesture, endGesture } from '../../state/store';
import { clamp } from '../../state/utils';
import { wheelGesture } from './Knob';

// Fader law: position^2 * max (gives finer control near unity like hardware faders)
const MAXV = 1.25;
export const faderToGain = (pos: number) => pos * pos * MAXV;
export const gainToFader = (g: number) => Math.sqrt(clamp(g, 0, MAXV) / MAXV);

export const Fader = memo(function Fader({
  value, onChange, height = 140, defaultValue = 0.8, title,
}: { value: number; onChange: (v: number) => void; height?: number; defaultValue?: number; title?: string }) {
  const drag = useRef<{ y: number; pos: number } | null>(null);
  const pos = gainToFader(value);
  const thumbH = 22;
  const travel = height - thumbH;
  const top = (1 - pos) * travel;

  const down = (e: RPE) => {
    if (e.button !== 0) return;
    e.preventDefault();
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    // relative drag from anywhere on the fader (no accidental jumps)
    drag.current = { y: e.clientY, pos };
    beginGesture();
  };
  const move = (e: RPE) => {
    const d = drag.current;
    if (!d) return;
    const sens = e.shiftKey ? 4 : 1;
    d.pos = clamp(d.pos + (d.y - e.clientY) / travel / sens, 0, 1);
    d.y = e.clientY;
    onChange(faderToGain(d.pos));
  };
  const up = () => {
    if (!drag.current) return;
    drag.current = null;
    endGesture();
  };
  // 0 dB marker
  const unity = (1 - gainToFader(1)) * travel + thumbH / 2;
  return (
    <div
      className="fader"
      style={{ height }}
      title={title}
      onPointerDown={down}
      onPointerMove={move}
      onPointerUp={up}
      onPointerCancel={up}
      onDoubleClick={() => onChange(defaultValue)}
      onWheel={(e) => { wheelGesture(); onChange(faderToGain(clamp(pos + (e.deltaY < 0 ? 0.02 : -0.02), 0, 1))); }}
    >
      <div className="fader-slot" />
      <div className="fader-unity" style={{ top: unity }} />
      <div className="fader-fill" style={{ top: top + thumbH / 2 }} />
      <div className="fader-thumb" style={{ top }} />
    </div>
  );
});
