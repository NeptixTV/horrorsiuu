// Idea dice window: roll a chord progression + bass + melody + beat in the
// project key, lock the parts you like, re-roll the rest. Each roll is one
// undo step (Ctrl+Z = previous idea). The result is a normal pattern.
import { create } from 'zustand';
import type { Draft } from 'immer';
import type { Channel, DrumType, Project } from '../../state/types';
import { MASTER_ID, TICKS_PER_STEP } from '../../state/types';
import { getProject, update, useProject } from '../../state/store';
import { setUI, toast, type FloatingWindow } from '../../state/ui';
import { DRUM_LABELS, SYNTH_PRESETS, defaultDream, defaultDrum, makeChannel, makeEffect } from '../../state/defaults';
import { applyDreamPreset, findDreamPreset } from '../../audio/instruments/dreamPresets';
import { IDEA_PARTS, MOODS, STYLES, newSeed, rollIdea, toNotes, type Idea, type IdeaMood, type IdeaOptions, type IdeaPart, type IdeaStyle } from '../../midi/ideas';
import { keyName, type ScaleMode } from '../../midi/chords';
import { paletteColor, ticksPerBar, uid } from '../../state/utils';
import { songLength } from '../../audio/sequencer';
import { engine } from '../../audio/engine';
import { setMode } from '../commands';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import './ideas.css';

interface DiceState {
  mood: IdeaMood;
  style: IdeaStyle;
  bars: 2 | 4 | 8;
  seeds: Record<IdeaPart, number>;
  locked: Record<IdeaPart, boolean>;
  enabled: Record<IdeaPart, boolean>;
  autoplay: boolean;
  patternId: string | null;
  progression: string;
  rolls: number;
}

const useDice = create<DiceState>(() => ({
  mood: 'dreamy',
  style: 'lofi',
  bars: 4,
  seeds: { chords: newSeed(), bass: newSeed(), melody: newSeed(), drums: newSeed() },
  locked: { chords: false, bass: false, melody: false, drums: false },
  enabled: { chords: true, bass: true, melody: true, drums: true },
  autoplay: true,
  patternId: null,
  progression: '',
  rolls: 0,
}));

const PART_LABEL: Record<IdeaPart, string> = { chords: 'Chords', bass: 'Bass', melody: 'Melody', drums: 'Drums' };
const PART_CHANNEL: Record<Exclude<IdeaPart, 'drums'>, string> = { chords: 'Idea Keys', bass: 'Idea Bass', melody: 'Idea Lead' };

export function projectMode(scale: string): ScaleMode {
  return /minor|dorian|phrygian|blues/i.test(scale) ? 'minor' : 'major';
}

/** Mixer insert that is still unused (same rule as "Add channel"). */
function freeMixer(d: Draft<Project>, taken: Set<string>): string {
  const used = new Set([...d.channels.map((c) => c.mixerTrackId), ...taken]);
  const free = d.mixer.find((m) => m.id !== MASTER_ID && !used.has(m.id) && m.inserts.length === 0 && m.sends.length === 0 && /^Insert \d+$/.test(m.name));
  return free?.id ?? MASTER_ID;
}

function newInstrument(part: Exclude<IdeaPart, 'drums'>, s: DiceState, p: Project): Partial<Channel> & { kind: Channel['kind'] } {
  if (part === 'chords') {
    const dreamy = s.mood === 'dreamy' || s.mood === 'chill' || s.mood === 'sad' || s.style === 'ambient' || s.style === 'lofi';
    if (dreamy) {
      const name = s.style === 'ambient' ? 'Space Ambient' : s.style === 'lofi' ? 'Lo-Fi Cassette' : s.mood === 'sad' ? 'Nostalgic Memories' : 'Ocean Dream';
      const preset = findDreamPreset(name);
      const base = { ...defaultDream(), key: ((p.key % 12) + 12) % 12, mode: projectMode(p.scale) };
      return { kind: 'dream', dream: preset ? applyDreamPreset(base, preset) : base, volume: 0.6 };
    }
    const pr = SYNTH_PRESETS.find((x) => x.name === (s.mood === 'happy' ? 'Bell Keys' : 'Soft Pad'));
    return { kind: 'synth', synth: structuredClone(pr!.params), volume: 0.5 };
  }
  if (part === 'bass') {
    const pr = SYNTH_PRESETS.find((x) => x.name === (s.style === 'trap' ? 'Sub Bass' : 'Fat Bass'));
    return { kind: 'synth', synth: structuredClone(pr!.params), volume: 0.7 };
  }
  const pr = SYNTH_PRESETS.find((x) => x.name === (s.style === 'house' || s.style === 'pop' || s.style === 'trap' ? 'Square Lead' : 'Glass Pluck'));
  return { kind: 'synth', synth: structuredClone(pr!.params), volume: 0.5 };
}

