// DreamChord: a pocket chord synthesizer. Seven chord buttons play the
// diatonic chords of the selected key, the joystick changes the held chord
// live (7th, 9th, sus, major/minor …) or bends / filters it, knobs shape the
// sound, function buttons switch hold, arpeggiator, strum, octave and key,
// and REC records what you play into the selected pattern.
import { useEffect, useMemo, useRef, useState, useSyncExternalStore, type PointerEvent as RPE } from 'react';
import type { DreamParams, DreamWave } from '../../state/types';
import { useProject, getProject } from '../../state/store';
import { setUI, toast, ui, useUI, type FloatingWindow } from '../../state/ui';
import { updateDream } from '../../state/actions';
import { engine } from '../../audio/engine';
import { DREAM_PRESETS, applyDreamPreset, findDreamPreset } from '../../audio/instruments/dreamPresets';
import { diatonicChord, JOY_MODS, keyName, MOD_LABELS, romanNumerals, type JoyDir } from '../../midi/chords';
import { liveRecorder } from '../../midi/liveRecord';
import { captureToPattern, capture } from '../../midi/capture';
import { deleteUserPreset, saveUserPreset, usePresets } from '../../state/userPresets';
import { Window } from '../components/Window';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { onFrame } from '../components/frameLoop';
import { askText } from '../dialogs/askText';
import { registerKeyOverride } from '../shortcuts';
import { getPerformer, installArpClock, type DreamPerformer } from './performer';
import './dreamchord.css';

const WAVES: { value: DreamWave; label: string }[] = [
  { value: 'sine', label: 'Sine' }, { value: 'triangle', label: 'Triangle' }, { value: 'sawtooth', label: 'Saw' }, { value: 'square', label: 'Square' },
  { value: 'glass', label: 'Glass' }, { value: 'piano', label: 'Piano' }, { value: 'organ', label: 'Organ' }, { value: 'choir', label: 'Choir' },
];
const ARP_MODES: DreamParams['arp'][] = ['up', 'down', 'updown', 'random', 'chord'];
const pct = (v: number) => `${Math.round(v * 100)}%`;
const secs = (v: number) => (v < 1 ? `${Math.round(v * 1000)} ms` : `${v.toFixed(2)} s`);
const hz = (v: number) => (v >= 1000 ? `${(v / 1000).toFixed(1)} kHz` : `${Math.round(v)} Hz`);

function currentDream(channelId: string) {
  return getProject().channels.find((c) => c.id === channelId)?.dream;
}

function toggleHold(channelId: string, performer: DreamPerformer) {
  const cur = currentDream(channelId);
  if (!cur) return;
  updateDream(channelId, { hold: !cur.hold }, 'Hold');
  if (cur.hold) performer.unlatch();
}

function toggleArp(channelId: string, performer: DreamPerformer) {
  const cur = currentDream(channelId);
  if (!cur) return;
  updateDream(channelId, { arp: cur.arp === 'off' ? 'up' : 'off' }, 'Arpeggiator');
  performer.refresh();
}

/** The sound part of the settings (what a user preset stores). */
function soundOf(p: DreamParams): Partial<DreamParams> {
  const s: Partial<DreamParams> = { ...p };
  delete s.key;
  delete s.mode;
  delete s.octave;
  delete s.hold;
  delete s.preset;
  return s;
}

function usePerformer(p: DreamPerformer) {
  return useSyncExternalStore((cb) => p.onChange(cb), () => p.view);
}

function useRecorder() {
  return useSyncExternalStore((cb) => liveRecorder.onChange(cb), () => liveRecorder.active);
}

const DIR_POS: Record<Exclude<JoyDir, 'c'>, [number, number]> = {
  n: [50, 4], ne: [88, 16], e: [99, 50], se: [88, 84], s: [50, 97], sw: [12, 84], w: [1, 50], nw: [12, 16],
};

