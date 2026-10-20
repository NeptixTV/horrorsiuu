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

/** Oscillator shapes of the DreamChord ("glass", "piano", "organ", "choir" are additive tables). */
export type DreamWave = 'sine' | 'triangle' | 'sawtooth' | 'square' | 'glass' | 'piano' | 'organ' | 'choir';
export type ArpMode = 'off' | 'up' | 'down' | 'updown' | 'random' | 'chord';
export type ArpRate = '1/4' | '1/8' | '1/8t' | '1/16' | '1/16t' | '1/32';
export type JoystickMode = 'chord' | 'bend' | 'filter';

/** DreamChord: chord synthesizer inspired by pocket chord instruments. */
export interface DreamParams {
  preset: string;
  /** chord buttons */
  key: number; // 0..11
  mode: 'major' | 'minor';
  octave: number; // -2..2
  voicing: 'close' | 'open' | 'spread';
  /** sound */
  waveA: DreamWave;
  waveB: DreamWave;
  mixB: number; // 0..1
  detune: number; // cents spread 0..50
  sub: number; // 0..1
  air: number; // breath noise 0..1
  cutoff: number; // Hz
  resonance: number; // 0..12 (dB)
  filterEnv: number; // 0..1
  attack: number; // s
  decay: number; // s
  sustain: number; // 0..1
  release: number; // s
  vibrato: number; // 0..1
  tape: number; // wow/flutter + saturation 0..1
  chorus: number; // 0..1
  delay: number; // 0..1 send
  delayTime: '1/4' | '1/8' | '1/8d' | '1/16';
  reverb: number; // 0..1
  space: number; // reverb size 0..1
  volume: number; // 0..1
  /** performance */
  strum: number; // ms between chord notes, 0 = off
  strumDir: 'up' | 'down';
  arp: ArpMode;
  arpRate: ArpRate;
  arpOctaves: number; // 1..3
  arpGate: number; // 0.1..1
  hold: boolean;
  joystick: JoystickMode;
}

export type ChannelKind = 'synth' | 'drum' | 'sampler' | 'dream';

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
  dream?: DreamParams;
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
  solo?: boolean;
  /** Mixer insert that audio clips on this track are routed to. */
  mixerTrackId: Id;
}

export type ClipKind = 'pattern' | 'audio' | 'automation';

/** What an automation clip controls. Values are stored normalised (0..1). */
export type AutomationTarget =
  | { kind: 'mixer'; id: Id; param: 'volume' | 'pan' }
  | { kind: 'channel'; id: Id; param: 'volume' | 'pan' | 'cutoff' }
  | { kind: 'effect'; trackId: Id; effectId: Id; param: string };

export interface AutomationPoint {
  /** ticks from the start of the clip */
  t: number;
  /** normalised value 0..1 */
  v: number;
}

export interface AutomationData {
  target: AutomationTarget;
  points: AutomationPoint[];
}

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
  /** linear gain (audio) / velocity scale (pattern) */
  gain?: number;
  /** stereo pan of an audio clip, -1..1 */
  pan?: number;
  /** semitones added to every note of a pattern clip */
  transpose?: number;
  /** audio clips: playback speed (changes pitch like a tape), 1 = normal */
  speed?: number;
  /** audio clips: play the source reversed */
  reverse?: boolean;
  /** audio clips: fades in seconds */
  fadeIn?: number;
  fadeOut?: number;
  /** audio clips: effect chain applied to this clip only */
  fx?: EffectInstance[];
  /** optional display name */
  name?: string;
  /** automation clips */
  automation?: AutomationData;
}

export type EffectType =
  | 'eq'
  | 'compressor'
  | 'reverb'
  | 'delay'
  | 'filter'
  | 'distortion'
  | 'chorus'
  | 'pitch'
  | 'voice'
  | 'phaser'
  | 'flanger'
  | 'tremolo'
  | 'autopan'
  | 'limiter';

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
  /** free text: lyrics, ideas, to-dos (shown in the Notes panel and while recording) */
  notes?: string;
  /** time markers in the playlist ruler */
  markers?: Marker[];
  createdAt: number;
  updatedAt: number;
}

export interface Marker {
  id: Id;
  tick: number;
  name: string;
  color: string;
}

export const MASTER_ID = 'master';
