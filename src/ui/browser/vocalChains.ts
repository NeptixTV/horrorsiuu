import type { EffectType, ParamValue } from '../../state/types';
import { getProject, update } from '../../state/store';
import { makeEffect, makeMixerTrack } from '../../state/defaults';
import { setUI, toast } from '../../state/ui';
import { engine } from '../../audio/engine';

export interface VocalChain {
  name: string;
  effects: [EffectType, Record<string, ParamValue>][];
}

export const VOCAL_CHAINS: VocalChain[] = [
  {
    name: 'Hard Tune Rap', effects: [
      ['pitch', { scale: 'Minor', retune: 0, humanize: 0, correction: 1 }],
      ['eq', { low: -10, lowFreq: 110, highMid: 3, high: 4 }],
      ['compressor', { threshold: -24, ratio: 6, attack: 3, release: 80, makeup: 7 }],
      ['delay', { sync: '1/8', feedback: 0.25, mix: 0.18 }],
    ],
  },
  {
    name: 'Natural Pop Vocal', effects: [
      ['pitch', { scale: 'Major', retune: 70, humanize: 0.6, correction: 0.85 }],
      ['eq', { low: -8, lowFreq: 90, lowMid: -2, highMid: 2, high: 3.5 }],
      ['compressor', { threshold: -20, ratio: 3.5, attack: 8, release: 140, makeup: 4 }],
      ['reverb', { size: 0.5, decay: 1.8, wet: 0.18 }],
    ],
  },
  {
    name: 'Lo-Fi Radio Voice', effects: [
      ['filter', { mode: 'bandpass', cutoff: 1400, resonance: 1.2 }],
      ['distortion', { shape: 'soft', drive: 0.45, mix: 0.6, output: -4 }],
      ['compressor', { threshold: -18, ratio: 8, makeup: 6 }],
    ],
  },
  {
    name: 'Robot Monster', effects: [
      ['pitch', { scale: 'Chromatic', retune: 0, humanize: 0, correction: 1, formant: -8 }],
      ['chorus', { rate: 3, depth: 0.8, mix: 0.5 }],
      ['distortion', { shape: 'fold', drive: 0.3, mix: 0.4, output: -6 }],
    ],
  },
];

/** Replaces the inserts of the vocal mixer track (creating one if needed) with a chain. */
export function applyVocalChain(name: string) {
  const chain = VOCAL_CHAINS.find((c) => c.name === name);
  if (!chain) return;
  const p = getProject();
  let target = p.mixer.find((m) => m.name.toLowerCase().includes('vocal'));
  const newTrack = target ? null : { ...makeMixerTrack(p.mixer.length, 'Vocals'), color: '#f28c28' };
  update(`Vocal chain: ${name}`, (d) => {
    if (newTrack) d.mixer.push(newTrack);
    const t = d.mixer.find((m) => m.id === (target?.id ?? newTrack!.id))!;
    t.inserts = chain.effects.map(([type, params]) => makeEffect(type, { ...params, key: String(d.key ?? 0) }));
  });
  target = getProject().mixer.find((m) => m.id === (target?.id ?? newTrack!.id));
  if (target) {
    engine.inputTrackId = target.id;
    setUI({ selectedMixerId: target.id });
  }
  toast(`Vocal chain “${name}” loaded on ${target?.name}`, 'ok');
}
