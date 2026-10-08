import { memo, useRef, useState, type PointerEvent as RPE } from 'react';
import { beginGesture, endGesture } from '../../state/store';
import { clamp } from '../../state/utils';

/** LCD style number that can be dragged vertically or edited by double click (e.g. BPM). */
export const DragNumber = memo(function DragNumber({
  value, min, max, onChange, decimals = 0, step = 1, width = 64, className = '', suffix, title,
}: {
  value: number; min: number; max: number; onChange: (v: number) => void; decimals?: number; step?: number;
  width?: number; className?: string; suffix?: string; title?: string;
}) {
  const drag = useRef<{ y: number; v: number; moved: boolean } | null>(null);
  const [editing, setEditing] = useState(false);
  const [text, setText] = useState('');

  const down = (e: RPE) => {
    if (editing || e.button !== 0) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    drag.current = { y: e.clientY, v: value, moved: false };
    beginGesture();
  };
  const move = (e: RPE) => {
    const d = drag.current;
    if (!d) return;
    const dy = d.y - e.clientY;
    if (Math.abs(dy) < 2 && !d.moved) return;
    d.moved = true;
    const s = e.shiftKey ? step / 10 : step;
    d.v = clamp(d.v + Math.round(dy / 3) * s, min, max);
    d.y = e.clientY - (dy % 3);
    onChange(Number(d.v.toFixed(Math.max(decimals, 3))));
  };
  const up = () => {
    drag.current = null;
    endGesture();
  };
  if (editing) {
    return (
      <input
        className={`lcd lcd-input ${className}`}
        style={{ width }}
        autoFocus
        value={text}
        onChange={(e) => setText(e.target.value)}
        onBlur={() => setEditing(false)}
        onKeyDown={(e) => {
          e.stopPropagation();
          if (e.key === 'Enter') {
            const v = parseFloat(text.replace(',', '.'));
            if (!isNaN(v)) onChange(clamp(v, min, max));
            setEditing(false);
          } else if (e.key === 'Escape') setEditing(false);
        }}
      />
    );
  }
  return (
    <div
      className={`lcd lcd-number ${className}`}
      style={{ width }}
      title={title ?? 'Drag to change, double-click to type'}
      onPointerDown={down}
      onPointerMove={move}
      onPointerUp={up}
      onDoubleClick={() => { setText(value.toFixed(decimals)); setEditing(true); }}
      onWheel={(e) => onChange(clamp(value + (e.deltaY < 0 ? step : -step), min, max))}
    >
      {value.toFixed(decimals)}
      {suffix && <span className="lcd-suffix">{suffix}</span>}
    </div>
  );
});
