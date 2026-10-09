import { useEffect, useRef, useState } from 'react';
import logo from '../../assets/logo.png';
import { engine } from '../../audio/engine';
import { useProject, getProject } from '../../state/store';
import { setUI, toast, ui, useUI, openWindow, type DockTab } from '../../state/ui';
import { formatBBS, formatTime, ticksToSeconds, formatDb } from '../../state/utils';
import { MASTER_ID } from '../../state/types';
import {
  addChannel, addMixerTrack, addPattern, addTrack, clonePattern, removePattern, renameProject, setBpm, setLoop, setTimeSig, updateMixerTrack,
} from '../../state/actions';
import { DRUM_LABELS, SYNTH_PRESETS } from '../../state/defaults';
import type { DrumType } from '../../state/types';
import { Icon } from '../components/Icon';
import { Knob } from '../components/Knob';
import { DragNumber } from '../components/DragNumber';
import { Meter } from '../components/Meter';
import { onFrame, uiLoad } from '../components/frameLoop';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { Oscilloscope } from '../visualizers/Visualizers';
import {
  deleteSelected, doRedo, doSave, doUndo, duplicateSelected, exportWav, selectAll, setMode, stop, toggleMetronome, togglePlay, toggleRecord, showDock,
} from '../commands';
import { exportProjectFile, importProjectFile, newProject, saveProjectAs } from '../../project/manager';
import { pickFile } from '../dialogs/filePick';
import './toolbar.css';
import { askText } from '../dialogs/askText';

function menuAt(e: React.MouseEvent, items: MenuItem[]) {
  const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
  showMenu(r.left, r.bottom + 2, items);
}

function fileMenu(): MenuItem[] {
  return [
    { label: 'New project', shortcut: 'Ctrl+N', onClick: () => { if (!useProject.getState().dirty || confirm('Discard unsaved changes?')) newProject(); } },
    { label: 'Open…', shortcut: 'Ctrl+O', onClick: () => openWindow({ kind: 'projects' }) },
    { separator: true },
    { label: 'Save', shortcut: 'Ctrl+S', onClick: () => void doSave() },
    { label: 'Save as…', shortcut: 'Ctrl+Shift+S', onClick: () => { void askText('Save project as', `${getProject().name} copy`).then((n) => { if (n) void saveProjectAs(n); }); } },
    { separator: true },
    { label: 'Export WAV (song)…', onClick: () => void exportWav('song') },
    { label: 'Export WAV (current pattern)…', onClick: () => void exportWav('pattern') },
    { label: 'Export project file (.goofy.json)', onClick: exportProjectFile },
    { label: 'Import project file…', onClick: () => pickFile('.json,application/json', (f) => void importProjectFile(f)) },
  ];
}

function editMenu(): MenuItem[] {
  const s = useProject.getState();
  return [
    { label: s.past.length ? `Undo ${s.lastAction}` : 'Undo', shortcut: 'Ctrl+Z', disabled: !s.past.length, onClick: doUndo },
    { label: 'Redo', shortcut: 'Ctrl+Shift+Z', disabled: !s.future.length, onClick: doRedo },
    { separator: true },
    { label: 'Select all', shortcut: 'Ctrl+A', onClick: selectAll },
    { label: 'Duplicate selection', shortcut: 'Ctrl+D', onClick: duplicateSelected },
    { label: 'Delete selection', shortcut: 'Del', onClick: deleteSelected },
  ];
}

function addMenu(): MenuItem[] {
  return [
    { label: 'Goofy Synth', submenu: SYNTH_PRESETS.map((p) => ({ label: p.name, onClick: () => setUI({ selectedChannelId: addChannel('synth', { synthPreset: p.name }) }) })) },
    { label: 'Drum sound', submenu: (Object.keys(DRUM_LABELS) as DrumType[]).map((d) => ({ label: DRUM_LABELS[d], onClick: () => setUI({ selectedChannelId: addChannel('drum', { drum: d }) }) })) },
    { label: 'Sampler (empty)', onClick: () => setUI({ selectedChannelId: addChannel('sampler') }) },
    { separator: true },
    { label: 'Pattern', onClick: () => setUI({ selectedPatternId: addPattern() }) },
    { label: 'Playlist track', onClick: () => addTrack() },
    { label: 'Mixer insert', onClick: () => setUI({ selectedMixerId: addMixerTrack() }) },
  ];
}

