import { beforeEach, describe, expect, it } from 'vitest';
import { beginGesture, endGesture, loadProjectState, redo, undo, update, useProject } from '../state/store';
import { createEmptyProject } from '../state/defaults';
import { addSend, setOutput } from '../state/actions';
import { audibleTracks, wouldCycle } from '../state/routing';
import { MASTER_ID } from '../state/types';

beforeEach(() => loadProjectState(createEmptyProject()));

describe('undo / redo', () => {
  it('undoes and redoes a change', () => {
    update('Tempo', (d) => { d.bpm = 99; });
    expect(useProject.getState().project.bpm).toBe(99);
    undo();
    expect(useProject.getState().project.bpm).toBe(130);
    redo();
    expect(useProject.getState().project.bpm).toBe(99);
  });

  it('groups a gesture into a single undo step', () => {
    beginGesture();
    for (let i = 0; i < 20; i++) update('Drag', (d) => { d.swing = i / 20; });
    endGesture();
    expect(useProject.getState().past).toHaveLength(1);
    undo();
    expect(useProject.getState().project.swing).toBe(0);
  });

  it('keeps unchanged branches structurally shared', () => {
    const before = useProject.getState().project;
    update('Tempo', (d) => { d.bpm = 140; });
    const after = useProject.getState().project;
    expect(after.mixer).toBe(before.mixer);
    expect(after.channels).toBe(before.channels);
  });
});

describe('routing', () => {
  it('prevents feedback loops', () => {
    const m = useProject.getState().project.mixer;
    const [a, b] = [m[1].id, m[2].id];
    expect(setOutput(a, b)).toBe(true);
    expect(setOutput(b, a)).toBe(false);
    expect(addSend(b, a)).toBe(false);
    expect(wouldCycle(useProject.getState().project.mixer, a, a)).toBe(true);
  });

  it('keeps upstream and downstream tracks of a soloed track audible', () => {
    const m = useProject.getState().project.mixer;
    const [a, bus, other] = [m[1].id, m[2].id, m[3].id];
    setOutput(a, bus);
    update('solo', (d) => { d.mixer[1].solo = true; });
    const audible = audibleTracks(useProject.getState().project.mixer);
    expect(audible.has(a)).toBe(true);
    expect(audible.has(bus)).toBe(true);
    expect(audible.has(MASTER_ID)).toBe(true);
    expect(audible.has(other)).toBe(false);
  });
});
