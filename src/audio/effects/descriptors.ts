import type { EffectType, ParamValue } from '../../state/types';

export interface NumberParamDef {
  key: string;
  label: string;
  kind: 'number';
  min: number;
  max: number;
  default: number;
  unit?: string;
  /** log scaling for frequency-like params */
  log?: boolean;
  step?: number;
}

export interface ChoiceParamDef {
  key: string;
  label: string;
  kind: 'choice';
  options: { value: string; label: string }[];
  default: string;
}

export type ParamDef = NumberParamDef | ChoiceParamDef;

export interface EffectDescriptor {
  type: EffectType;
  name: string;
  short: string;
  category: 'EQ & Filter' | 'Dynamics' | 'Space' | 'Modulation' | 'Character' | 'Vocal';
  color: string;
  params: ParamDef[];
}

const n = (key: string, label: string, min: number, max: number, def: number, unit = '', log = false): NumberParamDef => ({
  key, label, kind: 'number', min, max, default: def, unit, log,
});

const KEYS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

export const EFFECTS: Record<EffectType, EffectDescriptor> = {
  eq: {
    type: 'eq', name: 'Goofy EQ', short: 'EQ', category: 'EQ & Filter', color: '#3fc7e8',
    params: [
      n('low', 'Low', -18, 18, 0, 'dB'),
      n('lowFreq', 'Low Freq', 30, 400, 90, 'Hz', true),
      n('lowMid', 'Low Mid', -18, 18, 0, 'dB'),
      n('lowMidFreq', 'LM Freq', 120, 1200, 320, 'Hz', true),
      n('mid', 'Mid', -18, 18, 0, 'dB'),
      n('midFreq', 'Mid Freq', 400, 4000, 1200, 'Hz', true),
      n('highMid', 'High Mid', -18, 18, 0, 'dB'),
      n('highMidFreq', 'HM Freq', 1500, 9000, 3800, 'Hz', true),
      n('high', 'High', -18, 18, 0, 'dB'),
      n('highFreq', 'High Freq', 4000, 18000, 9500, 'Hz', true),
      n('q', 'Q', 0.3, 6, 0.9),
      n('gain', 'Gain', -18, 18, 0, 'dB'),
    ],
  },
  compressor: {
    type: 'compressor', name: 'Goofy Comp', short: 'COMP', category: 'Dynamics', color: '#ffbe3d',
    params: [
      n('threshold', 'Threshold', -60, 0, -18, 'dB'),
      n('ratio', 'Ratio', 1, 20, 4, ':1'),
      n('attack', 'Attack', 0.1, 200, 10, 'ms', true),
      n('release', 'Release', 10, 1000, 160, 'ms', true),
      n('knee', 'Knee', 0, 30, 6, 'dB'),
      n('makeup', 'Makeup', 0, 24, 3, 'dB'),
    ],
  },
  reverb: {
    type: 'reverb', name: 'Goofy Verb', short: 'VERB', category: 'Space', color: '#8f6bff',
    params: [
      n('size', 'Room Size', 0.05, 1, 0.55),
      n('decay', 'Decay', 0.2, 10, 2.4, 's', true),
      n('predelay', 'Pre-Delay', 0, 200, 15, 'ms'),
      n('damping', 'Damping', 1000, 18000, 7000, 'Hz', true),
      n('wet', 'Wet', 0, 1, 0.3),
      n('dry', 'Dry', 0, 1, 1),
    ],
  },
  delay: {
    type: 'delay', name: 'Goofy Delay', short: 'DLY', category: 'Space', color: '#4cd6a3',
    params: [
      { key: 'sync', label: 'Sync', kind: 'choice', default: '1/8d', options: [
        { value: 'off', label: 'Free' }, { value: '1/4', label: '1/4' }, { value: '1/8', label: '1/8' },
        { value: '1/8d', label: '1/8 dot' }, { value: '1/16', label: '1/16' }, { value: '1/4t', label: '1/4 trip' },
      ] },
      n('time', 'Time', 10, 2000, 375, 'ms', true),
      n('feedback', 'Feedback', 0, 0.95, 0.4),
      n('tone', 'Tone', 500, 18000, 6000, 'Hz', true),
      n('pingpong', 'Ping-Pong', 0, 1, 0.5),
      n('mix', 'Mix', 0, 1, 0.3),
    ],
  },
  filter: {
    type: 'filter', name: 'Goofy Filter', short: 'FLT', category: 'EQ & Filter', color: '#5b8cff',
    params: [
      { key: 'mode', label: 'Mode', kind: 'choice', default: 'lowpass', options: [
        { value: 'lowpass', label: 'Low Pass' }, { value: 'highpass', label: 'High Pass' }, { value: 'bandpass', label: 'Band Pass' },
      ] },
      n('cutoff', 'Cutoff', 20, 20000, 2000, 'Hz', true),
      n('resonance', 'Resonance', 0.1, 20, 1, 'Q', true),
      n('lfoRate', 'LFO Rate', 0.05, 12, 0.5, 'Hz', true),
      n('lfoDepth', 'LFO Depth', 0, 1, 0),
      n('mix', 'Mix', 0, 1, 1),
    ],
  },
  distortion: {
    type: 'distortion', name: 'Goofy Drive', short: 'DIST', category: 'Character', color: '#ff5c5c',
    params: [
      { key: 'shape', label: 'Shape', kind: 'choice', default: 'soft', options: [
        { value: 'soft', label: 'Soft Clip' }, { value: 'hard', label: 'Hard Clip' }, { value: 'fold', label: 'Wavefold' }, { value: 'bit', label: 'Bitcrush' },
      ] },
      n('drive', 'Drive', 0, 1, 0.4),
      n('tone', 'Tone', 300, 18000, 5000, 'Hz', true),
      n('mix', 'Mix', 0, 1, 1),
      n('output', 'Output', -24, 12, -3, 'dB'),
    ],
  },
  chorus: {
    type: 'chorus', name: 'Goofy Chorus', short: 'CHO', category: 'Modulation', color: '#ff6bb3',
    params: [
      n('rate', 'Rate', 0.05, 8, 0.8, 'Hz', true),
      n('depth', 'Depth', 0, 1, 0.5),
      n('delay', 'Delay', 5, 40, 18, 'ms'),
      n('spread', 'Spread', 0, 1, 0.8),
      n('mix', 'Mix', 0, 1, 0.5),
    ],
  },
  pitch: {
    type: 'pitch', name: 'Goofy Tune', short: 'TUNE', category: 'Vocal', color: '#f28c28',
    params: [
      { key: 'key', label: 'Key', kind: 'choice', default: '0', options: KEYS.map((k, i) => ({ value: String(i), label: k })) },
      { key: 'scale', label: 'Scale', kind: 'choice', default: 'Major', options: [
        'Chromatic', 'Major', 'Minor', 'Harmonic Minor', 'Dorian', 'Mixolydian', 'Pentatonic Major', 'Pentatonic Minor', 'Blues',
      ].map((s) => ({ value: s, label: s })) },
      n('correction', 'Correction', 0, 1, 1),
      n('retune', 'Retune Speed', 0, 400, 20, 'ms'),
      n('humanize', 'Humanize', 0, 1, 0.2),
      n('formant', 'Formant', -12, 12, 0, 'st'),
      n('mix', 'Wet/Dry', 0, 1, 1),
    ],
  },
};

