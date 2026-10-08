import { engine } from '../../audio/engine';
import { getProject } from '../../state/store';
import { TICKS_PER_STEP } from '../../state/types';
import { patternLength } from '../../audio/sequencer';

/**
 * Index of the step currently playing in `patternId`, or -1. In song mode
 * the step is taken from the pattern clip under the playhead.
 */
export function currentStepFor(patternId: string | null): number {
  const t = engine.transport;
  if (!t || !t.playing || !patternId) return -1;
  const project = getProject();
  const pattern = project.patterns.find((p) => p.id === patternId);
  if (!pattern) return -1;
  const tick = t.getTick();
  const len = patternLength(pattern, project.timeSig);
  let local: number;
  if (t.mode === 'pattern') {
    if (t.patternId !== patternId) return -1;
    local = tick % len;
  } else {
    const clip = project.clips.find((c) => c.kind === 'pattern' && c.patternId === patternId && tick >= c.start && tick < c.start + c.length);
    if (!clip) return -1;
    local = (((tick - clip.start + clip.offset) % len) + len) % len;
  }
  const step = Math.floor(local / TICKS_PER_STEP);
  return step < pattern.lengthSteps ? step : -1;
}

/** Local tick position inside `patternId` (for the piano roll cursor) or -1. */
export function currentPatternTick(patternId: string | null): number {
  const t = engine.transport;
  if (!t || !t.playing || !patternId) return -1;
  const project = getProject();
  const pattern = project.patterns.find((p) => p.id === patternId);
  if (!pattern) return -1;
  const tick = t.getTick();
  const len = patternLength(pattern, project.timeSig);
  if (t.mode === 'pattern') return t.patternId === patternId ? tick % len : -1;
  const clip = project.clips.find((c) => c.kind === 'pattern' && c.patternId === patternId && tick >= c.start && tick < c.start + c.length);
  if (!clip) return -1;
  return (((tick - clip.start + clip.offset) % len) + len) % len;
}
