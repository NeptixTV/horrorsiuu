// Title bar of the desktop app (Windows): app icon, menu bar, project name.
// Windows draws its minimise / maximise / close buttons on the right side
// (titleBarOverlay); the empty space can be used to drag the window.
import { useEffect, useRef } from 'react';
import logo from '../../assets/logo.png';
import { useProject } from '../../state/store';
import { useUI } from '../../state/ui';
import { desktop } from '../../admin/desktop';
import { menuOpen, showMenu } from '../components/ContextMenu';
import { MENUS } from './Toolbar';

const toHex = (rgb: string) => {
  const m = /rgba?\((\d+),\s*(\d+),\s*(\d+)/.exec(rgb);
  return m ? `#${[m[1], m[2], m[3]].map((x) => Number(x).toString(16).padStart(2, '0')).join('')}` : null;
};

export function TitleBar() {
  const name = useProject((s) => s.project.name);
  const dirty = useProject((s) => s.dirty);
  const theme = useUI((s) => s.theme);
  const accent = useUI((s) => s.accent);
  const custom = useUI((s) => s.custom);
  const ref = useRef<HTMLDivElement>(null);
  const openFrom = useRef<number | null>(null);

  // the window buttons take the colours of the bar
  useEffect(() => {
    const id = setTimeout(() => {
      const el = ref.current;
      if (!el || !desktop?.setTitleBarColors) return;
      const cs = getComputedStyle(el);
      const bg = toHex(cs.backgroundColor);
      const fg = toHex(cs.color);
      if (bg && fg) void desktop.setTitleBarColors(bg, fg);
    }, 50);
    return () => clearTimeout(id);
  }, [theme, accent, custom]);

  const open = (i: number, el: HTMLElement) => {
    const r = el.getBoundingClientRect();
    openFrom.current = i;
    showMenu(r.left, r.bottom + 1, MENUS[i][1]());
  };

  return (
    <div className="titlebar" ref={ref} onDoubleClick={(e) => e.preventDefault()}>
      <img src={logo} className="titlebar-logo" alt="" />
      <div className="titlebar-menus">
        {MENUS.map(([label], i) => (
          <button
            key={label}
            className="titlebar-menu"
            onClick={(e) => open(i, e.currentTarget)}
            // like a real menu bar: while a menu is open, moving to another name opens that one
            onPointerEnter={(e) => { if (menuOpen() && openFrom.current !== null && openFrom.current !== i) open(i, e.currentTarget); }}
          >
            {label}
          </button>
        ))}
      </div>
      <div className="titlebar-title">{name}{dirty ? ' •' : ''} <span>– Goofy Studio</span></div>
    </div>
  );
}