function patternMenu(): MenuItem[] {
  const p = getProject();
  const sel = ui().selectedPatternId;
  return [
    { label: 'New pattern', onClick: () => setUI({ selectedPatternId: addPattern() }) },
    { label: 'Clone current pattern', onClick: () => { if (sel) setUI({ selectedPatternId: clonePattern(sel) }); } },
    { label: 'Delete current pattern', danger: true, disabled: p.patterns.length <= 1, onClick: () => { if (sel) { removePattern(sel); setUI({ selectedPatternId: getProject().patterns[0]?.id ?? null }); } } },
    { separator: true },
    ...p.patterns.map((pt) => ({ label: pt.name, color: pt.color, checked: pt.id === sel, onClick: () => setUI({ selectedPatternId: pt.id }) })),
  ];
}

function viewMenu(): MenuItem[] {
  const s = ui();
  const tab = (t: DockTab, label: string, key: string): MenuItem => ({ label, shortcut: key, checked: s.dockOpen && s.dockTab === t, onClick: () => showDock(t) });
  return [
    { label: 'Browser', shortcut: 'Alt+F8', checked: s.browserOpen, onClick: () => setUI({ browserOpen: !s.browserOpen }) },
    { separator: true },
    tab('rack', 'Channel rack', 'F6'),
    tab('piano', 'Piano roll', 'F7'),
    tab('mixer', 'Mixer', 'F9'),
    tab('drums', 'Drum machine', 'F10'),
    tab('vocal', 'Recording & vocal tuner', 'F11'),
    tab('analyzer', 'Analyzer', 'F12'),
    { label: 'Hide bottom dock', checked: !s.dockOpen, onClick: () => setUI({ dockOpen: !s.dockOpen }) },
    { separator: true },
    { label: 'Follow playhead', checked: s.followPlayhead, onClick: () => setUI({ followPlayhead: !s.followPlayhead }) },
  ];
}

function optionsMenu(): MenuItem[] {
  const s = ui();
  return [
    { label: 'Settings & themes…', onClick: () => openWindow({ kind: 'settings' }) },
    { label: 'Audio setup wizard…', onClick: () => setUI({ setupDone: false }) },
    { separator: true },
    { label: 'Metronome', checked: s.metronome, onClick: toggleMetronome },
    { label: 'Typing keyboard to piano', checked: s.typingKeyboard, onClick: () => setUI({ typingKeyboard: !s.typingKeyboard }) },
    { label: 'Autosave', checked: s.autosave, onClick: () => setUI({ autosave: !s.autosave }) },
  ];
}

function helpMenu(): MenuItem[] {
  return [
    { label: 'Keyboard shortcuts', onClick: () => openWindow({ kind: 'shortcuts' }) },
    { label: 'About Goofy Studio', onClick: () => openWindow({ kind: 'about' }) },
  ];
}

const MENUS: [string, () => MenuItem[]][] = [
  ['File', fileMenu], ['Edit', editMenu], ['Add', addMenu], ['Patterns', patternMenu], ['View', viewMenu], ['Options', optionsMenu], ['Help', helpMenu],
];

function TimeDisplay() {
  const ref = useRef<HTMLDivElement>(null);
  const [showTime, setShowTime] = useState(false);
  useEffect(() => onFrame(() => {
    if (!ref.current || !engine.transport) return;
    const p = getProject();
    const tick = engine.transport.getTick();
    ref.current.textContent = showTime ? formatTime(ticksToSeconds(tick, p.bpm)) : formatBBS(tick, p.timeSig);
  }), [showTime]);
  return (
    <div className="lcd tb-time" title="Click to toggle bars / time" onClick={() => setShowTime(!showTime)}>
      <div ref={ref} className="tb-time-value">1:01:01</div>
      <div className="tb-time-unit">{showTime ? 'M:S:MS' : 'BAR:BEAT:STEP'}</div>
    </div>
  );
}

