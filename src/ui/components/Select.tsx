// Drop-down list in the app's own style (replaces the browser's <select>).
// Same usage as <select>: <Select value onChange={(e) => e.target.value}>
// with <option> / <optgroup> children. Keyboard: ↑ ↓ change the value,
// Enter / Space / Alt+↓ open the list, typing jumps to an entry.
import { Children, isValidElement, useEffect, useLayoutEffect, useRef, useState, type CSSProperties, type KeyboardEvent, type ReactElement, type ReactNode } from 'react';
import { create } from 'zustand';

interface Opt {
  value: string;
  label: string;
  disabled?: boolean;
  group?: string;
}

export interface SelectProps {
  value: string | number | undefined | null;
  onChange: (e: { target: { value: string } }) => void;
  children?: ReactNode;
  className?: string;
  style?: CSSProperties;
  title?: string;
  disabled?: boolean;
  onKeyDown?: (e: KeyboardEvent<HTMLButtonElement>) => void;
}

const textOf = (n: ReactNode): string => {
  if (n === null || n === undefined || typeof n === 'boolean') return '';
  if (typeof n === 'string' || typeof n === 'number') return String(n);
  if (Array.isArray(n)) return n.map(textOf).join('');
  if (isValidElement(n)) return textOf((n.props as { children?: ReactNode }).children);
  return '';
};

/** Reads <option> / <optgroup> children (also nested in arrays and fragments). */
export function readOptions(children: ReactNode, group?: string, out: Opt[] = []): Opt[] {
  Children.forEach(children, (c) => {
    if (!isValidElement(c)) return;
    const el = c as ReactElement<{ value?: string | number; children?: ReactNode; disabled?: boolean; label?: string }>;
    if (el.type === 'option') {
      const label = textOf(el.props.children);
      out.push({ value: el.props.value !== undefined ? String(el.props.value) : label, label, disabled: el.props.disabled, group });
    } else if (el.type === 'optgroup') {
      readOptions(el.props.children, el.props.label, out);
    } else if (el.props.children) {
      readOptions(el.props.children, group, out);
    }
  });
  return out;
}

interface PopupState {
  open: { rect: DOMRect; options: Opt[]; value: string; pick: (v: string) => void; close: () => void } | null;
}
const usePopup = create<PopupState>(() => ({ open: null }));

export function Select({ value, onChange, children, className = '', style, title, disabled, onKeyDown }: SelectProps) {
  const ref = useRef<HTMLButtonElement>(null);
  const options = readOptions(children);
  const v = value === undefined || value === null ? '' : String(value);
  const cur = options.find((o) => o.value === v);
  const [isOpen, setOpen] = useState(false);
  const pick = (nv: string) => { if (nv !== v) onChange({ target: { value: nv } }); };
  const open = () => {
    if (disabled || !ref.current) return;
    setOpen(true);
    usePopup.setState({ open: { rect: ref.current.getBoundingClientRect(), options, value: v, pick, close: () => { setOpen(false); ref.current?.focus(); } } });
  };
  const step = (dir: number) => {
    const list = options.filter((o) => !o.disabled);
    const i = list.findIndex((o) => o.value === v);
    const n = list[Math.max(0, Math.min(list.length - 1, (i < 0 ? 0 : i) + dir))];
    if (n) pick(n.value);
  };
  return (
    <button
      ref={ref}
      type="button"
      className={`${className} gsel ${isOpen ? 'open' : ''}`}
      style={style}
      title={title}
      disabled={disabled}
      onPointerDown={(e) => { if (e.button === 0) { e.preventDefault(); if (isOpen) usePopup.getState().open?.close(); else open(); } }}
      onKeyDown={(e) => {
        onKeyDown?.(e);
        if (e.defaultPrevented) return;
        if (e.key === 'ArrowDown' && e.altKey) { e.preventDefault(); open(); }
        else if (e.key === 'ArrowDown' || e.key === 'ArrowRight') { e.preventDefault(); e.stopPropagation(); step(1); }
        else if (e.key === 'ArrowUp' || e.key === 'ArrowLeft') { e.preventDefault(); e.stopPropagation(); step(-1); }
        else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.stopPropagation(); open(); }
      }}
    >
      <span className="gsel-label">{cur?.label ?? ''}</span>
      <svg className="gsel-arrow" width="9" height="9" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3"><path d="m6 9 6 6 6-6" /></svg>
    </button>
  );
}

