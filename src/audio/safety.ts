// Guards for values that end up in AudioParams. A single NaN or Infinity
// written to an AudioParam throws ("non-finite") and, inside the scheduler,
// used to stall playback for good; inside a filter it can poison the state of
// the node until it is rebuilt. Everything numeric that comes from the project
// passes through these helpers before it reaches the audio graph.

/** `v` if it is a finite number, otherwise `fallback`. */
export function finite(v: unknown, fallback: number): number {
  return typeof v === 'number' && Number.isFinite(v) ? v : fallback;
}

/** Finite and clamped to [min, max]. */
export function clampFinite(v: unknown, min: number, max: number, fallback: number): number {
  const x = finite(v, fallback);
  return x < min ? min : x > max ? max : x;
}

/** Sets `param.value` only for finite values (never throws). */
export function setValue(param: AudioParam, v: unknown, fallback: number) {
  try {
    param.value = finite(v, fallback);
  } catch {
    /* value outside the nominal range of the param – keep the old one */
  }
}

/** Highest delay-line feedback that is still guaranteed to die away. */
export const MAX_FEEDBACK = 0.95;
