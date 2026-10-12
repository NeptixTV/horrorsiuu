// One-click effect chains ("Sound FX"): voice changers, funny sounds, rooms,
// character effects and autotune flavours. They can be put on a mixer insert
// (e.g. the live microphone) or on a single clip in the playlist.
import type { EffectType, ParamValue } from '../../state/types';

export type FxChainCategory = 'Voice' | 'Funny' | 'Space' | 'Character' | 'Autotune';

export interface FxChainPreset {
  name: string;
  icon: string;
  category: FxChainCategory;
  effects: [EffectType, Record<string, ParamValue>][];
}

export const FX_CHAINS: FxChainPreset[] = [
  // ---- voice changers
  { name: 'Chipmunk', icon: '🐿️', category: 'Voice', effects: [['voice', { transpose: 8, formant: 5 }]] },
  { name: 'Helium', icon: '🎈', category: 'Voice', effects: [['voice', { transpose: 12, formant: 6 }], ['eq', { low: -6, high: 3 }]] },
  { name: 'Kid Voice', icon: '🧒', category: 'Voice', effects: [['voice', { transpose: 4, formant: 4 }]] },
  { name: 'Deep Voice', icon: '🧔', category: 'Voice', effects: [['voice', { transpose: -5, formant: -3 }], ['eq', { low: 4, lowFreq: 120 }]] },
  { name: 'Monster', icon: '👹', category: 'Voice', effects: [['voice', { transpose: -10, formant: -6, tone: 5000 }], ['distortion', { shape: 'soft', drive: 0.35, mix: 0.5, output: -4 }], ['reverb', { size: 0.6, decay: 1.6, wet: 0.2 }]] },
  { name: 'Robot', icon: '🤖', category: 'Voice', effects: [['voice', { transpose: 0, robot: 0.85, robotFreq: 70 }], ['delay', { sync: 'off', time: 25, feedback: 0.3, mix: 0.25, pingpong: 0 }]] },
  { name: 'Alien', icon: '👽', category: 'Voice', effects: [['voice', { transpose: 5, robot: 0.5, robotFreq: 520 }], ['chorus', { rate: 2.5, depth: 0.7, mix: 0.5 }], ['reverb', { size: 0.7, decay: 2.5, wet: 0.25 }]] },
  { name: 'Villain', icon: '🦹', category: 'Voice', effects: [['voice', { transpose: -6, formant: -4 }], ['distortion', { shape: 'soft', drive: 0.25, mix: 0.4 }], ['reverb', { size: 0.9, decay: 3.5, wet: 0.3 }]] },
  { name: 'Ghost', icon: '👻', category: 'Voice', effects: [['voice', { transpose: 3, mix: 0.7 }], ['chorus', { rate: 0.4, depth: 0.9, mix: 0.6 }], ['reverb', { size: 1, decay: 7, predelay: 60, wet: 0.55, dry: 0.6 }]] },
  { name: 'Dalek', icon: '🛸', category: 'Voice', effects: [['voice', { transpose: 0, robot: 1, robotFreq: 30 }], ['distortion', { shape: 'hard', drive: 0.3, mix: 0.4, output: -6 }]] },

  // ---- funny / character sounds
  { name: 'Underwater', icon: '🌊', category: 'Funny', effects: [['filter', { mode: 'lowpass', cutoff: 520, resonance: 5, lfoRate: 0.6, lfoDepth: 0.35, mix: 1 }], ['chorus', { rate: 0.9, depth: 0.8, delay: 25, mix: 0.6 }], ['reverb', { size: 0.6, decay: 2, damping: 1500, wet: 0.35 }]] },
  { name: 'Telephone', icon: '☎️', category: 'Funny', effects: [['eq', { low: -18, lowMid: -6, mid: 8, highMid: -6, high: -18 }], ['distortion', { shape: 'soft', drive: 0.3, mix: 0.5, output: -3 }]] },
  { name: 'Old Radio', icon: '📻', category: 'Funny', effects: [['filter', { mode: 'bandpass', cutoff: 1500, resonance: 1.4 }], ['distortion', { shape: 'soft', drive: 0.4, mix: 0.6, output: -4 }], ['compressor', { threshold: -20, ratio: 6, makeup: 5 }]] },
  { name: 'Megaphone', icon: '📣', category: 'Funny', effects: [['filter', { mode: 'highpass', cutoff: 650, resonance: 1.5 }], ['eq', { mid: 9, midFreq: 1800, high: -10 }], ['distortion', { shape: 'hard', drive: 0.5, mix: 0.7, output: -8 }]] },
  { name: 'Walkie-Talkie', icon: '📟', category: 'Funny', effects: [['filter', { mode: 'bandpass', cutoff: 1800, resonance: 2.5 }], ['distortion', { shape: 'bit', drive: 0.55, mix: 0.6, output: -6 }]] },
  { name: 'Drunk', icon: '🥴', category: 'Funny', effects: [['chorus', { rate: 0.35, depth: 1, delay: 35, mix: 0.75 }], ['voice', { transpose: -1, mix: 0.6 }]] },
  { name: 'Helicopter', icon: '🚁', category: 'Funny', effects: [['tremolo', { shape: 'square', rate: 13, depth: 0.95 }], ['filter', { mode: 'lowpass', cutoff: 2500 }]] },
  { name: 'Tin Can', icon: '🥫', category: 'Funny', effects: [['flanger', { rate: 0.1, depth: 0.2, delay: 1.2, feedback: 0.85, mix: 0.6 }], ['filter', { mode: 'bandpass', cutoff: 2500, resonance: 2 }]] },

  // ---- rooms & space
  { name: 'Cave', icon: '🕳️', category: 'Space', effects: [['reverb', { size: 0.9, decay: 4.5, predelay: 30, damping: 4000, wet: 0.45 }], ['delay', { sync: 'off', time: 260, feedback: 0.35, tone: 3000, mix: 0.2 }]] },
  { name: 'Stadium', icon: '🏟️', category: 'Space', effects: [['reverb', { size: 1, decay: 6, predelay: 70, wet: 0.4 }], ['delay', { sync: '1/4', feedback: 0.25, mix: 0.15 }]] },
  { name: 'Small Room', icon: '🚪', category: 'Space', effects: [['reverb', { size: 0.25, decay: 0.7, predelay: 5, wet: 0.25 }]] },
  { name: 'Echo Echo', icon: '🏔️', category: 'Space', effects: [['delay', { sync: '1/4', feedback: 0.55, tone: 5000, pingpong: 0.7, mix: 0.4 }]] },
  { name: '8D Audio', icon: '🎧', category: 'Space', effects: [['autopan', { rate: 0.12, depth: 1, motion: 0.5 }], ['reverb', { size: 0.7, decay: 2.2, wet: 0.25 }]] },
  { name: 'Spaceship', icon: '🚀', category: 'Space', effects: [['flanger', { rate: 0.12, depth: 0.9, delay: 4, feedback: 0.7, mix: 0.5 }], ['reverb', { size: 0.9, decay: 5, wet: 0.35 }]] },

  // ---- character
  { name: 'Bass Boosted', icon: '🔊', category: 'Character', effects: [['eq', { low: 12, lowFreq: 70, lowMid: 4, lowMidFreq: 160, high: -2 }], ['distortion', { shape: 'soft', drive: 0.35, tone: 9000, mix: 0.6, output: -4 }], ['limiter', { drive: 8, ceiling: -0.5 }]] },
  { name: 'Ultra Bass', icon: '💥', category: 'Character', effects: [['eq', { low: 18, lowFreq: 55, lowMid: 8, lowMidFreq: 120, gain: -3 }], ['distortion', { shape: 'hard', drive: 0.5, mix: 0.5, output: -8 }], ['limiter', { drive: 12, ceiling: -0.3 }]] },
  { name: 'Lo-Fi', icon: '📼', category: 'Character', effects: [['distortion', { shape: 'bit', drive: 0.45, tone: 4000, mix: 0.5, output: -3 }], ['filter', { mode: 'lowpass', cutoff: 3800, resonance: 0.8 }], ['chorus', { rate: 0.3, depth: 0.3, mix: 0.25 }]] },
  { name: 'Vaporwave', icon: '🌴', category: 'Character', effects: [['voice', { transpose: -3, formant: -1 }], ['chorus', { rate: 0.4, depth: 0.5, mix: 0.4 }], ['reverb', { size: 0.85, decay: 4, wet: 0.4 }]] },
  { name: 'Nightcore', icon: '⚡', category: 'Character', effects: [['voice', { transpose: 4, formant: 2 }], ['eq', { high: 3, highMid: 2 }]] },
  { name: 'Phaser Swirl', icon: '🌀', category: 'Character', effects: [['phaser', { rate: 0.3, depth: 0.8, feedback: 0.6, mix: 0.5 }]] },
  { name: 'Loud & Clear', icon: '✨', category: 'Character', effects: [['eq', { low: -3, highMid: 3, high: 4 }], ['compressor', { threshold: -22, ratio: 4, makeup: 6 }], ['limiter', { drive: 4, ceiling: -0.5 }]] },

  // ---- autotune flavours
  { name: 'Hard Autotune', icon: '🎤', category: 'Autotune', effects: [['pitch', { correction: 1, retune: 0, humanize: 0 }]] },
  { name: 'Trap Tune', icon: '💎', category: 'Autotune', effects: [['pitch', { correction: 1, retune: 6, humanize: 0.1, scale: 'Minor' }], ['delay', { sync: '1/8', feedback: 0.3, mix: 0.18 }], ['reverb', { size: 0.5, decay: 1.5, wet: 0.15 }]] },
  { name: 'Natural Tune', icon: '🎙️', category: 'Autotune', effects: [['pitch', { correction: 0.8, retune: 70, humanize: 0.6 }]] },
  { name: 'Subtle Fix', icon: '🩹', category: 'Autotune', effects: [['pitch', { correction: 0.5, retune: 120, humanize: 0.8 }]] },
  { name: 'Robo Tune', icon: '🦾', category: 'Autotune', effects: [['pitch', { correction: 1, retune: 0, humanize: 0, scale: 'Chromatic' }], ['voice', { transpose: 0, robot: 0.3, robotFreq: 110 }]] },
  { name: 'Choir Tune', icon: '⛪', category: 'Autotune', effects: [['pitch', { correction: 1, retune: 20 }], ['chorus', { rate: 0.6, depth: 0.7, mix: 0.5 }], ['reverb', { size: 0.9, decay: 4, wet: 0.35 }]] },
];

export const FX_CATEGORIES: FxChainCategory[] = ['Voice', 'Funny', 'Space', 'Character', 'Autotune'];