function Joystick({ performer, mode, dir, joy }: { performer: DreamPerformer; mode: DreamParams['joystick']; dir: JoyDir; joy: { x: number; y: number } }) {
  const well = useRef<HTMLDivElement>(null);
  const [pos, setPos] = useState({ x: 0, y: 0 });
  const [dragging, setDragging] = useState(false);
  const move = (e: RPE) => {
    const r = well.current!.getBoundingClientRect();
    const rad = r.width / 2 - 14;
    let x = (e.clientX - (r.left + r.width / 2)) / rad;
    let y = -(e.clientY - (r.top + r.height / 2)) / rad;
    const m = Math.hypot(x, y);
    if (m > 1) { x /= m; y /= m; }
    setPos({ x, y });
    performer.setJoystick(x, y);
  };
  const end = () => {
    setDragging(false);
    setPos({ x: 0, y: 0 });
    performer.releaseJoystick();
  };
  const labels: Partial<Record<Exclude<JoyDir, 'c'>, string>> = mode === 'chord'
    ? { n: '7', ne: '9', e: 'add9', se: '6', s: 'sus4', sw: 'sus2', w: 'M/m', nw: 'V7' }
    : mode === 'bend'
      ? { n: 'vib', e: 'bend +', w: 'bend −', s: 'dark' }
      : { n: 'bright', s: 'dark', e: 'vib', w: 'vib' };
  return (
    <div className={`dc-joy mode-${mode}`}>
      <div
        ref={well}
        className={`dc-joy-well ${dragging ? 'drag' : ''}`}
        onPointerDown={(e) => { e.preventDefault(); (e.currentTarget as Element).setPointerCapture(e.pointerId); setDragging(true); move(e); }}
        onPointerMove={(e) => { if (dragging) move(e); }}
        onPointerUp={end}
        onPointerCancel={end}
        title="Joystick – push while holding a chord (arrow keys work too)"
      >
        {(Object.keys(DIR_POS) as Exclude<JoyDir, 'c'>[]).map((d) => labels[d] && (
          <span key={d} className={`dc-joy-label ${dir === d ? 'on' : ''}`} style={{ left: `${DIR_POS[d][0]}%`, top: `${DIR_POS[d][1]}%` }}>{labels[d]}</span>
        ))}
        <div className="dc-joy-ring" />
        {/* follows the mouse while dragging, otherwise the arrow keys */}
        <div className="dc-joy-stick" style={{ transform: `translate(calc(-50% + ${(dragging ? pos : joy).x * 34}px), calc(-50% + ${-(dragging ? pos : joy).y * 34}px))`, transition: dragging ? 'none' : undefined }}>
          <div className="dc-joy-cap" />
        </div>
      </div>
      <div className="dc-joy-mode">
        {(['chord', 'bend', 'filter'] as const).map((m) => (
          <button key={m} className={`dc-mini ${mode === m ? 'on' : ''}`} onClick={() => performer && updateDream(performer.channelId, { joystick: m }, 'Joystick mode')}>{m}</button>
        ))}
      </div>
    </div>
  );
}

function MiniKeys({ notes }: { notes: number[] }) {
  const lo = 36;
  const hi = 96;
  const on = new Set(notes);
  const whites: number[] = [];
  for (let n = lo; n < hi; n++) if (![1, 3, 6, 8, 10].includes(n % 12)) whites.push(n);
  const w = 100 / whites.length;
  return (
    <div className="dc-keys">
      {whites.map((n, i) => <div key={n} className={`dc-key w ${on.has(n) ? 'on' : ''}`} style={{ left: `${i * w}%`, width: `${w}%` }} />)}
      {whites.map((n, i) => ([1, 3, 6, 8, 10].includes((n + 1) % 12) && n + 1 < hi
        ? <div key={`b${n}`} className={`dc-key b ${on.has(n + 1) ? 'on' : ''}`} style={{ left: `${(i + 1) * w - w * 0.3}%`, width: `${w * 0.6}%` }} />
        : null))}
    </div>
  );
}

function Scope({ channelId }: { channelId: string }) {
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    const buf = new Float32Array(512);
    return onFrame(() => {
      const c = ref.current;
      if (!c || !engine.initialized) return;
      const an = engine.graph.channels.get(channelId)?.analyser;
      const g = c.getContext('2d')!;
      const w = c.width;
      const h = c.height;
      g.clearRect(0, 0, w, h);
      if (!an) return;
      an.getFloatTimeDomainData(buf);
      g.strokeStyle = getComputedStyle(c).color;
      g.lineWidth = 1.5;
      g.beginPath();
      for (let i = 0; i < buf.length; i++) {
        const x = (i / (buf.length - 1)) * w;
        const y = h / 2 - Math.max(-1, Math.min(1, buf[i] * 2.2)) * (h / 2 - 2);
        if (i) g.lineTo(x, y);
        else g.moveTo(x, y);
      }
      g.stroke();
    });
  }, [channelId]);
  return <canvas ref={ref} className="dc-scope" width={220} height={44} />;
}

