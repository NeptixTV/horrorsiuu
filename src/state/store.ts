// Project store with undo/redo. The project is an immutable tree updated with
// immer, so unchanged branches keep their identity (cheap diffing for the
// audio engine and memoised React components).
import { create } from 'zustand';
import { produce, type Draft } from 'immer';
import type { Project } from './types';
import { createDemoProject } from './defaults';

let historyLimit = 150;

/** Number of undo steps kept (Settings → Projects). */
export function setHistoryLimit(n: number) {
  historyLimit = Math.max(10, Math.min(1000, Math.round(n) || 150));
  const s = useProject.getState();
  if (s.past.length > historyLimit) useProject.setState({ past: s.past.slice(-historyLimit), pastLabels: s.pastLabels.slice(-historyLimit) });
}

export interface ProjectState {
  project: Project;
  past: Project[];
  /** label of the action that led from past[i] to the next state */
  pastLabels: string[];
  future: Project[];
  futureLabels: string[];
  /** project snapshot at the start of a continuous gesture (knob drag etc.) */
  gestureBase: Project | null;
  gestureLabel: string;
  gestureStart: number;
  dirty: boolean;
  lastSaved: number | null;
  lastAutosave: number | null;
  lastAction: string;
}

export const useProject = create<ProjectState>(() => ({
  project: createDemoProject(),
  past: [],
  pastLabels: [],
  future: [],
  futureLabels: [],
  gestureBase: null,
  gestureLabel: '',
  gestureStart: 0,
  dirty: false,
  lastSaved: null,
  lastAutosave: null,
  lastAction: '',
}));

export const getProject = () => useProject.getState().project;

/**
 * Applies a change to the project. Creates an undo step unless a gesture is
 * active (the gesture creates one step when it ends) or history is disabled.
 */
export function update(label: string, recipe: (draft: Draft<Project>) => void, opts: { history?: boolean } = {}) {
  const s = useProject.getState();
  const next = produce(s.project, (d) => {
    recipe(d);
  });
  if (next === s.project) return;
  const record = opts.history !== false && !s.gestureBase;
  useProject.setState({
    project: next,
    past: record ? [...s.past, s.project].slice(-historyLimit) : s.past,
    pastLabels: record ? [...s.pastLabels, label].slice(-historyLimit) : s.pastLabels,
    future: record ? [] : s.future,
    futureLabels: record ? [] : s.futureLabels,
    dirty: true,
    lastAction: label,
    ...(s.gestureBase && !s.gestureLabel ? { gestureLabel: label } : {}),
  });
}

export function beginGesture() {
  const s = useProject.getState();
  if (!s.gestureBase) useProject.setState({ gestureBase: s.project, gestureLabel: '', gestureStart: Date.now() });
}

export function endGesture() {
  const s = useProject.getState();
  const base = s.gestureBase;
  if (!base) return;
  if (base !== s.project) {
    useProject.setState({
      gestureBase: null,
      gestureLabel: '',
      past: [...s.past, base].slice(-historyLimit),
      pastLabels: [...s.pastLabels, s.gestureLabel || s.lastAction || 'Edit'].slice(-historyLimit),
      future: [],
      futureLabels: [],
    });
  } else {
    useProject.setState({ gestureBase: null, gestureLabel: '' });
  }
}

/**
 * Safety net: a gesture whose pointer-up never arrived (window lost focus
 * mid-drag, a dialog opened …) would otherwise swallow every later edit
 * from the undo history.
 */
export function installGestureSafety() {
  const close = () => {
    if (useProject.getState().gestureBase) setTimeout(endGesture, 0);
  };
  window.addEventListener('pointerup', close);
  window.addEventListener('pointercancel', close);
  window.addEventListener('blur', close);
  setInterval(() => {
    const s = useProject.getState();
    // wheel gestures end on their own after ~0.5 s; anything open for > 30 s is stale
    if (s.gestureBase && Date.now() - s.gestureStart > 30000) endGesture();
  }, 5000);
}

export function undo() {
  const s = useProject.getState();
  if (s.gestureBase) endGesture();
  const st = useProject.getState();
  const prev = st.past[st.past.length - 1];
  if (!prev) return false;
  const label = st.pastLabels[st.pastLabels.length - 1] ?? 'Edit';
  useProject.setState({
    project: prev,
    past: st.past.slice(0, -1),
    pastLabels: st.pastLabels.slice(0, -1),
    future: [st.project, ...st.future],
    futureLabels: [label, ...st.futureLabels],
    dirty: true,
    lastAction: `Undo ${label}`,
  });
  return true;
}

export function redo() {
  const s = useProject.getState();
  const next = s.future[0];
  if (!next) return false;
  const label = s.futureLabels[0] ?? 'Edit';
  useProject.setState({
    project: next,
    past: [...s.past, s.project],
    pastLabels: [...s.pastLabels, label],
    future: s.future.slice(1),
    futureLabels: s.futureLabels.slice(1),
    dirty: true,
    lastAction: `Redo ${label}`,
  });
  return true;
}

/** Jumps to a point in the history: index into `past` (0 = oldest), or past.length = now. */
export function jumpToHistory(index: number) {
  const s = useProject.getState();
  const n = s.past.length;
  if (index < n) {
    for (let i = n; i > index; i--) undo();
  } else {
    const steps = index - n;
    for (let i = 0; i < steps; i++) redo();
  }
}

export function loadProjectState(project: Project, opts: { dirty?: boolean } = {}) {
  useProject.setState({
    project, past: [], pastLabels: [], future: [], futureLabels: [], gestureBase: null, gestureLabel: '', dirty: opts.dirty ?? false,
    lastSaved: opts.dirty ? null : Date.now(), lastAutosave: null, lastAction: 'Open',
  });
}

export function markSaved(autosave = false) {
  useProject.setState(autosave ? { lastAutosave: Date.now(), dirty: false } : { lastSaved: Date.now(), dirty: false });
}