/** Writes an idea into the project (one undo step) and returns the pattern id. */
function writeIdea(idea: Idea, s: DiceState): string {
  const p = getProject();
  const existing = s.patternId && p.patterns.some((x) => x.id === s.patternId) ? s.patternId : null;
  const patternId = existing ?? uid('pt');
  update(existing ? 'Re-roll idea' : 'Roll idea', (d) => {
    const taken = new Set<string>();
    const channelFor = (name: string, make: () => Partial<Channel> & { kind: Channel['kind'] }, fx: () => ReturnType<typeof makeEffect>[] = () => []): string => {
      const found = d.channels.find((c) => c.name === name);
      if (found) return found.id;
      const mixerId = freeMixer(d, taken);
      taken.add(mixerId);
      const { kind, ...extra } = make();
      const ch = makeChannel(kind, name, mixerId, d.channels.length + 3, extra);
      d.channels.push(ch);
      const mx = d.mixer.find((m) => m.id === mixerId);
      if (mx && mx.id !== MASTER_ID) {
        mx.name = name;
        mx.color = ch.color;
        mx.inserts = fx();
      }
      return ch.id;
    };
    const drumFor = (type: DrumType): string => {
      const found = d.channels.find((c) => c.kind === 'drum' && c.drum?.type === type);
      return found ? found.id : channelFor(DRUM_LABELS[type], () => ({ kind: 'drum', drum: defaultDrum(type) }));
    };
    let pt = d.patterns.find((x) => x.id === patternId);
    if (!pt) {
      pt = { id: patternId, name: 'Idea', color: paletteColor(d.patterns.length * 3 + 2), lengthSteps: idea.lengthSteps, steps: {}, notes: {} };
      d.patterns.push(pt);
    }
    pt.name = `Idea · ${idea.progression}`.slice(0, 48);
    pt.lengthSteps = idea.lengthSteps;
    pt.steps = {};
    pt.notes = {};
    const parts: [Exclude<IdeaPart, 'drums'>, Idea['chords']][] = [['chords', idea.chords], ['bass', idea.bass], ['melody', idea.melody]];
    for (const [part, notes] of parts) {
      if (!s.enabled[part] || !notes.length) continue;
      const id = channelFor(PART_CHANNEL[part], () => newInstrument(part, s, p), () =>
        part === 'melody' ? [makeEffect('delay', { sync: '1/8d', feedback: 0.3, mix: 0.18 }), makeEffect('reverb', { wet: 0.22 })] : part === 'chords' ? [makeEffect('reverb', { wet: 0.18 })] : []);
      pt.notes[id] = toNotes(notes, () => uid('n'));
    }
    if (s.enabled.drums) {
      for (const [type, line] of Object.entries(idea.drums) as [DrumType, number[]][]) {
        if (line.some((v) => v > 0)) pt.steps[drumFor(type)] = line.slice(0, idea.lengthSteps);
      }
    }
  });
  return patternId;
}

function options(s: DiceState): IdeaOptions {
  const p = getProject();
  return { key: ((p.key % 12) + 12) % 12, mode: projectMode(p.scale), mood: s.mood, style: s.style, bars: s.bars, seeds: s.seeds };
}

/** Rolls the unlocked parts (or only `only`) and writes the idea. */
export async function rollDice(only?: IdeaPart) {
  const s = useDice.getState();
  const seeds = { ...s.seeds };
  for (const part of IDEA_PARTS) if (only ? part === only : !s.locked[part]) seeds[part] = newSeed();
  const next = { ...s, seeds };
  const idea = rollIdea(options(next));
  const patternId = writeIdea(idea, next);
  useDice.setState({ seeds, patternId, progression: idea.progression, rolls: s.rolls + 1 });
  setUI({ selectedPatternId: patternId });
  if (next.autoplay) {
    await engine.resume();
    const t = engine.transport;
    if (t.mode !== 'pattern') setMode('pattern');
    t.patternId = patternId;
    if (!t.playing) t.play();
  }
}

/** Puts the current idea at the end of the song (on its own playlist track). */
function addToSong() {
  const s = useDice.getState();
  const p = getProject();
  const pt = p.patterns.find((x) => x.id === s.patternId);
  if (!pt) return;
  const bar = ticksPerBar(p.timeSig);
  const start = Math.ceil(songLength(p) / bar) * bar;
  const used = new Set(p.clips.map((c) => c.trackId));
  const track = p.tracks.find((t) => !used.has(t.id)) ?? p.tracks[0];
  update('Add idea to song', (d) => {
    d.clips.push({ id: uid('c'), trackId: track.id, kind: 'pattern', start, length: pt.lengthSteps * TICKS_PER_STEP, offset: 0, patternId: pt.id });
  });
  // the next roll starts a new pattern, so this one stays as it is in the song
  useDice.setState({ patternId: null });
  toast(`“${pt.name}” added to the song at bar ${Math.round(start / bar) + 1}`, 'ok');
}

