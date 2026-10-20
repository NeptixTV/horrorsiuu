// The live audio engine singleton: owns the AudioContext, the project graph,
// the transport, the audio input and the output stage.
//
// Output stage: graph → masterOut → guard (NaN scrubber + brickwall limiter +
// runaway protection) → analysers / output volume → speakers.
//
// Reliability: the engine watches the AudioContext (state changes, device
// errors, a stalled clock), output/input device changes (headphones plugged
// in or out) and the guard (broken or runaway signals). Problems are repaired
// in a targeted way – rebuilding the mixer graph, or re-creating the
// AudioContext when the device itself failed – and logged in the diagnostics.
import type { Project } from '../state/types';
import { MASTER_ID } from '../state/types';
import { loadWorklets, markWorkletsReady } from './worklets';
import { MixGraph } from './mixgraph';
import { DEFAULT_LOOKAHEAD, Transport } from './transport';
import { AudioInput } from './input';
import { buildLibrary } from './library';
import { diagnostics } from './diagnostics';

export type LatencyHint = 'interactive' | 'balanced' | 'playback';

export interface EngineConfig {
  latencyHint: LatencyHint;
  /** custom output buffer in ms (0 = use the hint) */
  latencyMs: number;
  /** 0 = device default */
  sampleRate: number;
}

export interface OutputSettings {
  limiter: boolean;
  ceilingDb: number;
  protect: boolean;
  /** listening volume 0..1, applied after the limiter */
  volume: number;
}

export type EngineEvent =
  | { type: 'protect'; reason: 'runaway' | 'nan' }
  | { type: 'device'; text: string; kind: 'info' | 'error' | 'ok' }
  | { type: 'reset'; hard: boolean }
  | { type: 'state'; state: string };

export type StepHook = (from: number, to: number, time: number, spt: number) => void;

export interface RecordingInfo {
  startTick: number;
  trackId: string;
  startTime: number;
}

export interface RecordingResult {
  buffer: AudioBuffer;
  startTick: number;
  trackId: string;
}

export interface GuardStats {
  /** gain reduction of the limiter in dB (0 = untouched) */
  reductionDb: number;
  muted: boolean;
  peak: number;
  time: number;
}

class Engine {
  ctx!: AudioContext;
  graph!: MixGraph;
  transport!: Transport;
  input!: AudioInput;
  /** the mix bus – everything that should be heard connects here */
  masterOut!: GainNode;
  spectrum!: AnalyserNode;
  scope!: AnalyserNode;
  scopeR!: AnalyserNode;
  private guard: AudioNode | null = null;
  private guardWorklet: AudioWorkletNode | null = null;
  private outGain!: GainNode;
  worklets = false;
  initialized = false;
  recording: RecordingInfo | null = null;
  latencyCompensation: number | null = null; // seconds; null = automatic
  config: EngineConfig = { latencyHint: 'interactive', latencyMs: 0, sampleRate: 0 };
  output: OutputSettings = { limiter: true, ceilingDb: -0.3, protect: true, volume: 1 };
  /** transport settings kept across engine restarts */
  lookahead = DEFAULT_LOOKAHEAD;
  metronomeVolume = 0.7;
  countInBars = 0;
  releaseMicAfterRecording = true;
  autoFallbackOutput = true;
  /** device the user picked ('' = system default) and the one in use */
  preferredOutput = '';
  currentSink = '';
  guardStats: GuardStats = { reductionDb: 0, muted: false, peak: 0, time: 0 };
  readonly stepHooks = new Set<StepHook>();
  inputTrackId: string | null = null;
  onRecordingComplete: ((r: RecordingResult) => void) | null = null;

  private getProject: () => Project = () => { throw new Error('engine not attached'); };
  private listeners = new Set<() => void>();
  private eventListeners = new Set<(e: EngineEvent) => void>();
  private initPromise: Promise<void> | null = null;
  private restarting: Promise<void> | null = null;
  private recoveries: number[] = [];
  private lastRecovery = 0;
  private hardFailures: number[] = [];
  private watchdog: ReturnType<typeof setInterval> | null = null;
  private lastClock = -1;
  private stalls = 0;

  init(config: Partial<EngineConfig>, progress: (label: string, p: number) => void): Promise<void> {
    this.initPromise ??= this.doInit(config, progress);
    return this.initPromise;
  }

