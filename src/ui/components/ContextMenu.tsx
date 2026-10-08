import { useEffect, useLayoutEffect, useRef, useState } from 'react';
import { create } from 'zustand';

export interface MenuItem {
  label?: string;
  shortcut?: string;
  onClick?: () => void;
  disabled?: boolean;
  checked?: boolean;
  danger?: boolean;
  separator?: boolean;
  submenu?: MenuItem[];
  color?: string;
}

interface MenuState {
  items: MenuItem[] | null;
  x: number;
  y: number;
}

const useMenu = create<MenuState>(() => ({ items: null, x: 0, y: 0 }));

export function showMenu(x: number, y: number, items: MenuItem[]) {
  useMenu.setState({ items, x, y });
}

export function hideMenu() {
  useMenu.setState({ items: null });
}

function MenuList({ items, x, y }: { items: MenuItem[]; x: number; y: number }) {
  const ref = useRef<HTMLDivElement>(null);
  const [pos, setPos] = useState({ x, y });
  const [open, setOpen] = useState<number | null>(null);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const r = el.getBoundingClientRect();
    setPos({
      x: Math.min(x, window.innerWidth - r.width - 4),
      y: Math.min(y, window.innerHeight - r.height - 4),
    });
  }, [x, y]);
  return (
    <div ref={ref} className="ctx-menu" style={{ left: pos.x, top: pos.y }} onContextMenu={(e) => e.preventDefault()}>
      {items.map((it, i) =>
        it.separator ? (
          <div key={i} className="ctx-sep" />
        ) : (
          <div
            key={i}
            className={`ctx-item ${it.disabled ? 'disabled' : ''} ${it.danger ? 'danger' : ''}`}
            onPointerEnter={() => setOpen(it.submenu ? i : null)}
            onClick={(e) => {
              e.stopPropagation();
              if (it.disabled || it.submenu) return;
              hideMenu();
              it.onClick?.();
            }}
          >
            <span className="ctx-check">{it.checked ? '✓' : it.color ? <span className="ctx-color" style={{ background: it.color }} /> : ''}</span>
            <span className="ctx-label">{it.label}</span>
            {it.shortcut && <span className="ctx-shortcut">{it.shortcut}</span>}
            {it.submenu && <span className="ctx-shortcut">›</span>}
            {it.submenu && open === i && <SubMenu items={it.submenu} />}
          </div>
        ),
      )}
    </div>
  );
}

function SubMenu({ items }: { items: MenuItem[] }) {
  const ref = useRef<HTMLSpanElement>(null);
  const [anchor, setAnchor] = useState<{ x: number; y: number } | null>(null);
  useLayoutEffect(() => {
    const r = ref.current?.parentElement?.getBoundingClientRect();
    if (r) setAnchor({ x: r.right - 2, y: r.top - 4 });
  }, []);
  return <span ref={ref}>{anchor && <MenuList items={items} x={anchor.x} y={anchor.y} />}</span>;
}

export function ContextMenuHost() {
  const { items, x, y } = useMenu();
  useEffect(() => {
    if (!items) return;
    const close = (e: Event) => {
      if (e instanceof KeyboardEvent && e.key !== 'Escape') return;
      if (e.target instanceof Element && e.target.closest('.ctx-menu')) return;
      hideMenu();
    };
    window.addEventListener('pointerdown', close, true);
    window.addEventListener('keydown', close, true);
    window.addEventListener('blur', hideMenu);
    return () => {
      window.removeEventListener('pointerdown', close, true);
      window.removeEventListener('keydown', close, true);
      window.removeEventListener('blur', hideMenu);
    };
  }, [items]);
  if (!items) return null;
  return <MenuList items={items} x={x} y={y} />;
}
