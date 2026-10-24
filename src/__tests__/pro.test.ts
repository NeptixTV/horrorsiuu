import { describe, expect, it } from 'vitest';
import { PRO_KITS, PRO_SOUNDS, defaultPro, proSound, proZones } from '../audio/pro/catalog';
import { coreZones, midiVel, projectZones, zonesFor } from '../audio/pro/loader';
import { collectNoteEvents } from '../audio/sequencer';
import { clipCurve } from '../audio/effects/beat';
import { createEmptyProject, makeChannel } from '../state/defaults';
import { normalizeProject } from '../state/normalize';
import { TICKS_PER_STEP } from '../state/types';
import { BEAT_GENRES, createBeatProject, kick808 } from '../project/beatStarter';
import manifest from '../audio/pro/manifest.json';

// file names only (lazy glob: nothing is loaded)
const FILES = new Set(Object.keys(import.meta.glob('../../public/sounds/**/*.ogg')).map((k) => k.replace('../../public/sounds/', '')));

describe('Pro Sounds catalog', () => {
  it('every catalog sound has zones and every file exists', () => {
    for (const s of PRO_SOUNDS) {
      const zones = proZones(s.id);
      expect(zones.length, s.id).toBeGreaterThan(0);
      for (const z of zones) expect(FILES.has(z.f), z.f).toBe(true);
    }
  });

  it('there are no stray sound files', () => {
    const used = new Set(PRO_SOUNDS.flatMap((s) => proZones(s.id).map((z) => z.f)));
    expect([...FILES].filter((f) => !used.has(f))).toEqual([]);
  });

  it('every sound in the manifest is in the catalog', () => {
    const ids = new Set(PRO_SOUNDS.map((s) => s.id));
    for (const id of Object.keys((manifest as { sounds: Record<string, unknown> }).sounds)) expect(ids.has(id), id).toBe(true);
  });

  it('zones cover the keyboard without gaps for melodic sounds', () => {
    for (const s of PRO_SOUNDS.filter((x) => !x.oneShot)) {
      const zones = proZones(s.id);
      for (let key = 36; key <= 96; key++) {
        for (const v of [0.3, 0.8, 1]) expect(zonesFor(zones, key, midiVel(v)).length, `${s.id} key ${key} vel ${v}`).toBeGreaterThan(0);
      }
    }
  });

  it('loop points lie inside the sample and are ordered', () => {
    for (const s of PRO_SOUNDS) {
      for (const z of proZones(s.id)) {
        if (z.ls === undefined) continue;
        expect(z.le!, z.f).toBeGreaterThan(z.ls + 0.2);
      }
    }
  });

  it('the core layer of the piano is one sample per key range', () => {
    const core = coreZones(proZones('keys.upright'));
    expect(core.length).toBeGreaterThan(20);
    expect(core.every((z) => midiVel(0.8) >= z.vl && midiVel(0.8) <= z.vh)).toBe(true);
  });

  it('kit starter patterns are two bars and use known sounds', () => {
    for (const k of PRO_KITS) {
      for (const slot of k.slots) {
        expect(proSound(slot.sound), slot.sound).toBeTruthy();
        if (slot.steps) expect(slot.steps.length, `${k.id} ${slot.name}`).toBe(32);
      }
      if (k.bass) expect(proSound(k.bass.sound)).toBeTruthy();
    }
  });

  it('defaults take the sound settings (808 is mono with glide)', () => {
    const p = defaultPro('bass.808.classic');
    expect(p.mono).toBe(true);
    expect(p.glide).toBeGreaterThan(0);
    expect(defaultPro('keys.upright').mono).toBe(false);
  });
});