function PerfDisplay() {
  const cpu = useRef<HTMLDivElement>(null);
  const voices = useRef<HTMLSpanElement>(null);
  const mem = useRef<HTMLSpanElement>(null);
  useEffect(() => {
    let acc = 0;
    return onFrame((dt) => {
      acc += dt;
      if (acc < 0.25) return;
      acc = 0;
      const load = Math.min(1, uiLoad());
      if (cpu.current) cpu.current.style.transform = `scaleX(${Math.max(0.03, load)})`;
      if (voices.current && engine.initialized) voices.current.textContent = String(engine.graph.voiceCount());
      const pm = (performance as Performance & { memory?: { usedJSHeapSize: number } }).memory;
      if (mem.current) mem.current.textContent = pm ? `${Math.round(pm.usedJSHeapSize / 1048576)} MB` : '—';
    });
  }, []);
  return (
    <div className="tb-perf" title="UI frame load · active voices · memory">
      <div className="tb-perf-row"><Icon name="cpu" size={11} /><div className="tb-perf-bar"><div ref={cpu} /></div></div>
      <div className="tb-perf-row small"><span ref={voices}>0</span> voices · <span ref={mem}>—</span></div>
    </div>
  );
}

function masterLevels() {
  if (!engine.initialized) return null;
  const l = engine.graph.master.levels();
  return { l: l.peakL, r: l.peakR };
}