  private async doInit(config: Partial<EngineConfig>, progress: (label: string, p: number) => void) {
    Object.assign(this.config, config);
    progress('Initializing audio engine…', 0.05);
    progress('Loading AudioWorklet processors…', 0.15);
    await this.createContext();
    progress('Rendering sample library…', 0.25);
    await buildLibrary((d, t) => progress(`Rendering sample library… (${d}/${t})`, 0.25 + 0.45 * (d / t)));
    this.graph = new MixGraph(this.ctx, this.masterOut);
    this.input = new AudioInput(this.ctx);
    this.wireInput();
    this.installDeviceWatch();
    this.startWatchdog();
    this.initialized = true;
  }

  // ---------- context & output stage ----------

  private async createContext() {
    const latencyHint: AudioContextLatencyCategory | number = this.config.latencyMs > 0 ? this.config.latencyMs / 1000 : this.config.latencyHint;
    const opts: AudioContextOptions = { latencyHint };
    if (this.config.sampleRate > 0) opts.sampleRate = this.config.sampleRate;
    let ctx: AudioContext;
    try {
      ctx = new AudioContext(opts);
    } catch (err) {
      // e.g. a sample rate the device does not support – fall back to the defaults
      diagnostics.log('warn', 'ctx-options', `Audio settings rejected (${err instanceof Error ? err.message : String(err)}) – using device defaults`);
      ctx = new AudioContext({ latencyHint: this.config.latencyHint });
    }
    this.ctx = ctx;
    this.masterOut = ctx.createGain();
    this.outGain = ctx.createGain();
    this.outGain.gain.value = this.output.volume;
    this.spectrum = ctx.createAnalyser();
    this.spectrum.fftSize = 4096;
    this.spectrum.smoothingTimeConstant = 0.78;
    this.scope = ctx.createAnalyser();
    this.scope.fftSize = 2048;
    this.scopeR = ctx.createAnalyser();
    this.scopeR.fftSize = 2048;

    this.worklets = await loadWorklets(ctx);
    if (this.worklets) markWorkletsReady(ctx);
    this.guard = this.buildGuard(ctx);
    this.masterOut.connect(this.guard);
    const split = ctx.createChannelSplitter(2);
    this.guard.connect(this.spectrum);
    this.guard.connect(split);
    split.connect(this.scope, 0);
    split.connect(this.scopeR, 1);
    this.guard.connect(this.outGain).connect(ctx.destination);
    this.applyOutput(this.output);

    ctx.onstatechange = () => this.onStateChange(ctx);
    const target = ctx as unknown as EventTarget;
    target.addEventListener('error', () => this.onContextError(ctx));
    target.addEventListener('sinkchange', () => diagnostics.log('info', 'sink', 'Output device changed'));
    this.lastClock = -1;
    this.stalls = 0;
    diagnostics.log('info', 'ctx', `Audio started: ${ctx.sampleRate} Hz, ${Math.round((ctx.baseLatency || 0) * 1000)} ms base latency`);
  }

  private buildGuard(ctx: AudioContext): AudioNode {
    if (this.worklets) {
      try {
        const node = new AudioWorkletNode(ctx, 'goofy-guard', { numberOfInputs: 1, numberOfOutputs: 1, outputChannelCount: [2] });
        node.port.onmessage = (e) => this.onGuardMessage(e.data);
        this.guardWorklet = node;
        return node;
      } catch (err) {
        diagnostics.log('warn', 'guard', `Output guard unavailable: ${err instanceof Error ? err.message : String(err)}`);
      }
    }
    // fallback without AudioWorklet: a fast compressor as limiter
    this.guardWorklet = null;
    const comp = ctx.createDynamicsCompressor();
    comp.threshold.value = -1;
    comp.knee.value = 0;
    comp.ratio.value = 20;
    comp.attack.value = 0.001;
    comp.release.value = 0.1;
    return comp;
  }

  /** Limiter, ceiling, runaway protection and listening volume. */
  applyOutput(settings: Partial<OutputSettings>) {
    Object.assign(this.output, settings);
    const o = this.output;
    if (!this.ctx) return;
    this.guardWorklet?.port.postMessage({ type: 'config', enabled: o.limiter, ceiling: Math.pow(10, Math.min(0, o.ceilingDb) / 20), protect: o.protect });
    if (this.outGain) this.outGain.gain.setTargetAtTime(Math.min(1, Math.max(0, o.volume)), this.ctx.currentTime, 0.02);
  }