export function DreamChordWindow({ w, channelId }: { w: FloatingWindow; channelId: string }) {
  const channel = useProject((s) => s.project.channels.find((c) => c.id === channelId));
  const bpm = useProject((s) => s.project.bpm);
  const performer = useMemo(() => getPerformer(channelId), [channelId]);
  const view = usePerformer(performer);
  const recording = useRecorder();
  const focus = useUI((s) => s.focus);
  const recQuantize = useUI((s) => s.recQuantize);
  const userPresets = usePresets((s) => s.list).filter((p) => p.kind === 'dream');
  const [popup, setPopup] = useState<{ label: string; value: string; norm: number } | null>(null);
  const [editOpen, setEditOpen] = useState(false);
  const popupTimer = useRef<ReturnType<typeof setTimeout> | null>(null);
  const [captured, setCaptured] = useState(capture.notes.length);
  useEffect(() => capture.onChange(() => setCaptured(capture.notes.length)), []);
  useEffect(() => { installArpClock(); }, []);
  // stop sounding notes when the window closes
  useEffect(() => () => performer.panic(), [performer]);

  const p = channel?.dream;

  // keyboard: 1-7 chords, arrows = joystick, H hold, A arp (while the DreamChord has focus)
  useEffect(() => {
    if (!p) return;
    const arrows = new Set<string>();
    const joyFromArrows = () => {
      const x = (arrows.has('ArrowRight') ? 1 : 0) - (arrows.has('ArrowLeft') ? 1 : 0);
      const y = (arrows.has('ArrowUp') ? 1 : 0) - (arrows.has('ArrowDown') ? 1 : 0);
      if (x === 0 && y === 0) performer.releaseJoystick();
      else performer.setJoystick(x * 0.9, y * 0.9);
    };
    return registerKeyOverride((e, down) => {
      if (ui().focus !== 'dream' || e.ctrlKey || e.metaKey || e.altKey) return false;
      if (/^[1-7]$/.test(e.key)) {
        if (down && !e.repeat) performer.press(Number(e.key));
        if (!down) performer.release(Number(e.key));
        return true;
      }
      if (e.key.startsWith('Arrow')) {
        if (down) arrows.add(e.key);
        else arrows.delete(e.key);
        joyFromArrows();
        return true;
      }
      if (!down || e.repeat) return false;
      const k = e.key.toLowerCase();
      if (k === 'h') { toggleHold(channelId, performer); return true; }
      if (k === 'a') { toggleArp(channelId, performer); return true; }
      return false;
    });
  }, [performer, channelId, !!p]);

  if (!channel || !p) return null;
  const preset = findDreamPreset(p.preset);
  const accent = preset?.color ?? '#3fc7e8';

  const set = (patch: Partial<DreamParams>, label?: string, show?: { label: string; value: string; norm: number }) => {
    updateDream(channelId, patch, label);
    if (show) {
      setPopup(show);
      if (popupTimer.current) clearTimeout(popupTimer.current);
      popupTimer.current = setTimeout(() => setPopup(null), 1100);
    }
    if ('key' in patch || 'mode' in patch || 'octave' in patch || 'voicing' in patch) setTimeout(() => performer.refresh(), 0);
  };
  const loadPreset = (name: string) => {
    const pr = findDreamPreset(name);
    if (!pr) return;
    set(applyDreamPreset(p, pr), `Preset ${name}`);
  };
  const stepPreset = (d: number) => {
    const i = DREAM_PRESETS.findIndex((x) => x.name === p.preset);
    loadPreset(DREAM_PRESETS[(i + d + DREAM_PRESETS.length) % DREAM_PRESETS.length].name);
  };
  const presetMenu = (x: number, y: number) => {
    const cats = [...new Set(DREAM_PRESETS.map((x) => x.category))];
    const items: MenuItem[] = cats.map((c) => ({
      label: c, submenu: DREAM_PRESETS.filter((x) => x.category === c).map((x) => ({ label: x.name, checked: x.name === p.preset, onClick: () => loadPreset(x.name) })),
    }));
    if (userPresets.length) {
      items.push({ separator: true });
      items.push({ label: 'My presets', submenu: userPresets.map((u) => ({ label: u.name, checked: u.name === p.preset, onClick: () => set({ ...p, ...(u.params as Partial<DreamParams>), preset: u.name }, `Preset ${u.name}`) })) });
      items.push({ label: 'Delete my preset', submenu: userPresets.map((u) => ({ label: u.name, danger: true, onClick: () => { deleteUserPreset(u.id); toast(`Deleted “${u.name}”`, 'ok'); } })) });
    }
    items.push({ separator: true });
    items.push({
      label: 'Save current sound as preset…',
      onClick: async () => {
        const name = await askText('Preset name', `${p.preset} (mine)`);
        if (!name) return;
        saveUserPreset('dream', name, soundOf(p));
        set({ preset: name.trim() }, 'Name preset');
        toast(`Saved preset “${name.trim()}”`, 'ok');
      },
    });
    showMenu(x, y, items);
  };

  const romans = romanNumerals(p.mode);
  const modLabel = MOD_LABELS[JOY_MODS[view.dir]];
  const knob = (key: keyof DreamParams, label: string, min: number, max: number, def: number, fmt: (v: number) => string, log = false) => (
    <Knob
      key={key}
      value={p[key] as number}
      min={min}
      max={max}
      defaultValue={def}
      log={log}
      size={46}
      color={accent}
      label={label}
      format={fmt}
      onChange={(v) => {
        const norm = log ? Math.log(v / min) / Math.log(max / min) : (v - min) / (max - min);
        set({ [key]: v } as Partial<DreamParams>, `DreamChord ${label}`, { label, value: fmt(v), norm });
      }}
    />
  );

  return (
    <Window
      w={w}
      width={940}
      color={accent}
      title={<><Icon name="dream" size={12} /> <b>DreamChord</b> <span className="crumb">· {channel.name}</span></>}
      actions={(
        <>
          <button className="btn small" onClick={(e) => presetMenu(e.clientX, e.clientY)}><Icon name="preset" size={11} /> Presets</button>
          <button className="btn small" title="Stop all notes" onClick={() => performer.panic()}>Panic</button>
        </>
      )}
    >
      <div
        className={`dc ${focus === 'dream' ? 'focused' : ''}`}
        style={{ ['--dc' as string]: accent }}
        onPointerDown={() => { if (ui().focus !== 'dream') setUI({ focus: 'dream', selectedChannelId: channelId, pianoChannelId: channelId }); }}
      >
        <div className="dc-body">
          <div className="dc-top">
            <div className={`dc-display ${view.chord ? 'live' : ''}`}>
              <div className="dc-disp-row">
                <span>{keyName(p.key, p.mode)}</span>
                <span>♩ {Math.round(bpm)}</span>
                <span className="spacer" />
                {p.hold && <span className="dc-pill">HOLD</span>}
                {p.arp !== 'off' && <span className={`dc-pill ${view.arpIndex >= 0 ? 'blink' : ''}`} key={view.arpIndex}>ARP {p.arp.toUpperCase()}</span>}
                {p.strum > 0 && <span className="dc-pill">STRUM</span>}
                {p.octave !== 0 && <span className="dc-pill">OCT {p.octave > 0 ? '+' : ''}{p.octave}</span>}
                {recording && <span className="dc-pill rec">● REC</span>}
              </div>
              <div className="dc-disp-main">
                {popup ? (
                  <div className="dc-popup">
                    <div className="dc-popup-label">{popup.label}</div>
                    <div className="dc-popup-bar"><div style={{ width: `${Math.max(0, Math.min(1, popup.norm)) * 100}%` }} /></div>
                    <div className="dc-popup-value">{popup.value}</div>
                  </div>
                ) : view.chord ? (
                  <div className="dc-chord" key={view.hits}>
                    <div className="dc-chord-name">{view.chord.name}</div>
                    <div className="dc-chord-sub">{view.chord.degree} · {romans[view.chord.degree - 1]}{view.dir !== 'c' ? ` · ${modLabel}` : ''}</div>
                  </div>
                ) : (
                  <div className="dc-idle">
                    <div className="dc-chord-name dim">{p.preset}</div>
                    <div className="dc-chord-sub">{preset?.info ?? 'My preset'} · press 1–7</div>
                  </div>
                )}
              </div>
              <div className="dc-disp-row bottom">
                <MiniKeys notes={view.notes} />
                <Scope channelId={channelId} />
              </div>
            </div>
            <Joystick performer={performer} mode={p.joystick} dir={view.dir} joy={view.joy} />
          </div>

          <div className="dc-knobs">
            {knob('cutoff', 'Tone', 200, 12000, 2600, hz, true)}
            {knob('reverb', 'Space', 0, 1, 0.55, pct)}
            {knob('delay', 'Echo', 0, 1, 0.25, pct)}
            {knob('chorus', 'Dream', 0, 1, 0.55, pct)}
            {knob('tape', 'Tape', 0, 1, 0.2, pct)}
            {knob('attack', 'Attack', 0.002, 4, 0.35, secs, true)}
            {knob('release', 'Release', 0.05, 8, 2.2, secs, true)}
            {knob('volume', 'Volume', 0, 1, 0.75, pct)}
          </div>

          <div className="dc-funcs">
            <button className={`dc-fn ${p.hold ? 'on' : ''}`} onClick={() => toggleHold(channelId, performer)} title="Hold – chords keep sounding after you let go (H)">HOLD</button>
            <button className={`dc-fn ${p.arp !== 'off' ? 'on' : ''}`} onClick={() => toggleArp(channelId, performer)} onContextMenu={(e) => { e.preventDefault(); showMenu(e.clientX, e.clientY, ARP_MODES.map((m) => ({ label: m, checked: p.arp === m, onClick: () => set({ arp: m }, 'Arp mode') }))); }} title="Arpeggiator (A) – right-click for the pattern">ARP</button>
            <button className={`dc-fn ${p.strum > 0 ? 'on' : ''}`} onClick={() => set({ strum: p.strum > 0 ? 0 : 55 }, 'Strum')} title="Strum – notes of the chord one after another">STRUM</button>
            <span className="dc-sep" />
            <button className="dc-fn" onClick={() => set({ octave: Math.max(-2, p.octave - 1) }, 'Octave', { label: 'Octave', value: String(p.octave - 1), norm: (p.octave + 1) / 4 })}>OCT −</button>
            <button className="dc-fn" onClick={() => set({ octave: Math.min(2, p.octave + 1) }, 'Octave', { label: 'Octave', value: String(p.octave + 1), norm: (p.octave + 3) / 4 })}>OCT +</button>
            <span className="dc-sep" />
            <button className="dc-fn" onClick={() => set({ key: (p.key + 11) % 12 }, 'Key')}>KEY ◀</button>
            <button className="dc-fn wide" onClick={() => set({ key: (p.key + 1) % 12 }, 'Key')} onContextMenu={(e) => { e.preventDefault(); showMenu(e.clientX, e.clientY, Array.from({ length: 12 }, (_, k) => ({ label: keyName(k, p.mode), checked: k === p.key, onClick: () => set({ key: k }, 'Key') }))); }} title="Next key – right-click to pick">KEY ▶</button>
            <button className={`dc-fn ${p.mode === 'minor' ? 'on' : ''}`} onClick={() => set({ mode: p.mode === 'major' ? 'minor' : 'major' }, 'Major/minor')}>{p.mode === 'major' ? 'MAJOR' : 'MINOR'}</button>
            <button className="dc-fn" onClick={() => set({ voicing: p.voicing === 'close' ? 'open' : p.voicing === 'open' ? 'spread' : 'close' }, 'Voicing')} title="Voicing: close / open / spread">{p.voicing.toUpperCase()}</button>
            <span className="dc-sep" />
            <button className={`dc-fn rec ${recording ? 'on' : ''}`} onClick={() => liveRecorder.toggle()} title="Record what you play into the selected pattern (starts the pattern loop)">● REC</button>
            <button className="dc-fn" disabled={!captured} onClick={() => captureToPattern(engine.ctx.currentTime)} title="Capture: turn what you just played into a new pattern – even if you were not recording">CAPTURE</button>
            <button className="dc-fn" onClick={() => stepPreset(-1)} title="Previous preset">◀</button>
            <button className="dc-fn" onClick={() => stepPreset(1)} title="Next preset">▶</button>
          </div>

          <div className="dc-pads">
            {[1, 2, 3, 4, 5, 6, 7].map((d) => {
              const active = view.degree === d;
              const chord = active && view.chord ? view.chord : diatonicChord(p.key, p.mode, d - 1, 'none');
              return (
                <button
                  key={d}
                  className={`dc-pad ${active ? 'on' : ''}`}
                  onPointerDown={(e) => {
                    e.preventDefault();
                    (e.currentTarget as Element).setPointerCapture(e.pointerId);
                    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
                    // press lower on the pad for a softer touch
                    const vel = 1 - Math.max(0, Math.min(1, (e.clientY - r.top) / r.height)) * 0.45;
                    performer.press(d, vel);
                  }}
                  onPointerUp={() => performer.release(d)}
                  onPointerCancel={() => performer.release(d)}
                >
                  <span className="dc-pad-num">{d}</span>
                  <span className="dc-pad-chord">{chord.name}</span>
                  <span className="dc-pad-roman">{romans[d - 1]}</span>
                </button>
              );
            })}
          </div>
        </div>

        <button className="dc-edit-toggle" onClick={() => setEditOpen((x) => !x)}>
          <Icon name={editOpen ? 'chevronUp' : 'chevronDown'} size={12} /> Sound editor &amp; performance settings
        </button>
        {editOpen && (
          <div className="dc-edit">
            <section>
              <h4>Oscillators</h4>
              <label>Wave A <select className="input small-select" value={p.waveA} onChange={(e) => set({ waveA: e.target.value as DreamWave }, 'Wave A')}>{WAVES.map((x) => <option key={x.value} value={x.value}>{x.label}</option>)}</select></label>
              <label>Wave B <select className="input small-select" value={p.waveB} onChange={(e) => set({ waveB: e.target.value as DreamWave }, 'Wave B')}>{WAVES.map((x) => <option key={x.value} value={x.value}>{x.label}</option>)}</select></label>
              <div className="dc-edit-knobs">
                {knob('mixB', 'Mix B', 0, 1, 0.35, pct)}
                {knob('detune', 'Detune', 0, 60, 14, (v) => `${Math.round(v)} ct`)}
                {knob('sub', 'Sub', 0, 1, 0.25, pct)}
                {knob('air', 'Air', 0, 1, 0.12, pct)}
              </div>
            </section>
            <section>
              <h4>Filter &amp; envelope</h4>
              <div className="dc-edit-knobs">
                {knob('resonance', 'Reso', 0, 18, 1, (v) => `${v.toFixed(1)} dB`)}
                {knob('filterEnv', 'Env', 0, 1, 0.25, pct)}
                {knob('decay', 'Decay', 0.01, 10, 1.2, secs, true)}
                {knob('sustain', 'Sustain', 0, 1, 0.75, pct)}
                {knob('vibrato', 'Vibrato', 0, 1, 0.15, pct)}
              </div>
            </section>
            <section>
              <h4>Space</h4>
              <label>Echo time <select className="input small-select" value={p.delayTime} onChange={(e) => set({ delayTime: e.target.value as DreamParams['delayTime'] }, 'Echo time')}>{(['1/4', '1/8', '1/8d', '1/16'] as const).map((x) => <option key={x} value={x}>{x === '1/8d' ? '1/8 dotted' : x}</option>)}</select></label>
              <div className="dc-edit-knobs">
                {knob('space', 'Size', 0, 1, 0.8, pct)}
              </div>
            </section>
            <section>
              <h4>Performance</h4>
              <label>Arp <select className="input small-select" value={p.arp} onChange={(e) => set({ arp: e.target.value as DreamParams['arp'] }, 'Arp mode')}>{(['off', ...ARP_MODES] as const).map((x) => <option key={x} value={x}>{x}</option>)}</select></label>
              <label>Rate <select className="input small-select" value={p.arpRate} onChange={(e) => set({ arpRate: e.target.value as DreamParams['arpRate'] }, 'Arp rate')}>{(['1/4', '1/8', '1/8t', '1/16', '1/16t', '1/32'] as const).map((x) => <option key={x} value={x}>{x}</option>)}</select></label>
              <label>Octaves <select className="input small-select" value={p.arpOctaves} onChange={(e) => set({ arpOctaves: Number(e.target.value) }, 'Arp octaves')}>{[1, 2, 3].map((x) => <option key={x} value={x}>{x}</option>)}</select></label>
              <label>Strum <select className="input small-select" value={p.strumDir} onChange={(e) => set({ strumDir: e.target.value as DreamParams['strumDir'] }, 'Strum direction')}><option value="up">up</option><option value="down">down</option></select></label>
              <label className="dc-check"><input type="checkbox" checked={recQuantize} onChange={(e) => setUI({ recQuantize: e.target.checked })} /> Quantise recording to 1/16</label>
              <div className="dc-edit-knobs">
                {knob('arpGate', 'Gate', 0.05, 1, 0.8, pct)}
                {knob('strum', 'Strum', 0, 400, 0, (v) => `${Math.round(v)} ms`)}
              </div>
            </section>
          </div>
        )}
        <div className="dc-help">
          Click the device, then use <b>1–7</b> for chords, <b>arrow keys</b> for the joystick, <b>H</b> hold, <b>A</b> arp. Recorded notes appear in the
          Piano Roll of this channel. Pressing lower on a pad plays softer.
        </div>
      </div>
    </Window>
  );
}
