// DreamChord presets: dreamy, warm, nostalgic ambient sounds.
// A preset sets the sound; key, octave and the performance settings (arp,
// strum, hold, joystick) stay as they are unless the preset is built around
// them (e.g. the arpeggio presets).
import type { DreamParams } from '../../state/types';
import { defaultDream } from '../../state/defaults';

export interface DreamPreset {
  name: string;
  category: 'Pads' | 'Keys' | 'Lo-Fi' | 'Ambient' | 'Retro' | 'Plucks';
  /** short description shown in the display */
  info: string;
  color: string;
  params: Partial<DreamParams>;
}

export const DREAM_PRESETS: DreamPreset[] = [
  {
    name: 'Ocean Dream', category: 'Pads', color: '#3fc7e8', info: 'Soft shimmering pad with waves of chorus',
    params: {},
  },
  {
    name: 'Nostalgic Memories', category: 'Lo-Fi', color: '#e3a75e', info: 'Warm worn-out keys from an old tape',
    params: {
      waveA: 'piano', waveB: 'sine', mixB: 0.25, detune: 6, sub: 0.15, air: 0.05, cutoff: 3200, resonance: 0.5, filterEnv: 0.3,
      attack: 0.005, decay: 2.4, sustain: 0.25, release: 1.6, vibrato: 0.05, tape: 0.55, chorus: 0.35, delay: 0.2, delayTime: '1/8d',
      reverb: 0.45, space: 0.6, volume: 0.8,
    },
  },
  {
    name: 'Midnight Beach', category: 'Ambient', color: '#5b8cff', info: 'Slow breathing pad, sea breeze and long echoes',
    params: {
      waveA: 'sine', waveB: 'triangle', mixB: 0.5, detune: 12, sub: 0.3, air: 0.22, cutoff: 1800, resonance: 1, filterEnv: 0.15,
      attack: 1.2, decay: 2, sustain: 0.85, release: 3.5, vibrato: 0.2, tape: 0.25, chorus: 0.7, delay: 0.35, delayTime: '1/4',
      reverb: 0.7, space: 0.9, volume: 0.8,
    },
  },
  {
    name: 'Warm Sunset', category: 'Pads', color: '#ff8a5c', info: 'Analog saw pad, golden and wide',
    params: {
      waveA: 'sawtooth', waveB: 'sawtooth', mixB: 0.5, detune: 18, sub: 0.35, air: 0.06, cutoff: 1400, resonance: 2, filterEnv: 0.35,
      attack: 0.45, decay: 1.5, sustain: 0.7, release: 2.5, vibrato: 0.1, tape: 0.3, chorus: 0.5, delay: 0.15, delayTime: '1/8d',
      reverb: 0.45, space: 0.7, volume: 0.7,
    },
  },
  {
    name: 'Dreamy Piano', category: 'Keys', color: '#f2e6c9', info: 'Felt piano with glassy overtones in a big room',
    params: {
      waveA: 'piano', waveB: 'glass', mixB: 0.2, detune: 4, sub: 0.1, air: 0.03, cutoff: 5200, resonance: 0.4, filterEnv: 0.2,
      attack: 0.004, decay: 3.2, sustain: 0.1, release: 2.4, vibrato: 0, tape: 0.12, chorus: 0.25, delay: 0.3, delayTime: '1/8d',
      reverb: 0.6, space: 0.85, volume: 0.85,
    },
  },
  {
    name: 'Space Ambient', category: 'Ambient', color: '#8f6bff', info: 'Endless choir clouds drifting in space',
    params: {
      waveA: 'choir', waveB: 'sawtooth', mixB: 0.3, detune: 22, sub: 0.2, air: 0.25, cutoff: 2200, resonance: 3, filterEnv: 0.1,
      attack: 2, decay: 2, sustain: 0.9, release: 5, vibrato: 0.25, tape: 0.1, chorus: 0.8, delay: 0.45, delayTime: '1/4',
      reverb: 0.9, space: 1, volume: 0.75,
    },
  },
  {
    name: 'Retro Synth', category: 'Retro', color: '#ff5cb8', info: '80s poly synth with a bright filter snap',
    params: {
      waveA: 'square', waveB: 'sawtooth', mixB: 0.45, detune: 10, sub: 0.3, air: 0, cutoff: 2800, resonance: 4, filterEnv: 0.55,
      attack: 0.01, decay: 0.6, sustain: 0.55, release: 0.7, vibrato: 0.1, tape: 0.15, chorus: 0.6, delay: 0.3, delayTime: '1/8',
      reverb: 0.3, space: 0.5, volume: 0.65,
    },
  },
  {
    name: 'Lo-Fi Cassette', category: 'Lo-Fi', color: '#c9a36b', info: 'Wobbly tape keys with hiss and saturation',
    params: {
      waveA: 'piano', waveB: 'triangle', mixB: 0.4, detune: 9, sub: 0.2, air: 0.08, cutoff: 2100, resonance: 1, filterEnv: 0.2,
      attack: 0.01, decay: 1.6, sustain: 0.35, release: 1.2, vibrato: 0.12, tape: 0.85, chorus: 0.3, delay: 0.18, delayTime: '1/8d',
      reverb: 0.35, space: 0.55, volume: 0.85,
    },
  },
  {
    name: 'Glass Garden', category: 'Keys', color: '#9be15d', info: 'Crystal bells that ring out forever',
    params: {
      waveA: 'glass', waveB: 'sine', mixB: 0.3, detune: 3, sub: 0, air: 0.04, cutoff: 9000, resonance: 0.5, filterEnv: 0.1,
      attack: 0.002, decay: 2.5, sustain: 0, release: 2.5, vibrato: 0.04, tape: 0.05, chorus: 0.2, delay: 0.35, delayTime: '1/8d',
      reverb: 0.65, space: 0.8, volume: 0.8,
    },
  },
  {
    name: 'Aurora Choir', category: 'Ambient', color: '#56ccf2', info: 'Breathy voices, slowly swelling',
    params: {
      waveA: 'choir', waveB: 'choir', mixB: 0.5, detune: 16, sub: 0.15, air: 0.3, cutoff: 3000, resonance: 1.5, filterEnv: 0.1,
      attack: 0.8, decay: 2, sustain: 0.85, release: 3, vibrato: 0.3, tape: 0.1, chorus: 0.6, delay: 0.2, delayTime: '1/4',
      reverb: 0.75, space: 0.9, volume: 0.75,
    },
  },
  {
    name: 'Velvet Keys', category: 'Keys', color: '#d46bff', info: 'Round electric-piano tone with a soft chorus',
    params: {
      waveA: 'sine', waveB: 'piano', mixB: 0.35, detune: 5, sub: 0.2, air: 0, cutoff: 3600, resonance: 0.6, filterEnv: 0.25,
      attack: 0.006, decay: 1.8, sustain: 0.3, release: 1, vibrato: 0.08, tape: 0.25, chorus: 0.45, delay: 0.12, delayTime: '1/8',
      reverb: 0.3, space: 0.5, volume: 0.85,
    },
  },
  {
    name: 'Cloud Strings', category: 'Pads', color: '#f28c28', info: 'Wide string ensemble for big chords',
    params: {
      waveA: 'sawtooth', waveB: 'sawtooth', mixB: 0.6, detune: 25, sub: 0.15, air: 0.1, cutoff: 2400, resonance: 0.8, filterEnv: 0.15,
      attack: 1, decay: 1.5, sustain: 0.85, release: 2.2, vibrato: 0.2, tape: 0.1, chorus: 0.7, delay: 0.1, delayTime: '1/8d',
      reverb: 0.55, space: 0.8, volume: 0.65,
    },
  },
  {
    name: 'Pocket Pluck', category: 'Plucks', color: '#4cd6a3', info: 'Bouncy arpeggio pluck – hold a chord',
    params: {
      waveA: 'triangle', waveB: 'square', mixB: 0.3, detune: 6, sub: 0.15, air: 0, cutoff: 3000, resonance: 3, filterEnv: 0.6,
      attack: 0.002, decay: 0.35, sustain: 0, release: 0.4, vibrato: 0, tape: 0.15, chorus: 0.3, delay: 0.35, delayTime: '1/8d',
      reverb: 0.4, space: 0.6, volume: 0.85, arp: 'up', arpRate: '1/16', arpOctaves: 2, arpGate: 0.6,
    },
  },
  {
    name: 'Music Box', category: 'Plucks', color: '#ffbe3d', info: 'Tiny bells playing your chord up and down',
    params: {
      waveA: 'glass', waveB: 'sine', mixB: 0.4, detune: 2, sub: 0, air: 0, cutoff: 8000, resonance: 0.5, filterEnv: 0.1,
      attack: 0.002, decay: 1.2, sustain: 0, release: 1.4, vibrato: 0, tape: 0.2, chorus: 0.15, delay: 0.25, delayTime: '1/8',
      reverb: 0.55, space: 0.7, volume: 0.8, octave: 1, arp: 'updown', arpRate: '1/8', arpOctaves: 2, arpGate: 0.9,
    },
  },
];

/** Fields that belong to the sound (reset to the base sound before a preset is applied). */
const SOUND_KEYS: (keyof DreamParams)[] = [
  'waveA', 'waveB', 'mixB', 'detune', 'sub', 'air', 'cutoff', 'resonance', 'filterEnv', 'attack', 'decay', 'sustain', 'release',
  'vibrato', 'tape', 'chorus', 'delay', 'delayTime', 'reverb', 'space', 'volume',
];

/** Applies a preset to the current settings (keeps key, octave and performance settings). */
export function applyDreamPreset(current: DreamParams, preset: DreamPreset): DreamParams {
  const base = defaultDream();
  const next: DreamParams = { ...current, preset: preset.name };
  const n = next as unknown as Record<string, unknown>;
  const b = base as unknown as Record<string, unknown>;
  for (const k of SOUND_KEYS) n[k] = b[k];
  return { ...next, ...preset.params, preset: preset.name };
}

export function findDreamPreset(name: string) {
  return DREAM_PRESETS.find((p) => p.name === name);
}
