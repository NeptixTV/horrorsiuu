import { useRef } from 'react';

/** Drag handle between panels. `dir="x"` resizes widths, `"y"` heights. */
export function Splitter({ dir, onDrag, onEnd }: { dir: 'x' | 'y'; onDrag: (delta: number) => void; onEnd?: () => void }) {
  const last = useRef<number | null>(null);
  return (
    <div
      className={`splitter splitter-${dir}`}
      onPointerDown={(e) => {
        (e.currentTarget as Element).setPointerCapture(e.pointerId);
        last.current = dir === 'x' ? e.clientX : e.clientY;
        document.body.style.cursor = dir === 'x' ? 'col-resize' : 'row-resize';
      }}
      onPointerMove={(e) => {
        if (last.current === null) return;
        const p = dir === 'x' ? e.clientX : e.clientY;
        onDrag(p - last.current);
        last.current = p;
      }}
      onPointerUp={() => {
        last.current = null;
        document.body.style.cursor = '';
        onEnd?.();
      }}
    />
  );
}
