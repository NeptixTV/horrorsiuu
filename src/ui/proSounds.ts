// Commands for Pro Sounds: add a sound as a channel, load a whole drum kit
// (with a starter pattern), preview sounds in the browser.
import { engine } from '../audio/engine';
import { ProInstrument } from '../audio/instruments/pro';
import { defaultPro, PRO_KITS, proSound } from '../audio/pro/catalog';
import { preloadSound } from '../audio/pro/loader';
import { addChannel, addPattern, setProSound, updateChannel, updatePattern } from '../state/actions';
import { beginGesture, endGesture, getProject, update } from '../state/store';
import { setUI, toast } from '../state/ui';
import { TICKS_PER_STEP } from '../state/types';
import { uid } from '../state/utils';

export function addProChannel(sound: string, opts: { name?: string; select?: boolean } = {}): string {
  const id = addChannel('pro', { proSound: sound, name: opts.name });
  void preloadSound(sound);
  if (opts.select !== false) {
    const def = proSound(sound);
    setUI({ selectedChannelId: id, ...(def?.oneShot ? {} : { pianoChannelId: id }) });
  }
  return id;
}

/** Puts a Pro Sound into an existing channel (any kind becomes a Pro channel). */
export function loadProIntoChannel(channelId: string, sound: string) {
  const ch = getProject().channels.find((c) => c.id === channelId);
  if (!ch) return;
  if (ch.kind === 'pro') setProSound(channelId, sound);
  else updateChannel(channelId, { kind: 'pro', pro: defaultPro(sound), name: proSound(sound)?.name ?? ch.name }, 'Replace with Pro Sound');
  void preloadSound(sound);
}

/** Root of the 808 / bass line for the project key (C1 … B1). */
function bassRoot(key: number) {
  return 24 + (((key % 12) + 12) % 12);
}

const stepValue = (c: string) => (c === 'x' ? 1 : c === 'o' ? 0.55 : 0);

/**
 * Adds every sound of a kit as a channel. With `pattern` a new 2-bar pattern
 * with the kit's starter beat (and 808 line) is created and selected.
 * One undo step.
 */
export function loadKit(kitId: string, opts: { pattern?: boolean; bass?: boolean } = {}): { patternId: string | null; channels: string[] } {
  const kit = PRO_KITS.find((k) => k.id === kitId);
  if (!kit) return { patternId: null, channels: [] };
  const withPattern = opts.pattern !== false;
  const withBass = opts.bass !== false && !!kit.bass;
  beginGesture();
  update(`Load kit ${kit.name}`, () => undefined);
  const channels: string[] = [];
  const slotIds: { id: string; slot: (typeof kit.slots)[number] }[] = [];
  for (const slot of kit.slots) {
    const id = addChannel('pro', { proSound: slot.sound, name: slot.name, volume: slot.volume ?? proSound(slot.sound)?.volume, pan: slot.pan });
    channels.push(id);
    slotIds.push({ id, slot });
    void preloadSound(slot.sound);
  }
  let bassId: string | null = null;
  if (withBass && kit.bass) {
    bassId = addChannel('pro', { proSound: kit.bass.sound, name: '808' });
    channels.push(bassId);
    void preloadSound(kit.bass.sound);
  }
  let patternId: string | null = null;
  if (withPattern) {
    patternId = addPattern();
    const len = Math.max(16, ...kit.slots.map((s) => s.steps?.length ?? 16));
    updatePattern(patternId, { name: `${kit.name} Beat`, lengthSteps: len, color: kit.color });
    const root = bassRoot(getProject().key);
    update('Kit pattern', (d) => {
      const pt = d.patterns.find((p) => p.id === patternId);
      if (!pt) return;
      for (const { id, slot } of slotIds) {
        if (!slot.steps) continue;
        pt.steps[id] = Array.from({ length: len }, (_, i) => stepValue(slot.steps![i] ?? '.'));
        if (slot.rolls) {
          const r = Array.from({ length: len }, () => 1);
          for (const [i, n] of Object.entries(slot.rolls)) r[Number(i)] = n;
          (pt.rolls ??= {})[id] = r;
        }
      }
      if (bassId && kit.bass) {
        pt.notes[bassId] = kit.bass.notes.map(([semi, start, length]) => ({
          id: uid('n'), pitch: root + semi, start: start * TICKS_PER_STEP, length: length * TICKS_PER_STEP, velocity: 0.9,
        }));
      }
    });
    setUI({ selectedPatternId: patternId });
    if (engine.transport) engine.transport.patternId = patternId;
  }
  endGesture();
  setUI({ selectedChannelId: channels[0] ?? null, ...(bassId ? { pianoChannelId: bassId } : {}) });
  toast(`🥁 ${kit.name} loaded (${channels.length} sounds)`, 'ok');
  return { patternId, channels };
}

// ---------------------------------------------------------------- preview

let preview: { inst: ProInstrument; ctx: BaseAudioContext } | null = null;

/** Plays a Pro Sound once (loads it first if needed). */
export async function previewPro(sound: string, pitch = 60) {
  if (!engine.initialized) return;
  void engine.resume();
  await preloadSound(sound, false);
  const ctx = engine.ctx;
  if (!preview || preview.ctx !== ctx) {
    preview?.inst.dispose();
    const inst = new ProInstrument(ctx);
    inst.output.connect(engine.masterOut);
    preview = { inst, ctx };
  }
  const def = proSound(sound);
  const pro = { ...defaultPro(sound), mono: true, glide: 0, level: (def?.volume ?? 0.8) * 0.9 };
  preview.inst.update({ id: 'preview', name: '', color: '', kind: 'pro', volume: 1, pan: 0, mute: false, solo: false, mixerTrackId: '', rootNote: 60, pro });
  preview.inst.noteOn(pitch, 0.85, ctx.currentTime + 0.01, def?.oneShot ? undefined : 0.9);
}