  private onGuardMessage(m: { type: string; nan?: number; minGain?: number; peak?: number; muted?: boolean }) {
    if (m.type === 'stats') {
      const minGain = m.minGain ?? 1;
      this.guardStats = { reductionDb: minGain < 1 ? 20 * Math.log10(Math.max(1e-6, minGain)) : 0, muted: !!m.muted, peak: m.peak ?? 0, time: performance.now() };
      if (minGain < 0.89) {
        diagnostics.count('limiterHits');
        diagnostics.log('info', 'limiter', `Output limiter caught a peak (${(20 * Math.log10(Math.max(1e-6, minGain))).toFixed(1)} dB)`, 15000);
      }
      if ((m.nan ?? 0) > 0) {
        diagnostics.count('nanDetections');
        diagnostics.log('error', 'nan', 'A broken signal (NaN) was blocked at the output – rebuilding the mixer', 3000);
        this.emitEvent({ type: 'protect', reason: 'nan' });
        this.recover('broken signal');
      }
    } else if (m.type === 'runaway') {
      diagnostics.count('runaways');
      diagnostics.log('error', 'runaway', 'Extremely loud signal (feedback?) – output muted to protect your ears, effects reset');
      this.emitEvent({ type: 'protect', reason: 'runaway' });
      this.recover('runaway signal');
    }
  }

  /** Targeted repair: rebuild the graph; if that keeps happening, re-create the context. */
  private recoverTimer: ReturnType<typeof setTimeout> | null = null;

  private recover(reason: string) {
    const now = performance.now();
    if (now - this.lastRecovery < 2000) {
      // too soon after the last repair: queue one (the guard stays muted meanwhile)
      if (!this.recoverTimer) {
        this.recoverTimer = setTimeout(() => {
          this.recoverTimer = null;
          this.recover(reason);
        }, 2000 - (now - this.lastRecovery) + 50);
      }
      return;
    }
    this.lastRecovery = now;
    this.recoveries = this.recoveries.filter((t) => now - t < 60000);
    this.recoveries.push(now);
    if (this.recoveries.length >= 3) {
      this.recoveries = [];
      this.hardRestartThrottled(`${reason} (repeated)`);
      setTimeout(() => this.guardWorklet?.port.postMessage({ type: 'unmute' }), 600);
      return;
    }
    this.resetGraph(reason);
    setTimeout(() => this.guardWorklet?.port.postMessage({ type: 'unmute' }), 300);
  }

  private onStateChange(ctx: AudioContext) {
    if (ctx !== this.ctx) return;
    const st = ctx.state as string;
    diagnostics.log('info', 'ctx-state', `Audio context ${st}`);
    this.emitEvent({ type: 'state', state: st });
    if (st === 'suspended' || st === 'interrupted') {
      // the OS took the device away for a moment (sleep, another app, device switch)
      setTimeout(() => void this.resume(), 400);
    }
    this.emit();
  }

  private onContextError(ctx: AudioContext) {
    if (ctx !== this.ctx) return;
    diagnostics.log('error', 'ctx-error', 'The audio device reported an error – restarting the audio engine');
    this.hardRestartThrottled('device error');
  }

  private hardRestartThrottled(reason: string) {
    const now = performance.now();
    this.hardFailures = this.hardFailures.filter((t) => now - t < 60000);
    if (this.hardFailures.length >= 3) {
      diagnostics.log('error', 'restart', 'Audio device keeps failing – not restarting again automatically. Check the device or pick another output in Settings → Audio.', 30000);
      this.emitEvent({ type: 'device', kind: 'error', text: 'The audio device keeps failing. Pick another output in Settings → Audio.' });
      return;
    }
    this.hardFailures.push(now);
    void this.restart(reason);
  }

  /** Detects a device that stopped pulling audio (clock frozen while "running"). */
  private startWatchdog() {
    if (this.watchdog) clearInterval(this.watchdog);
    this.watchdog = setInterval(() => {
      const ctx = this.ctx;
      if (!ctx || this.restarting || ctx.state !== 'running') {
        this.stalls = 0;
        this.lastClock = -1;
        return;
      }
      const t = ctx.currentTime;
      if (t === this.lastClock) {
        this.stalls++;
        if (this.stalls >= 2) {
          this.stalls = 0;
          diagnostics.log('error', 'stall', 'Audio clock stopped (device stopped responding) – restarting the audio engine');
          this.hardRestartThrottled('device stalled');
        }
      } else {
        this.stalls = 0;
      }
      this.lastClock = t;
    }, 2000);
  }

  private wireInput() {
    this.input.onLost = () => {
      if (this.recording) void this.stopRecording();
      this.emitEvent({ type: 'device', kind: 'info', text: 'Microphone disconnected.' });
      this.emit();
    };
  }

