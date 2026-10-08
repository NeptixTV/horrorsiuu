// Core project data model. Everything in here is plain, serialisable data so
// that it can be stored in IndexedDB, exported as a file and snapshotted for
// undo/redo. Audio sample data lives separately in the asset store.

/** Pulses (ticks) per quarter note. One 16th step = PPQ / 4 ticks. */
export const PPQ = 96;
export const TICKS_PER_STEP = PPQ / 4;

export type Id = string;

export type Waveform = 'sine' | 'sawtooth' | 'square' | 'triangle';
export type FilterType = 'lowpass' | 'highpass' | 'bandpass';
export type LfoTarget = 'pitch' | 'filter' | 'amp';

export interface OscParams {
  wave: Waveform;
  octave: number; // -3..3
  semi: number; // -12..12
  detune: number; // cents -100..100
  level: number; // 0..1
}

export interface SynthParams {
  osc1: OscParams;
  osc2: OscParams;
  unisonDetune: number; // cents of spread for a third (sub) voice, 0 = off
  filterType: FilterType;
  cutoff: number; // Hz
  resonance: number; // Q 0.1..20
  filterEnv: number; // 0..1 envelope amount
  attack: number; // s
  decay: number; // s
  sustain: number; // 0..1
  release: number; // s
  lfoWave: Waveform;
  lfoRate: number; // Hz
  lfoDepth: number; // 0..1
  lfoTarget: LfoTarget;
  glide: number; // s
  volume: number; // 0..1
}

export type DrumType = 'kick' | 'snare' | 'clap' | 'hihat' | 'openhat' | 'tom' | 'perc' | 'rim' | 'cowbell' | 'shaker' | 'crash';

export interface DrumParams {
  type: DrumType;
  tune: number; // semitones -24..24
  decay: number; // 0.1..3 multiplier
  tone: number; // 0..1
  snap: number; // 0..1 (noise / click amount)
  level: number; // 0..1
}

export interface SamplerParams {
  assetId: Id | null;
  rootNote: number;
  tune: number; // semitones
  attack: number;
  release: number;
  reverse: boolean;
  level: number;
}

export type ChannelKind = 'synth' | 'drum' | 'sampler';

export interface Channel {
  id: Id;
  name: string;
  color: string;
  kind: ChannelKind;
  volume: number; // 0..1.25
  pan: number; // -1..1
  mute: boolean;
  solo: boolean;
  mixerTrackId: Id;
  rootNote: number; // pitch used by step sequencer
  synth?: SynthParams;
  drum?: DrumParams;
  sampler?: SamplerParams;
}

export interface Note {
  id: Id;
  pitch: number; // MIDI 0..127
  start: number; // ticks from pattern start
  length: number; // ticks
  velocity: number; // 0..1
}

export interface Pattern {
  id: Id;
  name: string;
  color: string;
  /** Length of the step grid in 16th steps. */
  lengthSteps: number;
  /** Per channel step velocities (0 = off). */
  steps: Record<Id, number[]>;
  /** Per channel piano roll notes. */
  notes: Record<Id, Note[]>;
}

export interface PlaylistTrack {
  id: Id;
  name: string;
  color: string;
  mute: boolean;
  /** Mixer insert that audio clips on this track are routed to. */
  mixerTrackId: Id;
}

export type ClipKind = 'pattern' | 'audio';

export interface Clip {
  id: Id;
  trackId: Id;
  kind: ClipKind;
  start: number; // ticks
  length: number; // ticks
  /** Offset into the source: ticks for pattern clips, seconds for audio clips. */
  offset: number;
  patternId?: Id;
  assetId?: Id;
  gain?: number;
}

export type EffectType =
  | 'eq'
  | 'compressor'
  | 'reverb'
  | 'delay'
  | 'filter'
  | 'distortion'
  | 'chorus'
  | 'pitch';

export type ParamValue = number | string | boolean;

export interface EffectInstance {
  id: Id;
  type: EffectType;
  bypass: boolean;
  params: Record<string, ParamValue>;
}

export interface Send {
  id: Id;
  target: Id;
  amount: number; // 0..1
}

export interface MixerTrack {
  id: Id;
  name: string;
  color: string;
  volume: number; // linear 0..1.25
  pan: number;
  mute: boolean;
  solo: boolean;
  inserts: EffectInstance[];
  sends: Send[];
  /** Output target, null for the master track itself. */
  output: Id | null;
}

export interface AssetMeta {
  id: Id;
  name: string;
  duration: number;
  sampleRate: number;
  channels: number;
  builtin?: boolean;
}

export interface LoopRegion {
  enabled: boolean;
  start: number; // ticks
  end: number; // ticks
}

export interface Project {
  version: 1;
  id: Id;
  name: string;
  bpm: number;
  timeSig: [number, number];
  swing: number; // 0..1
  key: number; // 0..11 root for scale helpers
  scale: string;
  channels: Channel[];
  patterns: Pattern[];
  tracks: PlaylistTrack[];
  clips: Clip[];
  loop: LoopRegion;
  /** Index 0 is always the master track. */
  mixer: MixerTrack[];
  assets: Record<Id, AssetMeta>;
  createdAt: number;
  updatedAt: number;
}

export const MASTER_ID = 'master';
