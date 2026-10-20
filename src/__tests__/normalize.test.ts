import { describe, expect, it } from 'vitest';
import { normalizeEffect, normalizeProject, normalizeSynth } from '../state/normalize';
import { createDemoProject, createEmptyProject } from '../state/defaults';
import { MASTER_ID } from '../state/types';

describe('project doctor', () => {
  it('leaves valid projects untouched', () => {
    for (const p of [createDemoProject(), createEmptyProject()]) {
      const r = normalizeProject(structuredClone(p));
      expect(r.fixes).toEqual([]);
      expect(r.project).toEqual(p);
    }
  });

  it('fills missing synth values instead of letting noteOn throw', () => {
    const p = createDemoProject() as unknown as { channels: { synth?: Record<string, unknown> }[] };
    const synth = p.channels.find((c) => c.synth)!.synth!;
    delete synth.resonance;
    delete (synth.osc1 as Record<string, unknown>).detune;
    synth.cutoff = Number.NaN;
    const r = normalizeProject(p);
    const s = r.project.channels.find((c) => c.synth)!.synth!;
    expect(Number.isFinite(s.resonance)).toBe(true);
    expect(Number.isFinite(s.osc1.detune)).toBe(true);
    expect(Number.isFinite(s.cutoff)).toBe(true);
    expect(r.count).toBeGreaterThan(0);
  });

  it('normalises any garbage into a playable synth', () => {
    const s = normalizeSynth({ osc1: 'x', cutoff: 'loud', attack: -5, sustain: 9, filterType: 'notch' });
    expect(s.osc1.wave).toBe('sawtooth');
    expect(s.attack).toBe(0);
    expect(s.sustain).toBe(1);
    expect(s.filterType).toBe('lowpass');
    for (const v of Object.values(s)) if (typeof v === 'number') expect(Number.isFinite(v)).toBe(true);
  });

  it('restores the master, drops dangling references and cuts feedback loops', () => {
    const p = createDemoProject();
    const raw = structuredClone(p) as unknown as Record<string, unknown> & typeof p;
    raw.mixer = raw.mixer.filter((m) => m.id !== MASTER_ID);
    const a = raw.mixer[0];
    const b = raw.mixer[1];
    a.output = b.id;
    b.output = a.id; // loop
    a.sends.push({ id: 'snd1', target: 'missing', amount: 0.5 });
    raw.clips.push({ id: 'cX', trackId: 'nope', kind: 'pattern', start: 0, length: 96, offset: 0, patternId: raw.patterns[0].id });
    raw.channels[0].mixerTrackId = 'gone';
    const r = normalizeProject(raw);
    const m = r.project.mixer;
    expect(m[0].id).toBe(MASTER_ID);
    const A = m.find((x) => x.id === a.id)!;
    const B = m.find((x) => x.id === b.id)!;
    expect(A.output === B.id && B.output === A.id).toBe(false);
    expect(A.sends.some((s) => s.target === 'missing')).toBe(false);
    expect(r.project.clips.some((c) => c.id === 'cX')).toBe(false);
    expect(r.project.channels[0].mixerTrackId).toBe(MASTER_ID);
  });

  it('removes unknown effects and clamps effect params to their range', () => {
    const p = createDemoProject();
    p.mixer[1].inserts.push({ id: 'fx-bad', type: 'warp' as never, bypass: false, params: {} });
    p.mixer[1].inserts.push({ id: 'fx-d', type: 'delay', bypass: false, params: { feedback: 7, time: Number.NaN } });
    const r = normalizeProject(structuredClone(p));
    const ins = r.project.mixer[1].inserts;
    expect(ins.some((i) => i.id === 'fx-bad')).toBe(false);
    const d = ins.find((i) => i.id === 'fx-d')!;
    expect(d.params.feedback).toBe(0.95);
    expect(d.params.time).toBe(375);
    expect(d.params.sync).toBe('1/8d');
  });

  it('never copies prototype keys from imported data', () => {
    const p = createEmptyProject() as unknown as Record<string, unknown>;
    const raw = JSON.parse(JSON.stringify(p).replace('"assets":{}', '"assets":{"__proto__":{"polluted":true}}'));
    const r = normalizeProject(raw);
    expect(({} as Record<string, unknown>).polluted).toBeUndefined();
    expect(Object.keys(r.project.assets)).toEqual([]);
  });

  it('throws for things that are not projects at all', () => {
    expect(() => normalizeProject(null)).toThrow();
    expect(() => normalizeProject('hello')).toThrow();
  });

  it('repairs effect settings from presets / old files (new effect types included)', () => {
    const tape = normalizeEffect({ id: 'a', type: 'tape', bypass: false, params: { wow: 'NaN', drive: 99, tone: -5, __proto__: { evil: 1 } } });
    expect(tape?.params.drive).toBe(1);
    expect(tape?.params.tone).toBe(1500);
    expect(tape?.params.wow).toBe(0.25);
    expect(Object.keys(tape!.params).sort()).toEqual(['drive', 'flutter', 'hiss', 'mix', 'tone', 'wow']);
    const gate = normalizeEffect({ id: 'b', type: 'gate', params: { threshold: '-30', release: 1e9 } });
    expect(gate?.params.threshold).toBe(-30);
    expect(gate?.params.release).toBe(1000);
    expect(normalizeEffect({ id: 'c', type: 'width', params: null })?.params.width).toBe(1.4);
    expect(normalizeEffect({ id: 'd', type: 'nope', params: {} })).toBeNull();
  });
});