  // ---------- devices ----------

  private installDeviceWatch() {
    const md = navigator.mediaDevices;
    if (!md?.addEventListener) return;
    md.addEventListener('devicechange', () => void this.onDeviceChange());
  }

  private async onDeviceChange() {
    diagnostics.count('deviceChanges');
    let outputs: { deviceId: string }[] = [];
    try {
      outputs = (await AudioInput.listDevices()).outputs;
    } catch {
      return;
    }
    diagnostics.log('info', 'devices', `Audio devices changed (${outputs.length} outputs)`);
    const want = this.preferredOutput;
    if (!want || want === 'default') return;
    const present = outputs.some((d) => d.deviceId === want);
    if (!present && this.currentSink === want && this.autoFallbackOutput) {
      const ok = await this.applySink('');
      this.emitEvent({ type: 'device', kind: 'info', text: ok ? 'Your output device was disconnected – playing through the system default.' : 'Your output device was disconnected.' });
    } else if (present && this.currentSink !== want) {
      if (await this.applySink(want)) this.emitEvent({ type: 'device', kind: 'ok', text: 'Output device reconnected.' });
    }
  }

  private async applySink(deviceId: string): Promise<boolean> {
    const c = this.ctx as AudioContext & { setSinkId?: (id: string) => Promise<void> };
    if (!c?.setSinkId) return false;
    try {
      await c.setSinkId(deviceId);
      this.currentSink = deviceId;
      return true;
    } catch (err) {
      diagnostics.log('warn', 'sink', `Could not switch the output device (${err instanceof Error ? err.name : 'error'})`);
      return false;
    }
  }

  /** Picks the output device ('' = system default). Remembered across restarts. */
  async setOutputDevice(deviceId: string): Promise<boolean> {
    this.preferredOutput = deviceId;
    const ok = await this.applySink(deviceId);
    if (!ok && deviceId) {
      // the device is not there (unplugged since last time) – keep playing on the default one
      await this.applySink('');
    }
    return ok;
  }

  get supportsOutputSelection() {
    return typeof (this.ctx as AudioContext & { setSinkId?: unknown })?.setSinkId === 'function';
  }

  // ---------- graph / transport ----------

  attach(getProject: () => Project) {
    this.getProject = getProject;
    this.makeTransport();
    this.graph.sync(getProject());
  }

  private makeTransport() {
    this.transport = new Transport(this.ctx, this.graph, this.getProject);
    this.transport.stepHooks = this.stepHooks;
    this.transport.lookahead = this.lookahead;
    this.transport.metronomeVolume = this.metronomeVolume;
    this.transport.onChange(() => this.emit());
  }

  setTransportOptions(o: { lookahead?: number; metronomeVolume?: number }) {
    if (o.lookahead !== undefined) this.lookahead = Math.min(0.5, Math.max(0.05, o.lookahead));
    if (o.metronomeVolume !== undefined) this.metronomeVolume = Math.min(1, Math.max(0, o.metronomeVolume));
    if (this.transport) {
      this.transport.lookahead = this.lookahead;
      this.transport.metronomeVolume = this.metronomeVolume;
    }
  }

  sync(project: Project) {
    if (!this.initialized) return;
    this.graph.sync(project);
    if (this.input) this.input.setMonitorDestination(this.graph.stripInput(this.monitorTarget(project)));
  }

  monitorTarget(project: Project) {
    const vocals = project.mixer.find((m) => m.name.toLowerCase().includes('vocal'));
    return this.inputTrackId && project.mixer.some((m) => m.id === this.inputTrackId) ? this.inputTrackId : vocals?.id ?? MASTER_ID;
  }

  /**
   * Rebuilds the whole mixer / instrument graph on the same AudioContext.
   * Clears stuck filter states and feedback tails; playback continues.
   */
  resetGraph(reason: string) {
    if (!this.initialized || !this.transport) return;
    const project = this.getProject();
    const old = this.graph;
    const next = new MixGraph(this.ctx, this.masterOut);
    next.sync(project);
    this.graph = next;
    this.transport.setGraph(next);
    try { old.dispose(); } catch { /* ignore */ }
    this.input?.setMonitorDestination(next.stripInput(this.monitorTarget(project)));
    diagnostics.count('graphResets');
    diagnostics.log('warn', 'graph-reset', `Mixer rebuilt (${reason})`);
    this.emitEvent({ type: 'reset', hard: false });
    this.emit();
  }

