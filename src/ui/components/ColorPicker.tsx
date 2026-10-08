import { PALETTE } from '../../state/utils';
import type { MenuItem } from './ContextMenu';

/** Colour submenu for context menus. */
export function colorMenu(current: string, onPick: (c: string) => void): MenuItem[] {
  return PALETTE.map((c) => ({ label: c.toUpperCase(), color: c, checked: c === current, onClick: () => onPick(c) }));
}

export function ColorDot({ color, onClick, size = 12 }: { color: string; onClick?: (e: React.MouseEvent) => void; size?: number }) {
  return (
    <span
      className="color-dot"
      onClick={onClick}
      style={{ background: color, width: size, height: size, cursor: onClick ? 'pointer' : undefined }}
    />
  );
}