export function Toolbar() {
  const bpm = useProject((s) => s.project.bpm);
  const timeSig = useProject((s) => s.project.timeSig);
  const name = useProject((s) => s.project.name);
  const dirty = useProject((s) => s.dirty);
  const lastSaved = useProject((s) => s.lastSaved);
  const lastAutosave = useProject((s) => s.lastAutosave);
  const canUndo = useProject((s) => s.past.length > 0);
  const canRedo = useProject((s) => s.future.length > 0);
  const masterVol = useProject((s) => s.project.mixer[0]?.volume ?? 0.8);
  const { playing, recording, mode, metronome, typingKeyboard, dockTab, dockOpen, browserOpen } = useUI();
  const loopOn = useProject((s) => s.project.loop.enabled);

  const status = recording ? 'Recording' : dirty ? 'Unsaved changes' : lastAutosave && (!lastSaved || lastAutosave > lastSaved) ? `Autosaved ${new Date(lastAutosave).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}` : lastSaved ? 'Saved' : 'Not saved yet';

  const dockBtn = (tab: DockTab, icon: string, title: string) => (
    <button className={`btn icon tb-win ${dockOpen && dockTab === tab ? 'on' : ''}`} title={title} onClick={() => (dockOpen && dockTab === tab ? setUI({ dockOpen: false }) : showDock(tab))}>
      <Icon name={icon} />
    </button>
  );

  return (
    <div className="toolbar">
      <div className="tb-left">
        <img src={logo} className="tb-logo" alt="" />
        <div className="tb-menus">
          {MENUS.map(([label, fn]) => (
            <button key={label} className="tb-menu" onClick={(e) => menuAt(e, fn())}>{label}</button>
          ))}
        </div>
      </div>

      <div className="tb-group">
        <div className="tb-mode" title="Pattern / Song mode (L)">
          <button className={mode === 'pattern' ? 'on' : ''} onClick={() => setMode('pattern')}>PAT</button>
          <button className={mode === 'song' ? 'on' : ''} onClick={() => setMode('song')}>SONG</button>
        </div>
        <button className={`btn tb-transport play ${playing ? 'on' : ''}`} title="Play / Pause (Space)" onClick={() => void togglePlay()}>
          <Icon name={playing ? 'pause' : 'play'} size={16} />
        </button>
        <button className="btn tb-transport" title="Stop" onClick={() => void stop()}>
          <Icon name="stop" size={14} />
        </button>
        <button className={`btn tb-transport rec ${recording ? 'recording' : ''}`} title="Record (R)" onClick={() => void toggleRecord()}>
          <Icon name="record" size={14} />
        </button>
      </div>

      <div className="tb-group">
        <div className="tb-field">
          <DragNumber value={bpm} min={20} max={400} decimals={3} step={1} width={86} onChange={setBpm} title="Tempo – drag, scroll or double-click" />
          <span className="label">BPM</span>
        </div>
        <TimeDisplay />
        <div className="tb-field">
          <select
            className="input tb-sig"
            value={`${timeSig[0]}/${timeSig[1]}`}
            onChange={(e) => { const [n, d] = e.target.value.split('/').map(Number); setTimeSig(n, d); }}
            title="Time signature"
          >
            {['2/4', '3/4', '4/4', '5/4', '6/8', '7/8', '12/8'].map((s) => <option key={s}>{s}</option>)}
          </select>
          <span className="label">Sig</span>
        </div>
      </div>

      <div className="tb-group">
        <button className={`btn icon ${metronome ? 'on' : ''}`} title="Metronome" onClick={toggleMetronome}><Icon name="metronome" /></button>
        <button className={`btn icon ${typingKeyboard ? 'on' : ''}`} title="Typing keyboard to piano (Z–M / Q–U rows play notes)" onClick={() => setUI({ typingKeyboard: !typingKeyboard })}><Icon name="keyboard" /></button>
        <button className={`btn icon ${loopOn ? 'on' : ''}`} title="Song loop region (set it by dragging in the playlist ruler)" onClick={() => setLoop({ enabled: !loopOn })}><Icon name="loop" /></button>
      </div>

      <div className="tb-group">
        <Knob
          value={masterVol}
          min={0}
          max={1.25}
          defaultValue={0.85}
          size={30}
          onChange={(v) => updateMixerTrack(MASTER_ID, { volume: v }, 'Master volume')}
          format={formatDb}
          title="Master volume"
        />
        <Meter source={masterLevels} width={12} height={34} />
        <div className="tb-scope"><Oscilloscope className="fill" lineWidth={1.2} /></div>
        <PerfDisplay />
      </div>

      <div className="tb-group">
        <button className="btn icon" title="Undo (Ctrl+Z)" disabled={!canUndo} onClick={doUndo}><Icon name="undo" /></button>
        <button className="btn icon" title="Redo (Ctrl+Shift+Z)" disabled={!canRedo} onClick={doRedo}><Icon name="redo" /></button>
        <button className="btn icon" title="Save (Ctrl+S)" onClick={() => void doSave()}><Icon name="save" /></button>
        <button className="btn icon" title="Open project (Ctrl+O)" onClick={() => openWindow({ kind: 'projects' })}><Icon name="folder" /></button>
      </div>

      <div className="tb-group">
        <button className={`btn icon tb-win ${browserOpen ? 'on' : ''}`} title="Browser" onClick={() => setUI({ browserOpen: !browserOpen })}><Icon name="browser" /></button>
        {dockBtn('rack', 'rack', 'Channel rack (F6)')}
        {dockBtn('piano', 'piano', 'Piano roll (F7)')}
        {dockBtn('mixer', 'mixer', 'Mixer (F9)')}
        {dockBtn('drums', 'drums', 'Drum machine (F10)')}
        {dockBtn('vocal', 'mic', 'Recording & vocal tuner (F11)')}
        {dockBtn('analyzer', 'spectrum', 'Analyzer (F12)')}
      </div>

      <div className="tb-project">
        <input
          className="tb-project-name"
          value={name}
          onChange={(e) => renameProject(e.target.value)}
          onKeyDown={(e) => { e.stopPropagation(); if (e.key === 'Enter') (e.target as HTMLInputElement).blur(); }}
          title="Project name"
          spellCheck={false}
        />
        <div className={`tb-status ${dirty ? 'dirty' : ''} ${recording ? 'rec' : ''}`}>
          <span className="tb-status-dot" />{status}
        </div>
      </div>
    </div>
  );
}

export function useAudioUnlockHint() {
  useEffect(() => {
    const t = setTimeout(() => {
      if (engine.initialized && !engine.running) toast('Click anywhere to enable audio', 'info');
    }, 1500);
    return () => clearTimeout(t);
  }, []);
}