  /**
   * Safe full audio reset: closes the AudioContext and builds everything
   * again (optionally with new latency / sample-rate settings). The project,
   * transport position and the open microphone are restored.
   */
  restart(reason: string, config?: Partial<EngineConfig>): Promise<void> {
    if (this.restarting) return this.restarting;
    if (!this.initialized) return Promise.resolve();
    this.restarting = (async () => {
      const t = this.transport;
      const state = t ? { mode: t.mode, patternId: t.patternId, metronome: t.metronome, playing: t.playing, tick: t.getTick() } : null;
      if (this.recording) await this.stopRecording().catch(() => undefined);
      const inp = this.input;
      const inputState = inp ? { open: inp.isOpen, device: inp.deviceId, pinned: inp.pinned, monitoring: inp.monitoring, gain: inp.gainValue } : null;
      try { t?.dispose(); } catch { /* ignore */ }
      try { inp?.dispose(); } catch { /* ignore */ }
      try { this.graph?.dispose(); } catch { /* ignore */ }
      const old = this.ctx;
      if (config) Object.assign(this.config, config);
      try { await old.close(); } catch { /* already closed */ }
      await this.createContext();
      this.graph = new MixGraph(this.ctx, this.masterOut);
      this.input = new AudioInput(this.ctx);
    this.wireInput();
      this.makeTransport();
      if (state) {
        this.transport.mode = state.mode;
        this.transport.patternId = state.patternId;
        this.transport.metronome = state.metronome;
        this.transport.setPosition(state.tick);
      }
      this.sync(this.getProject());
      if (this.preferredOutput) await this.setOutputDevice(this.preferredOutput);
      if (inputState?.open) {
        if (await this.input.open(inputState.device)) {
          this.input.pinned = inputState.pinned;
          this.input.setGain(inputState.gain);
          this.input.setMonitoring(inputState.monitoring);
          this.sync(this.getProject());
        }
      }
      try { await this.ctx.resume(); } catch { /* needs a gesture */ }
      if (state?.playing) this.transport.play(state.tick);
      diagnostics.count('engineRestarts');
      diagnostics.log('warn', 'restart', `Audio engine restarted (${reason})`);
      this.emitEvent({ type: 'reset', hard: true });
      this.emit();
    })().finally(() => {
      this.restarting = null;
    });
    return this.restarting;
  }

  /** Stops every sound right now and clears effect tails. */
  panic() {
    if (!this.initialized) return;
    if (this.recording) void this.stopRecording();
    this.transport?.stop();
    this.graph.allNotesOff(this.ctx.currentTime);
    this.resetGraph('panic');
  }

  /** Short sine beep on the left, right or both channels (headphone check). */
  playTestTone(side: 'left' | 'right' | 'both' = 'both') {
    if (!this.initialized) return;
    void this.resume();
    const ctx = this.ctx;
    const t = ctx.currentTime + 0.03;
    const osc = ctx.createOscillator();
    osc.frequency.value = side === 'right' ? 660 : 440;
    const g = ctx.createGain();
    g.gain.setValueAtTime(0, t);
    g.gain.linearRampToValueAtTime(0.18, t + 0.03);
    g.gain.setValueAtTime(0.18, t + 0.6);
    g.gain.linearRampToValueAtTime(0, t + 0.7);
    const pan = ctx.createStereoPanner();
    pan.pan.value = side === 'left' ? -1 : side === 'right' ? 1 : 0;
    osc.connect(g).connect(pan).connect(this.masterOut);
    osc.start(t);
    osc.stop(t + 0.75);
    osc.onended = () => { try { pan.disconnect(); } catch { /* ignore */ } };
  }

  onChange(l: () => void): () => void {
    this.listeners.add(l);
    return () => { this.listeners.delete(l); };
  }

  emit() {
    this.listeners.forEach((l) => l());
  }

  onEvent(l: (e: EngineEvent) => void): () => void {
    this.eventListeners.add(l);
    return () => { this.eventListeners.delete(l); };
  }

  private emitEvent(e: EngineEvent) {
    this.eventListeners.forEach((l) => {
      try { l(e); } catch { /* ignore */ }
    });
  }

  async resume() {
    if (this.ctx && this.ctx.state !== 'running' && this.ctx.state !== 'closed') {
      try { await this.ctx.resume(); } catch { /* needs gesture */ }
    }
  }

  get running() {
    return this.ctx?.state === 'running';
  }

