// Audio diagnostics: a small in-memory log of everything that can make
// playback unreliable (late scheduler callbacks, scheduling errors, NaN or
// runaway signals caught by the output guard, device changes, engine resets).
// Nothing here leaves the computer; the text report is only built when the
// user presses "copy" and it never contains device names.

export type DiagLevel = 'info' | 'warn' | 'error';

export interface DiagEntry {
  time: number; // Date.now()
  level: DiagLevel;
  code: string;
  text: string;
}

export interface DiagCounters {
  lateCallbacks: number;
  skippedEvents: number;
  scheduleErrors: number;
  nanDetections: number;
  runaways: number;
  limiterHits: number;
  graphResets: number;
  engineRestarts: number;
  deviceChanges: number;
  repairedProjects: number;
  underruns: number;
}

const MAX = 250;

class Diagnostics {
  entries: DiagEntry[] = [];
  counters: DiagCounters = {
    lateCallbacks: 0, skippedEvents: 0, scheduleErrors: 0, nanDetections: 0, runaways: 0, limiterHits: 0,
    graphResets: 0, engineRestarts: 0, deviceChanges: 0, repairedProjects: 0, underruns: 0,
  };
  /** worst scheduler delay seen (ms) */
  worstLateMs = 0;
  private listeners = new Set<() => void>();
  private lastByCode = new Map<string, number>();
  version = 0;

  log(level: DiagLevel, code: string, text: string, throttleMs = 0) {
    const now = Date.now();
    if (throttleMs > 0) {
      const last = this.lastByCode.get(code) ?? 0;
      if (now - last < throttleMs) return;
      this.lastByCode.set(code, now);
    }
    this.entries.push({ time: now, level, code, text });
    if (this.entries.length > MAX) this.entries.splice(0, this.entries.length - MAX);
    if (level === 'error') console.warn(`[GoofyStudio] ${code}: ${text}`);
    this.changed();
  }

  count(key: keyof DiagCounters, n = 1) {
    this.counters[key] += n;
    this.changed();
  }

  late(ms: number) {
    this.counters.lateCallbacks++;
    if (ms > this.worstLateMs) this.worstLateMs = ms;
    this.changed();
  }

  clear() {
    this.entries = [];
    for (const k of Object.keys(this.counters) as (keyof DiagCounters)[]) this.counters[k] = 0;
    this.worstLateMs = 0;
    this.changed();
  }

  private pending = false;
  private changed() {
    this.version++;
    if (this.pending) return;
    this.pending = true;
    // batch notifications (the scheduler can log several things per tick)
    queueMicrotask(() => {
      this.pending = false;
      this.listeners.forEach((l) => l());
    });
  }

  subscribe(l: () => void) {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }

  /** Plain-text report for bug reports (no device labels, no file names). */
  report(extra: Record<string, string | number | boolean>): string {
    const lines = ['Goofy Studio – audio diagnostics', ''];
    for (const [k, v] of Object.entries(extra)) lines.push(`${k}: ${v}`);
    lines.push('', 'Counters:');
    for (const [k, v] of Object.entries(this.counters)) lines.push(`  ${k}: ${v}`);
    lines.push(`  worstLateMs: ${this.worstLateMs.toFixed(0)}`, '', 'Recent events:');
    for (const e of this.entries.slice(-60)) {
      lines.push(`  ${new Date(e.time).toISOString().slice(11, 19)} [${e.level}] ${e.code} – ${e.text}`);
    }
    return lines.join('\n');
  }
}

export const diagnostics = new Diagnostics();