/** The open list – rendered once at the app root. */
export function SelectPopupHost() {
  const st = usePopup((s) => s.open);
  const listRef = useRef<HTMLDivElement>(null);
  const [hi, setHi] = useState(-1);
  const [pos, setPos] = useState<{ left: number; top: number; width: number; maxH: number } | null>(null);
  const typed = useRef({ text: '', at: 0 });

  useLayoutEffect(() => {
    if (!st) { setPos(null); return; }
    const r = st.rect;
    const below = window.innerHeight - r.bottom - 8;
    const above = r.top - 8;
    const want = Math.min(420, st.options.length * 22 + (new Set(st.options.map((o) => o.group)).size > 1 ? 120 : 8));
    const down = below >= Math.min(want, 220) || below >= above;
    const maxH = Math.max(120, Math.min(want, down ? below : above));
    setPos({ left: Math.min(r.left, window.innerWidth - Math.max(r.width, 160) - 6), top: down ? r.bottom + 2 : r.top - 2 - maxH, width: r.width, maxH });
    setHi(st.options.findIndex((o) => o.value === st.value));
  }, [st]);

  useEffect(() => {
    // the chosen entry is visible when the list opens
    const el = listRef.current?.querySelector('.gsel-opt.cur') as HTMLElement | null;
    el?.scrollIntoView({ block: 'center' });
  }, [pos]);

  useEffect(() => {
    if (!st) return;
    const close = () => { usePopup.setState({ open: null }); st.close(); };
    const onDown = (e: PointerEvent) => { if (!listRef.current?.contains(e.target as Node)) close(); };
    const onKey = (e: globalThis.KeyboardEvent) => {
      const opts = st.options;
      if (e.key === 'Escape' || e.key === 'Tab') { e.preventDefault(); close(); return; }
      if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
        e.preventDefault();
        setHi((h) => {
          let n = h;
          for (let k = 0; k < opts.length; k++) {
            n = (n + (e.key === 'ArrowDown' ? 1 : -1) + opts.length) % opts.length;
            if (!opts[n].disabled) break;
          }
          listRef.current?.querySelectorAll('.gsel-opt')[n]?.scrollIntoView({ block: 'nearest' });
          return n;
        });
        return;
      }
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        setHi((h) => { if (h >= 0 && !opts[h].disabled) st.pick(opts[h].value); return h; });
        close();
        return;
      }
      if (e.key.length === 1 && /\S/.test(e.key)) {
        const now = performance.now();
        typed.current = { text: (now - typed.current.at < 800 ? typed.current.text : '') + e.key.toLowerCase(), at: now };
        const i = opts.findIndex((o) => !o.disabled && o.label.toLowerCase().startsWith(typed.current.text));
        if (i >= 0) {
          setHi(i);
          listRef.current?.querySelectorAll('.gsel-opt')[i]?.scrollIntoView({ block: 'nearest' });
        }
      }
    };
    window.addEventListener('pointerdown', onDown, true);
    window.addEventListener('keydown', onKey, true);
    window.addEventListener('resize', close);
    return () => {
      window.removeEventListener('pointerdown', onDown, true);
      window.removeEventListener('keydown', onKey, true);
      window.removeEventListener('resize', close);
    };
  }, [st]);

  if (!st || !pos) return null;
  let lastGroup: string | undefined;
  return (
    <div
      ref={listRef}
      className="gsel-pop"
      style={{ left: pos.left, top: pos.top, minWidth: Math.max(pos.width, 120), maxHeight: pos.maxH }}
      onContextMenu={(e) => e.preventDefault()}
      onWheel={(e) => e.stopPropagation()}
    >
      {st.options.map((o, i) => {
        const head = o.group !== lastGroup && o.group ? o.group : null;
        lastGroup = o.group;
        return (
          <div key={`${o.group ?? ''}|${o.value}|${i}`}>
            {head && <div className="gsel-group">{head}</div>}
            <div
              className={`gsel-opt ${o.value === st.value ? 'cur' : ''} ${i === hi ? 'hi' : ''} ${o.disabled ? 'disabled' : ''} ${o.group ? 'in-group' : ''}`}
              onPointerEnter={() => setHi(i)}
              onPointerUp={() => {
                if (o.disabled) return;
                st.pick(o.value);
                usePopup.setState({ open: null });
                st.close();
              }}
            >
              <span className="gsel-check">{o.value === st.value ? '✓' : ''}</span>
              {o.label}
            </div>
          </div>
        );
      })}
    </div>
  );
}