  previewNote(channelId: string, pitch: number, velocity = 0.8, duration?: number, at?: number) {
    if (!this.initialized) return;
    void this.resume();
    try {
      this.graph.noteOn(channelId, pitch, velocity, at ?? this.ctx.currentTime + 0.005, duration);
    } catch (err) {
      diagnostics.log('error', 'preview', err instanceof Error ? err.message : String(err), 3000);
    }
  }

  previewNoteOff(channelId: string, pitch: number, at?: number) {
    if (!this.initialized) return;
    try {
      this.graph.noteOff(channelId, pitch, at ?? this.ctx.currentTime);
    } catch { /* ignore */ }
  }

  /** Estimated round-trip latency compensation in seconds. */
  get roundTripLatency(): number {
    if (this.latencyCompensation !== null) return this.latencyCompensation;
    const c = this.ctx as AudioContext & { outputLatency?: number };
    return (c.baseLatency || 0) + (c.outputLatency || 0) + 0.01;
  }

  // ---------- recording ----------

  async startRecording(trackId: string): Promise<string | null> {
    if (this.recording) return null;
    await this.resume();
    if (!this.worklets) return 'AudioWorklet is not supported in this browser – recording is unavailable.';
    if (!this.input.isOpen) {
      const ok = await this.input.open();
      if (!ok) return this.input.error ?? 'Could not open the audio input.';
      // opened just for this take: released again afterwards (Bluetooth headsets
      // otherwise stay in low-quality "hands-free" mode for the rest of the session)
      this.input.pinned = false;
    }
    this.sync(this.getProject());
    let startTime: number;
    if (!this.transport.playing) {
      if (this.transport.mode !== 'song') this.transport.setMode('song');
      const countIn = this.scheduleCountIn();
      startTime = this.transport.play(undefined, countIn);
    } else {
      startTime = this.ctx.currentTime + 0.03;
    }
    const startTick = this.transport.tickAt(startTime);
    this.input.startCapture(startTime);
    this.recording = { startTick, trackId, startTime };
    this.emit();
    return null;
  }

  /** Metronome clicks before a recording; returns the start time (or undefined). */
  private scheduleCountIn(): number | undefined {
    const bars = this.countInBars;
    if (!(bars > 0)) return undefined;
    const p = this.getProject();
    const beat = (60 / p.bpm) * (4 / p.timeSig[1]);
    const beats = Math.round(bars * p.timeSig[0]);
    const t0 = this.ctx.currentTime + 0.1;
    for (let b = 0; b < beats; b++) {
      const time = t0 + b * beat;
      const osc = this.ctx.createOscillator();
      osc.frequency.value = b % p.timeSig[0] === 0 ? 1760 : 1175;
      const g = this.ctx.createGain();
      g.gain.setValueAtTime(0, time);
      g.gain.linearRampToValueAtTime(0.45 * Math.max(0.2, this.metronomeVolume), time + 0.001);
      g.gain.exponentialRampToValueAtTime(0.0001, time + 0.06);
      osc.connect(g).connect(this.masterOut);
      osc.start(time);
      osc.stop(time + 0.07);
    }
    return t0 + beats * beat;
  }

  async stopRecording(): Promise<void> {
    const rec = this.recording;
    if (!rec) return;
    this.recording = null;
    const buffer = await this.input.stopCapture(this.roundTripLatency);
    if (!this.input.pinned && !this.input.monitoring && this.releaseMicAfterRecording) this.input.close();
    this.emit();
    if (buffer && this.onRecordingComplete) this.onRecordingComplete({ buffer, startTick: rec.startTick, trackId: rec.trackId });
  }

  get bufferSize(): number {
    return Math.round((this.ctx?.baseLatency || 128 / 44100) * (this.ctx?.sampleRate || 44100));
  }

  /** Short status for the settings / diagnostics page. */
  status() {
    const c = this.ctx as (AudioContext & { outputLatency?: number }) | undefined;
    return {
      state: c?.state ?? 'none',
      sampleRate: c?.sampleRate ?? 0,
      baseLatencyMs: (c?.baseLatency ?? 0) * 1000,
      outputLatencyMs: (c?.outputLatency ?? 0) * 1000,
      voices: this.initialized ? this.graph.voiceCount() : 0,
      strips: this.initialized ? this.graph.strips.size : 0,
      channels: this.initialized ? this.graph.channels.size : 0,
      worklets: this.worklets,
      inputOpen: !!this.input?.isOpen,
      guard: this.guardWorklet ? 'limiter' : 'compressor',
    };
  }
}

export const engine = new Engine();
