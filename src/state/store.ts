// Project store with undo/redo. The project is an immutable tree updated with
// immer, so unchanged branches keep their identity (cheap diffing for the
// audio engine and memoised React components).
import { create } from 'zustand';
import { produce, type Draft } from 'immer';
import type { Project } from './types';
import { createDemoProject } from './defaults';

const HISTORY_LIMIT = 150;

export interface ProjectState {
  project: Project;
  past: Project[];
  future: Project[];
  /** project snapshot at the start of a continuous gesture (knob drag etc.) */
  gestureBase: Project | null;
  dirty: boolean;
  lastSaved: number | null;
  lastAutosave: number | null;
  lastAction: string;
}

export const useProject = create<ProjectState>(() => ({
  project: createDemoProject(),
  past: [],
  future: [],
  gestureBase: null,
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
    past: record ? [...s.past, s.project].slice(-HISTORY_LIMIT) : s.past,
    future: record ? [] : s.future,
    dirty: true,
    lastAction: label,
  });
}

export function beginGesture() {
  const s = useProject.getState();
  if (!s.gestureBase) useProject.setState({ gestureBase: s.project });
}

export function endGesture() {
  const s = useProject.getState();
  const base = s.gestureBase;
  if (!base) return;
  if (base !== s.project) {
    useProject.setState({ gestureBase: null, past: [...s.past, base].slice(-HISTORY_LIMIT), future: [] });
  } else {
    useProject.setState({ gestureBase: null });
  }
}

export function undo() {
  const s = useProject.getState();
  if (s.gestureBase) endGesture();
  const st = useProject.getState();
  const prev = st.past[st.past.length - 1];
  if (!prev) return false;
  useProject.setState({ project: prev, past: st.past.slice(0, -1), future: [st.project, ...st.future], dirty: true, lastAction: 'Undo' });
  return true;
}

export function redo() {
  const s = useProject.getState();
  const next = s.future[0];
  if (!next) return false;
  useProject.setState({ project: next, past: [...s.past, s.project], future: s.future.slice(1), dirty: true, lastAction: 'Redo' });
  return true;
}

export function loadProjectState(project: Project, opts: { dirty?: boolean } = {}) {
  useProject.setState({
    project, past: [], future: [], gestureBase: null, dirty: opts.dirty ?? false,
    lastSaved: opts.dirty ? null : Date.now(), lastAutosave: null, lastAction: 'Open',
  });
}

export function markSaved(autosave = false) {
  useProject.setState(autosave ? { lastAutosave: Date.now(), dirty: false } : { lastSaved: Date.now(), dirty: false });
}
