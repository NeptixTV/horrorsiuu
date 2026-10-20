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
import { VoiceEffect } from './voice';
import { AutoPanEffect, FlangerEffect, LimiterEffect, PhaserEffect, TremoloEffect } from './modulation';
import { GateEffect, TapeEffect, WidthEffect } from './studio';

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
    case 'voice': return new VoiceEffect(ctx);
    case 'phaser': return new PhaserEffect(ctx);
    case 'flanger': return new FlangerEffect(ctx);
    case 'tremolo': return new TremoloEffect(ctx);
    case 'autopan': return new AutoPanEffect(ctx);
    case 'limiter': return new LimiterEffect(ctx);
    case 'tape': return new TapeEffect(ctx);
    case 'width': return new WidthEffect(ctx);
    case 'gate': return new GateEffect(ctx);
  }
}

export {
  EqEffect, CompressorEffect, ReverbEffect, DelayEffect, FilterEffect, DistortionEffect, ChorusEffect, PitchEffect, VoiceEffect,
  PhaserEffect, FlangerEffect, TremoloEffect, AutoPanEffect, LimiterEffect, TapeEffect, WidthEffect, GateEffect,
};