describe('hi-hat rolls', () => {
  it('a roll splits the step into equal hits', () => {
    const p = createEmptyProject();
    const pat = p.patterns[0];
    const hat = p.channels[2];
    pat.steps[hat.id] = [1, ...new Array(15).fill(0)];
    pat.rolls = { [hat.id]: [3, ...new Array(15).fill(1)] };
    const ev = collectNoteEvents(p, 'pattern', pat.id, 0, TICKS_PER_STEP * 16).filter((e) => e.channelId === hat.id);
    expect(ev.map((e) => e.tick)).toEqual([0, 8, 16]);
    expect(ev[1].velocity).toBeLessThan(ev[0].velocity);
    expect(ev.every((e) => e.length === 8)).toBe(true);
  });

  it('survives the project doctor and drops nonsense', () => {
    const p = createEmptyProject();
    const pat = p.patterns[0];
    const hat = p.channels[2];
    (pat as unknown as { rolls: unknown }).rolls = { [hat.id]: [2, 'x', 99, -3], unknown: [4] };
    const rep = normalizeProject(JSON.parse(JSON.stringify(p)));
    expect(rep.project.patterns[0].rolls).toEqual({ [hat.id]: [2, 1, 8, 1] });
  });
});

describe('Pro channels in projects', () => {
  it('the doctor keeps Pro channels and repairs their values', () => {
    const p = createEmptyProject();
    const ch = makeChannel('pro', 'Piano', p.mixer[1].id, 0, { pro: { ...defaultPro('keys.upright'), tune: 999, glide: Number.NaN } });
    p.channels.push(ch);
    const rep = normalizeProject(JSON.parse(JSON.stringify(p)));
    const out = rep.project.channels.find((c) => c.id === ch.id)!;
    expect(out.kind).toBe('pro');
    expect(out.pro!.sound).toBe('keys.upright');
    expect(out.pro!.tune).toBe(48);
    expect(out.pro!.glide).toBe(0);
  });

  it('knows which samples a project needs (steps, notes, transposed clips)', () => {
    const p = createEmptyProject();
    const ch = makeChannel('pro', 'Piano', p.mixer[1].id, 0, { pro: defaultPro('keys.upright') });
    p.channels.push(ch);
    p.patterns[0].notes[ch.id] = [{ id: 'n1', pitch: 21, start: 0, length: 96, velocity: 0.2 }];
    const files = projectZones(p).map((z) => z.f);
    expect(files.some((f) => f.includes('/021-v60'))).toBe(true);
  });
});

describe('clipper curve', () => {
  it('is linear below the knee and never exceeds the ceiling', () => {
    const c = clipCurve(0.5, 0.5);
    const at = (x: number) => c[Math.round(((x / 16 + 1) / 2) * (c.length - 1))];
    expect(Math.abs(at(0.1) - 0.1)).toBeLessThan(0.01);
    for (let i = 0; i < c.length; i++) expect(Math.abs(c[i])).toBeLessThanOrEqual(0.5 + 1e-6);
    expect(at(10)).toBeCloseTo(0.5, 2);
  });
});

describe('Beat Starter', () => {
  it('builds a valid, arranged project for every genre', () => {
    for (const g of BEAT_GENRES) {
      const p = createBeatProject({ genre: g.id, seed: 42 });
      const rep = normalizeProject(JSON.parse(JSON.stringify(p)));
      expect(rep.count, `${g.id}: ${rep.fixes.join(', ')}`).toBe(0);
      expect(p.clips.length).toBeGreaterThan(6);
      expect(p.channels.every((c) => c.kind === 'pro' && proSound(c.pro!.sound))).toBe(true);
      // something plays in the drop
      const ev = collectNoteEvents(p, 'song', null, 4 * 384, 6 * 384);
      expect(ev.length, g.id).toBeGreaterThan(20);
    }
  });

  it('is deterministic for a seed', () => {
    const strip = (p: ReturnType<typeof createBeatProject>) => JSON.stringify({ key: p.key, notes: p.patterns.map((x) => Object.values(x.notes).flat().map((n) => [n.pitch, n.start, n.length])) });
    expect(strip(createBeatProject({ genre: 'trap', seed: 7 }))).toBe(strip(createBeatProject({ genre: 'trap', seed: 7 })));
  });

  it('puts an 808 note on every kick, in the 808 range', () => {
    const kit = PRO_KITS.find((k) => k.id === 'trap')!;
    const notes = kick808(kit, [9, 5, 0, 7], 384, 4);
    const kicks = kit.slots[0].steps!.split('').filter((c) => c === 'x').length * 2;
    expect(notes.length).toBeGreaterThanOrEqual(kicks);
    for (const n of notes) expect(n.pitch >= 28 && n.pitch <= 51).toBe(true);
  });
});
