// Automatic audio stability. Two kinds of trouble make playback crackle or
// stutter, and both are fixed by giving the computer more time:
// 1. the sound card runs out of audio (dropouts, counted by the browser) →
//    a bigger output buffer (balanced → playback → 150 ms → 250 ms)
// 2. the scheduler on the UI thread is late (notes skipped) → a longer
//    scheduling window (200 → 350 → 500 ms)
// Each step is saved as a normal setting, so the user can see and undo it.
import { engine } from '../audio/engine';
import { diagnostics } from '../audio/diagnostics';
import { setUI, toast, ui, type UIState } from '../state/ui';

type Step = { patch: Partial<UIState>; label: string };

/** Next, safer output buffer after the current one (null = already the safest). */
export function nextBuffer(s: Pick<UIState, 'latencyHint' | 'latencyMs'>): Step | null {
  if (s.latencyMs > 0) {
    if (s.latencyMs < 150) return { patch: { latencyMs: 150 }, label: '150 ms' };
    if (s.latencyMs < 250) return { patch: { latencyMs: 250 }, label: '250 ms' };
    return null;
  }
  if (s.latencyHint === 'interactive') return { patch: { latencyHint: 'balanced' }, label: '“Balanced”' };
  if (s.latencyHint === 'balanced') return { patch: { latencyHint: 'playback' }, label: '“Most stable”' };
  return { patch: { latencyMs: 150 }, label: '150 ms' };
}

/** Next, longer scheduling window (null = already the longest). */
export function nextLookahead(current: number): number | null {
  if (current < 0.2) return 0.2;
  if (current < 0.35) return 0.35;
  if (current < 0.5) return 0.5;
  return null;
}

const PAUSE_MS = 30000; // between two automatic steps

/**
 * Crackles that call for a bigger buffer: a burst (3 within 20 s) – or a few
 * now and then (2 within 5 minutes). Sporadic stutters used to be ignored, so
 * a computer that stuttered once a minute was never helped.
 */
export function tooManyDropouts(times: number[], now: number): boolean {
  const burst = times.filter((t) => now - t < 20000).length >= 3;
  const sporadic = times.filter((t) => now - t < 300000).length >= 2;
  return burst || sporadic;
}

/** Same idea for the scheduler: 2 late moments at once, or 3 within 3 minutes. */
export function tooManyLate(times: number[], now: number, fresh: number): boolean {
  return fresh >= 2 || times.filter((t) => now - t < 180000).length >= 3;
}

export function installAudioStability() {
  let dropouts: number[] = [];
  let lastStep = 0;
  let toldMax = false;
  const offEvents = engine.onEvent((e) => {
    if (e.type !== 'glitch') return;
    const now = Date.now();
    for (let i = 0; i < Math.min(e.count, 20); i++) dropouts.push(now);
    dropouts = dropouts.filter((t) => now - t < 300000);
    const s = ui();
    if (!s.autoStabilize || !tooManyDropouts(dropouts, now) || now - lastStep < PAUSE_MS) return;
    // never restart the engine in the middle of a recording – try again afterwards
    if (engine.recording) return;
    const step = nextBuffer(s);
    dropouts = [];
    lastStep = now;
    if (!step) {
      if (!toldMax) {
        toldMax = true;
        toast('The sound card still has dropouts at the biggest buffer. Close other programs, plug in the laptop or try another output in Settings → Audio.', 'error', true);
      }
      return;
    }
    setUI(step.patch);
    const next = { ...s, ...step.patch };
    diagnostics.log('warn', 'stabilize', `Dropouts detected – output buffer raised to ${step.label}`);
    void engine.restart('audio dropouts', { latencyHint: next.latencyHint, latencyMs: next.latencyMs, sampleRate: next.sampleRate }).then(() => {
      toast(`Crackles detected – the audio buffer was made bigger (${step.label}). You can change it in Settings → Performance.`, 'info', true);
    });
  });

  // the scheduler: notes that had to be skipped because the UI was busy
  let seen = diagnostics.counters.lateCallbacks;
  let lastLook = 0;
  let lateTimes: number[] = [];
  const timer = setInterval(() => {
    const late = diagnostics.counters.lateCallbacks;
    const fresh = late - seen;
    seen = late;
    const s = ui();
    const now = Date.now();
    if (fresh > 0) for (let i = 0; i < Math.min(fresh, 20); i++) lateTimes.push(now);
    lateTimes = lateTimes.filter((t) => now - t < 180000);
    if (!fresh || !s.autoStabilize || !engine.transport?.playing || !tooManyLate(lateTimes, now, fresh)) return;
    if (now - lastLook < PAUSE_MS) return;
    lateTimes = [];
    const next = nextLookahead(s.lookahead);
    if (next === null) return;
    lastLook = now;
    setUI({ lookahead: next });
    diagnostics.log('warn', 'stabilize', `Scheduler was late ${fresh}× – scheduling window raised to ${Math.round(next * 1000)} ms`);
    toast(`Playback stuttered because the computer was busy – notes are now prepared ${Math.round(next * 1000)} ms ahead.`, 'info', true);
  }, 3000);

  return () => {
    offEvents();
    clearInterval(timer);
  };
}