export const EFFECT_TYPES = Object.keys(EFFECTS) as EffectType[];

export function defaultParams(type: EffectType): Record<string, ParamValue> {
  const out: Record<string, ParamValue> = {};
  for (const p of EFFECTS[type].params) out[p.key] = p.default;
  return out;
}

export function num(params: Record<string, ParamValue>, key: string, fallback = 0): number {
  const v = params[key];
  return typeof v === 'number' ? v : typeof v === 'string' ? parseFloat(v) || fallback : fallback;
}

export function str(params: Record<string, ParamValue>, key: string, fallback = ''): string {
  const v = params[key];
  return v === undefined ? fallback : String(v);
}

/** Built-in effect presets shown in the browser. */
export interface EffectPreset {
  name: string;
  type: EffectType;
  params: Record<string, ParamValue>;
}

export const EFFECT_PRESETS: EffectPreset[] = [
  { name: 'Vocal Presence', type: 'eq', params: { ...defaultParams('eq'), low: -6, lowFreq: 110, lowMid: -2, mid: 1, highMid: 3.5, high: 4 } },
  { name: 'Bass Boost', type: 'eq', params: { ...defaultParams('eq'), low: 7, lowFreq: 70, lowMid: -2 } },
  { name: 'Telephone', type: 'eq', params: { ...defaultParams('eq'), low: -18, lowMid: -6, mid: 8, highMid: -6, high: -18 } },
  { name: 'Drum Bus Glue', type: 'compressor', params: { threshold: -20, ratio: 3, attack: 25, release: 120, knee: 8, makeup: 4 } },
  { name: 'Vocal Leveler', type: 'compressor', params: { threshold: -24, ratio: 5, attack: 4, release: 90, knee: 4, makeup: 6 } },
  { name: 'Brickwall-ish', type: 'compressor', params: { threshold: -6, ratio: 20, attack: 0.5, release: 60, knee: 0, makeup: 4 } },
  { name: 'Small Room', type: 'reverb', params: { size: 0.25, decay: 0.8, predelay: 5, damping: 6000, wet: 0.25, dry: 1 } },
  { name: 'Big Hall', type: 'reverb', params: { size: 0.9, decay: 5.5, predelay: 40, damping: 9000, wet: 0.35, dry: 1 } },
  { name: 'Shimmer Wash', type: 'reverb', params: { size: 1, decay: 9, predelay: 80, damping: 16000, wet: 0.6, dry: 0.7 } },
  { name: 'Slapback', type: 'delay', params: { sync: 'off', time: 110, feedback: 0.15, tone: 5000, pingpong: 0, mix: 0.25 } },
  { name: 'Dotted Echo', type: 'delay', params: { sync: '1/8d', time: 375, feedback: 0.45, tone: 6000, pingpong: 0.8, mix: 0.3 } },
  { name: 'Wobble LP', type: 'filter', params: { mode: 'lowpass', cutoff: 900, resonance: 8, lfoRate: 2, lfoDepth: 0.7, mix: 1 } },
  { name: 'Radio HP', type: 'filter', params: { mode: 'highpass', cutoff: 900, resonance: 1.5, lfoRate: 0.5, lfoDepth: 0, mix: 1 } },
  { name: 'Warm Saturation', type: 'distortion', params: { shape: 'soft', drive: 0.25, tone: 9000, mix: 0.7, output: -2 } },
  { name: 'Fuzz Box', type: 'distortion', params: { shape: 'hard', drive: 0.85, tone: 3500, mix: 1, output: -9 } },
  { name: 'Lo-Fi Crush', type: 'distortion', params: { shape: 'bit', drive: 0.6, tone: 6000, mix: 0.8, output: -4 } },
  { name: 'Wide Chorus', type: 'chorus', params: { rate: 0.6, depth: 0.6, delay: 20, spread: 1, mix: 0.5 } },
  { name: 'Hard Tune (T-Pain)', type: 'pitch', params: { key: '0', scale: 'Minor', correction: 1, retune: 0, humanize: 0, formant: 0, mix: 1 } },
  { name: 'Natural Tune', type: 'pitch', params: { key: '0', scale: 'Major', correction: 0.8, retune: 80, humanize: 0.6, formant: 0, mix: 1 } },
  { name: 'Robot Voice', type: 'pitch', params: { key: '0', scale: 'Chromatic', correction: 1, retune: 0, humanize: 0, formant: 0, mix: 1 } },
];
