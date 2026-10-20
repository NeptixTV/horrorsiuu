import type { ChannelKind } from '../../state/types';
import { Synth } from './synth';
import { DrumVoice } from './drums';
import { Sampler } from './sampler';
import { DreamSynth } from './dream';
import type { Instrument } from './types';

export function createInstrument(ctx: BaseAudioContext, kind: ChannelKind): Instrument {
  switch (kind) {
    case 'synth': return new Synth(ctx);
    case 'drum': return new DrumVoice(ctx);
    case 'sampler': return new Sampler(ctx);
    case 'dream': return new DreamSynth(ctx);
  }
}

export type { Instrument };
export { DreamSynth };