function Chips<T extends string | number>({ value, list, onChange }: { value: T; list: { id: T; label: string }[]; onChange: (v: T) => void }) {
  return (
    <div className="dice-chips">
      {list.map((x) => <button key={String(x.id)} className={`dice-chip ${x.id === value ? 'on' : ''}`} onClick={() => onChange(x.id)}>{x.label}</button>)}
    </div>
  );
}

export function IdeaDiceWindow({ w }: { w: FloatingWindow }) {
  const s = useDice();
  const key = useProject((st) => st.project.key);
  const scale = useProject((st) => st.project.scale);
  const bpm = useProject((st) => st.project.bpm);
  const exists = useProject((st) => !!s.patternId && st.project.patterns.some((x) => x.id === s.patternId));
  const set = (patch: Partial<DiceState>) => useDice.setState(patch);
  return (
    <Window w={w} width={430} color="#ffbe3d" title={<><Icon name="dice" size={12} /> <b>Idea dice</b> <span className="crumb">· {keyName(((key % 12) + 12) % 12, projectMode(scale))} · {Math.round(bpm)} BPM</span></>}>
      <div className="dice">
        <div className="dice-top">
          <button className={`dice-roll ${s.rolls ? '' : 'pulse'}`} onClick={() => void rollDice()} title="Roll everything that is not locked">
            <span className="dice-face" key={s.rolls}><Icon name="dice" size={26} /></span>
            <span>{s.rolls ? 'Roll again' : 'Roll an idea'}</span>
          </button>
          <div className="dice-result">
            {s.progression ? <><span className="label">Progression</span><b>{s.progression}</b></> : <span className="crumb">Chords, bass, melody and a beat that fit together – in the key and tempo of your project.</span>}
          </div>
        </div>
        <div className="dice-section">
          <span className="label">Mood</span>
          <Chips value={s.mood} list={MOODS} onChange={(mood) => set({ mood })} />
        </div>
        <div className="dice-section">
          <span className="label">Style</span>
          <Chips value={s.style} list={STYLES} onChange={(style) => set({ style })} />
        </div>
        <div className="dice-section">
          <span className="label">Length</span>
          <Chips value={s.bars} list={[{ id: 2, label: '2 bars' }, { id: 4, label: '4 bars' }, { id: 8, label: '8 bars' }]} onChange={(bars) => set({ bars: bars as 2 | 4 | 8 })} />
        </div>
        <div className="dice-parts">
          {IDEA_PARTS.map((part) => (
            <div key={part} className={`dice-part ${s.enabled[part] ? '' : 'off'}`}>
              <label className="dice-part-name">
                <input type="checkbox" checked={s.enabled[part]} onChange={() => set({ enabled: { ...s.enabled, [part]: !s.enabled[part] } })} />
                {PART_LABEL[part]}
              </label>
              <span className="spacer" />
              <button className={`btn small ${s.locked[part] ? 'on' : ''}`} onClick={() => set({ locked: { ...s.locked, [part]: !s.locked[part] } })} title="Locked parts stay the same when you roll again">
                {s.locked[part] ? '🔒 Locked' : '🔓 Lock'}
              </button>
              <button className="btn small" disabled={!s.enabled[part]} onClick={() => void rollDice(part)} title={`Roll only the ${PART_LABEL[part].toLowerCase()}`}><Icon name="dice" size={11} /></button>
            </div>
          ))}
        </div>
        <div className="dice-foot">
          <label className="dlg-check" style={{ margin: 0 }}>
            <input type="checkbox" checked={s.autoplay} onChange={() => set({ autoplay: !s.autoplay })} /> Play right away
          </label>
          <span className="spacer" />
          <button className="btn small" disabled={!exists} onClick={() => setUI({ dockOpen: true, dockTab: 'piano', selectedPatternId: s.patternId })}>Edit notes</button>
          <button className="btn small primary" disabled={!exists} onClick={addToSong}>Add to song</button>
        </div>
        <p className="crumb dice-hint">Every roll is one undo step – Ctrl+Z brings back the previous idea. Instruments are added once (Idea Keys / Bass / Lead) and keep your sound tweaks.</p>
      </div>
    </Window>
  );
}
