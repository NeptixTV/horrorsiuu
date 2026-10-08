import { useRef, type ReactNode } from 'react';
import { closeWindow, focusWindow, moveWindow, type FloatingWindow } from '../../state/ui';
import { Icon } from './Icon';

/** Draggable floating plugin window (like plugin editors in a DAW). */
export function Window({ w, title, color, children, width, onClose, actions }: {
  w: FloatingWindow; title: ReactNode; color?: string; children: ReactNode; width?: number; onClose?: () => void; actions?: ReactNode;
}) {
  const drag = useRef<{ dx: number; dy: number } | null>(null);
  return (
    <div
      className="fwin"
      style={{ left: w.x, top: w.y, zIndex: 100 + w.z, width, ['--win-color' as string]: color ?? 'var(--accent)' }}
      onPointerDown={() => focusWindow(w.id)}
    >
      <div
        className="fwin-title"
        onPointerDown={(e) => {
          if ((e.target as HTMLElement).closest('button')) return;
          (e.currentTarget as Element).setPointerCapture(e.pointerId);
          drag.current = { dx: e.clientX - w.x, dy: e.clientY - w.y };
        }}
        onPointerMove={(e) => {
          if (!drag.current) return;
          const x = Math.max(-200, Math.min(window.innerWidth - 80, e.clientX - drag.current.dx));
          const y = Math.max(0, Math.min(window.innerHeight - 30, e.clientY - drag.current.dy));
          moveWindow(w.id, x, y);
        }}
        onPointerUp={() => { drag.current = null; }}
      >
        <span className="fwin-dot" />
        <span className="fwin-name">{title}</span>
        <span className="spacer" />
        {actions}
        <button className="btn ghost icon small" onClick={() => (onClose ? onClose() : closeWindow(w.id))} title="Close">
          <Icon name="close" size={12} />
        </button>
      </div>
      <div className="fwin-body">{children}</div>
    </div>
  );
}

export function Modal({ children, onClose, width = 520, title }: { children: ReactNode; onClose?: () => void; width?: number; title?: ReactNode }) {
  return (
    <div className="modal-backdrop" onPointerDown={(e) => { if (e.target === e.currentTarget) onClose?.(); }}>
      <div className="modal" style={{ width }}>
        {title && (
          <div className="modal-title">
            {title}
            <span className="spacer" />
            {onClose && <button className="btn ghost icon small" onClick={onClose}><Icon name="close" size={12} /></button>}
          </div>
        )}
        {children}
      </div>
    </div>
  );
}
