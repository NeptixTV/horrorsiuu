// Live automation: the transport calls this for every 16th it schedules; when
// playback stops, automated parameters go back to their values in the project.
import type { AutomationTarget } from '../state/types';
import { engine } from '../audio/engine';
import { getProject } from '../state/store';
import { restoreAutomated, scheduleAutomation } from '../audio/automation';

let installed = false;

export function installAutomationPlayback() {
  if (installed) return;
  installed = true;
  const touched = new Map<string, AutomationTarget>();
  engine.stepHooks.add((from, to, base, spt) => {
    if (engine.transport.mode !== 'song') return;
    scheduleAutomation(getProject(), engine.graph, from, to, base, spt, (time, fn) => {
      setTimeout(fn, Math.max(0, (time - engine.ctx.currentTime) * 1000));
    }, touched);
  });
  engine.onChange(() => {
    if (!engine.transport?.playing && touched.size) restoreAutomated(getProject(), engine.graph, touched);
  });
}
