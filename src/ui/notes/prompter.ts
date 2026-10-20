// Pure helpers of the teleprompter: section headings in the lyrics and which
// one belongs to the playlist marker the playhead is in.
import type { Project } from '../../state/types';

export const HEADING = /^\s*(?:\[([^\]]{1,60})\]|#{1,3}\s*(.{1,60}?))\s*$/;

/** Section headings of the text, e.g. "[Chorus]" or "# Verse 2". */
export function sections(text: string): { name: string; line: number }[] {
  const out: { name: string; line: number }[] = [];
  text.split('\n').forEach((l, i) => {
    const m = HEADING.exec(l);
    if (m) out.push({ name: (m[1] ?? m[2]).trim(), line: i });
  });
  return out;
}

/** The marker the playhead is in (latest marker at or before `tick`). */
export function currentMarker(p: Project, tick: number) {
  let best: { name: string; tick: number } | null = null;
  for (const m of p.markers ?? []) if (m.tick <= tick && (!best || m.tick > best.tick)) best = m;
  return best;
}

const norm = (s: string) => s.toLowerCase().replace(/[^a-z0-9äöüß]+/g, '');

/** Index into `secs` of the heading matching a marker name, or -1. */
export function matchSection(secs: { name: string }[], marker: string): number {
  const m = norm(marker);
  if (!m) return -1;
  const exact = secs.findIndex((s) => norm(s.name) === m);
  if (exact >= 0) return exact;
  return secs.findIndex((s) => norm(s.name).startsWith(m) || m.startsWith(norm(s.name)));
}
