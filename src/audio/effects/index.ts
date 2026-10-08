import type { EffectType } from '../../state/types';
import type { Effect } from './base';
import { EqEffect } from './eq';
import { CompressorEffect } from './compressor';
import { ReverbEffect } from './reverb';
import { DelayEffect } from './delay';
import { FilterEffect } from './filter';
import { DistortionEffect } from './distortion';
import { ChorusEffect } from './chorus';
import { PitchEffect } from './pitch';

export function createEffect(ctx: BaseAudioContext, type: EffectType): Effect {
  switch (type) {
    case 'eq': return new EqEffect(ctx);
    case 'compressor': return new CompressorEffect(ctx);
    case 'reverb': return new ReverbEffect(ctx);
    case 'delay': return new DelayEffect(ctx);
    case 'filter': return new FilterEffect(ctx);
    case 'distortion': return new DistortionEffect(ctx);
    case 'chorus': return new ChorusEffect(ctx);
    case 'pitch': return new PitchEffect(ctx);
  }
}

export { EqEffect, CompressorEffect, ReverbEffect, DelayEffect, FilterEffect, DistortionEffect, ChorusEffect, PitchEffect };
