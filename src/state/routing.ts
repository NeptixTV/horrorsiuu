import type { MixerTrack } from './types';
import { MASTER_ID } from './types';

function edges(t: MixerTrack): string[] {
  const out: string[] = [];
  if (t.output) out.push(t.output);
  for (const s of t.sends) out.push(s.target);
  return out;
}

/** True if routing `from` into `to` (as output or send) would create a feedback loop. */
export function wouldCycle(mixer: MixerTrack[], from: string, to: string): boolean {
  if (from === to) return true;
  const byId = new Map(mixer.map((t) => [t.id, t]));
  const stack = [to];
  const seen = new Set<string>();
  while (stack.length) {
    const id = stack.pop()!;
    if (id === from) return true;
    if (seen.has(id)) continue;
    seen.add(id);
    const t = byId.get(id);
    if (t) stack.push(...edges(t));
  }
  return false;
}

/** Tracks that should be audible given the current solo state (mute excluded). */
export function audibleTracks(mixer: MixerTrack[]): Set<string> {
  const all = new Set(mixer.map((t) => t.id));
  const soloed = mixer.filter((t) => t.solo && t.id !== MASTER_ID).map((t) => t.id);
  if (soloed.length === 0) return all;
  const byId = new Map(mixer.map((t) => [t.id, t]));
  const audible = new Set<string>([MASTER_ID]);
  // downstream of soloed tracks
  const down = [...soloed];
  while (down.length) {
    const id = down.pop()!;
    if (audible.has(id) && id !== MASTER_ID && !soloed.includes(id)) continue;
    audible.add(id);
    const t = byId.get(id);
    if (t) for (const e of edges(t)) if (!audible.has(e)) down.push(e);
  }
  // upstream: tracks that feed a soloed track
  const reaches = (id: string, seen = new Set<string>()): boolean => {
    if (soloed.includes(id)) return true;
    if (seen.has(id)) return false;
    seen.add(id);
    const t = byId.get(id);
    return t ? edges(t).some((e) => e !== MASTER_ID && reaches(e, seen)) : false;
  };
  for (const t of mixer) if (reaches(t.id)) audible.add(t.id);
  return audible;
}
