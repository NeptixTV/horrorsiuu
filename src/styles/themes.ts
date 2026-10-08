import type { CustomTheme, ThemeName } from '../state/ui';
import { shade } from '../state/utils';

export interface ThemeVars {
  bg: string;
  panel: string;
  panel2: string;
  panel3: string;
  border: string;
  text: string;
  dim: string;
  accent: string;
  accent2: string;
  grid: string;
  gridAlt: string;
  gridLine: string;
  gridBar: string;
  led: string;
  rec: string;
}

export const THEMES: Record<Exclude<ThemeName, 'custom'>, ThemeVars & { label: string }> = {
  dark: {
    label: 'Dark', bg: '#121417', panel: '#1c2025', panel2: '#252a30', panel3: '#2f353c', border: '#0a0b0d', text: '#d3d9df',
    dim: '#7c8792', accent: '#f28c28', accent2: '#ffc15e', grid: '#2a333b', gridAlt: '#262e35', gridLine: '#323c45', gridBar: '#46525d',
    led: '#8fe04f', rec: '#ff3b3b',
  },
  darkblue: {
    label: 'Dark Blue', bg: '#1b2229', panel: '#28323b', panel2: '#313d47', panel3: '#3b4853', border: '#141a1f', text: '#d6dee5',
    dim: '#8696a3', accent: '#ff9a1f', accent2: '#ffd27a', grid: '#3a4955', gridAlt: '#35434e', gridLine: '#43535f', gridBar: '#5a6c7a',
    led: '#9be15d', rec: '#ff4545',
  },
  purple: {
    label: 'Purple', bg: '#120e19', panel: '#1d1628', panel2: '#261d34', panel3: '#30253f', border: '#09060d', text: '#e2d9ee',
    dim: '#8f80a3', accent: '#b46bff', accent2: '#ff8bd8', grid: '#271f33', gridAlt: '#231c2e', gridLine: '#30273e', gridBar: '#4a3c5e',
    led: '#7dfcc0', rec: '#ff4d7d',
  },
  neon: {
    label: 'Neon', bg: '#05060a', panel: '#0d0f16', panel2: '#141722', panel3: '#1b1f2d', border: '#000000', text: '#e6f6ff',
    dim: '#6f8296', accent: '#00e5ff', accent2: '#ff2bd6', grid: '#0f1320', gridAlt: '#0c101b', gridLine: '#18203a', gridBar: '#273463',
    led: '#39ff88', rec: '#ff2b5e',
  },
};

export function resolveTheme(name: ThemeName, custom: CustomTheme, accentOverride: string | null): ThemeVars {
  let base: ThemeVars;
  if (name === 'custom') {
    base = {
      bg: custom.bg, panel: custom.panel, panel2: shade(custom.panel, 0.06), panel3: shade(custom.panel, 0.12), border: shade(custom.bg, -0.5),
      text: custom.text, dim: shade(custom.text, -0.42), accent: custom.accent, accent2: shade(custom.accent, 0.35),
      grid: custom.grid, gridAlt: shade(custom.grid, -0.08), gridLine: shade(custom.grid, 0.08), gridBar: shade(custom.grid, 0.25),
      led: '#8fe04f', rec: '#ff3b3b',
    };
  } else {
    base = { ...THEMES[name] };
  }
  if (accentOverride && name !== 'custom') {
    base.accent = accentOverride;
    base.accent2 = shade(accentOverride, 0.35);
  }
  return base;
}

let current: ThemeVars = THEMES.dark;
let version = 0;

export function applyTheme(vars: ThemeVars) {
  current = vars;
  version++;
  const root = document.documentElement;
  const map: Record<string, string> = {
    '--bg': vars.bg, '--panel': vars.panel, '--panel2': vars.panel2, '--panel3': vars.panel3, '--border': vars.border,
    '--text': vars.text, '--dim': vars.dim, '--accent': vars.accent, '--accent2': vars.accent2, '--grid': vars.grid,
    '--grid-alt': vars.gridAlt, '--grid-line': vars.gridLine, '--grid-bar': vars.gridBar, '--led': vars.led, '--rec': vars.rec,
  };
  for (const [k, v] of Object.entries(map)) root.style.setProperty(k, v);
}

/** Theme colours for canvas drawing. */
export function theme(): ThemeVars {
  return current;
}

export function themeVersion() {
  return version;
}
