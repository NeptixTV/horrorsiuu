import type { EffectInstance, ParamValue } from '../../state/types';

export interface EffectEnv {
  bpm: number;
}

/** Base class of all insert effects. Subclasses wire input → … → output. */
export abstract class Effect {
  readonly input: GainNode;
  readonly output: GainNode;
  protected nodes: AudioNode[] = [];
  protected sources: AudioScheduledSourceNode[] = [];

  constructor(protected ctx: BaseAudioContext) {
    this.input = ctx.createGain();
    this.output = ctx.createGain();
  }

  abstract update(params: Record<string, ParamValue>, env: EffectEnv): void;

  /** Called when the tempo changes (tempo synced effects override). */
  setTempo(_bpm: number): void {}

  protected track<T extends AudioNode>(n: T): T {
    this.nodes.push(n);
    return n;
  }

  protected source<T extends AudioScheduledSourceNode>(n: T): T {
    this.sources.push(n);
    this.nodes.push(n);
    n.start();
    return n;
  }

  dispose(): void {
    for (const s of this.sources) {
      try { s.stop(); } catch { /* already stopped */ }
    }
    for (const n of [this.input, this.output, ...this.nodes]) {
      try { n.disconnect(); } catch { /* ignore */ }
    }
  }
}

/** Smoothly sets an AudioParam (avoids zipper noise). */
export function setParam(param: AudioParam, value: number, ctx: BaseAudioContext, time = 0.02) {
  if (!isFinite(value)) return;
  const now = ctx.currentTime;
  if (ctx instanceof OfflineAudioContext && now === 0) {
    param.value = value;
    return;
  }
  param.cancelScheduledValues(now);
  param.setTargetAtTime(value, now, time);
}

/** Equal power dry/wet gains for a 0..1 mix value. */
export function mixGains(mix: number): [number, number] {
  const m = Math.min(1, Math.max(0, mix));
  return [Math.cos((m * Math.PI) / 2), Math.sin((m * Math.PI) / 2)];
}

/** Wraps an effect so it can be bypassed without rebuilding the chain. */
export class EffectSlot {
  readonly input: GainNode;
  readonly output: GainNode;
  private bypassed: boolean | null = null;
  instance: EffectInstance;

  constructor(private ctx: BaseAudioContext, readonly effect: Effect, instance: EffectInstance, env: EffectEnv) {
    this.input = ctx.createGain();
    this.output = ctx.createGain();
    this.instance = instance;
    effect.update(instance.params, env);
    this.setBypass(instance.bypass);
  }

  setBypass(b: boolean) {
    if (this.bypassed === b) return;
    this.bypassed = b;
    try { this.input.disconnect(); } catch { /* ignore */ }
    try { this.effect.output.disconnect(); } catch { /* ignore */ }
    if (b) {
      this.input.connect(this.output);
    } else {
      this.input.connect(this.effect.input);
      this.effect.output.connect(this.output);
    }
    this.onBypass?.(b);
  }

  onBypass?: (b: boolean) => void;

  apply(instance: EffectInstance, env: EffectEnv) {
    if (instance === this.instance) return;
    const prev = this.instance;
    this.instance = instance;
    if (prev.params !== instance.params) this.effect.update(instance.params, env);
    this.setBypass(instance.bypass);
  }

  dispose() {
    this.effect.dispose();
    try { this.input.disconnect(); } catch { /* ignore */ }
    try { this.output.disconnect(); } catch { /* ignore */ }
    void this.ctx;
  }
}
