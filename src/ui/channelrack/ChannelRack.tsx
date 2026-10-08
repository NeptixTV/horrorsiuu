import { memo, useEffect, useRef, useState, type DragEvent } from 'react';
import { useProject, beginGesture, endGesture, getProject } from '../../state/store';
import { setUI, ui, useUI, openWindow, toast } from '../../state/ui';
import type { Channel, Note } from '../../state/types';
import {
  addChannel, addPattern, clonePattern, duplicateChannel, fillSteps, moveChannel, removeChannel, setStep, setSwing, shiftSteps,
  toggleChannelMute, toggleChannelSolo, updateChannel, updatePattern, updateSampler,
} from '../../state/actions';
import { DRUM_LABELS, SYNTH_PRESETS } from '../../state/defaults';
import type { DrumType } from '../../state/types';
import { formatPan, formatDb } from '../../state/utils';
import { patternLength } from '../../audio/sequencer';
import { engine } from '../../audio/engine';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { colorMenu } from '../components/ColorPicker';
import { onFrame } from '../components/frameLoop';
import { readDragPayload } from '../browser/Browser';
import { addDrumKit } from '../browser/Browser';
import { decodeAudioFile } from '../../project/manager';
import { currentStepFor } from './playhead';
import './rack.css';

// paint-drag state shared by all step buttons
let paint: { patternId: string; channelId: string; value: number } | null = null;
window.addEventListener('pointerup', () => {
  if (paint) {
    paint = null;
    endGesture();
  }
});

function addChannelMenu(): MenuItem[] {
  return [
    { label: 'Goofy Synth', submenu: SYNTH_PRESETS.map((p) => ({ label: p.name, onClick: () => setUI({ selectedChannelId: addChannel('synth', { synthPreset: p.name }) }) })) },
    { label: 'Drum sound', submenu: (Object.keys(DRUM_LABELS) as DrumType[]).map((d) => ({ label: DRUM_LABELS[d], onClick: () => setUI({ selectedChannelId: addChannel('drum', { drum: d }) }) })) },
    { label: 'Drum kit (7 channels)', onClick: addDrumKit },
    { label: 'Sampler', onClick: () => setUI({ selectedChannelId: addChannel('sampler') }) },
  ];
}

const StepButton = memo(function StepButton({ i, value, color, patternId, channelId }: { i: number; value: number; color: string; patternId: string; channelId: string }) {
  return (
    <button
      className={`step ${value > 0 ? 'on' : ''} ${Math.floor(i / 4) % 2 ? 'alt' : ''}`}
      data-i={i}
      style={value > 0 ? { ['--sc' as string]: color, opacity: 0.45 + value * 0.55 } : undefined}
      onPointerDown={(e) => {
        e.preventDefault();
        (e.currentTarget as Element).releasePointerCapture(e.pointerId);
        beginGesture();
        const v = e.button === 2 ? 0 : value > 0 ? 0 : 1;
        paint = { patternId, channelId, value: v };
        setStep(patternId, channelId, i, v);
        if (v > 0) engine.previewNote(channelId, getProject().channels.find((c) => c.id === channelId)?.rootNote ?? 60, 0.8, 0.15);
      }}
      onPointerEnter={() => {
        if (paint && paint.patternId === patternId && paint.channelId === channelId) setStep(patternId, channelId, i, paint.value);
      }}
      onContextMenu={(e) => e.preventDefault()}
      onWheel={(e) => {
        if (value <= 0) return;
        const nv = Math.max(0.05, Math.min(1, value + (e.deltaY < 0 ? 0.1 : -0.1)));
        setStep(patternId, channelId, i, Math.round(nv * 100) / 100);
      }}
      title={value > 0 ? `Velocity ${Math.round(value * 100)}% (scroll to change)` : undefined}
    />
  );
});

function MiniNotes({ notes, length, color, onOpen }: { notes: Note[]; length: number; color: string; onOpen: () => void }) {
  const min = Math.min(...notes.map((n) => n.pitch));
  const max = Math.max(...notes.map((n) => n.pitch));
  const range = Math.max(8, max - min + 1);
  return (
    <div className="mini-notes" onClick={onOpen} title="Open in piano roll">
      {notes.map((n) => (
        <div
          key={n.id}
          className="mini-note"
          style={{
            left: `${(n.start / length) * 100}%`,
            width: `${Math.max(0.6, (n.length / length) * 100)}%`,
            top: `${((max - n.pitch) / range) * 100}%`,
            height: `${Math.max(6, 100 / range)}%`,
            background: color,
          }}
        />
      ))}
    </div>
  );
}

const ChannelRow = memo(function ChannelRow({
  channel, steps, notes, lengthSteps, patternLen, patternId, selected, mixerIndex, mixerName,
}: {
  channel: Channel; steps: number[] | undefined; notes: Note[] | undefined; lengthSteps: number; patternLen: number; patternId: string;
  selected: boolean; mixerIndex: number; mixerName: string;
}) {
  const [drop, setDrop] = useState(false);
  const levelRef = useRef<HTMLDivElement>(null);
  useEffect(() => onFrame(() => {
    const h = engine.initialized ? engine.graph.channels.get(channel.id) : undefined;
    if (levelRef.current) levelRef.current.style.opacity = h ? String(Math.min(1, h.level() * 3)) : '0';
  }), [channel.id]);

  const openEditor = () => openWindow({ kind: 'channel', channelId: channel.id });
  const menu = (e: React.MouseEvent) => {
    e.preventDefault();
    const p = getProject();
    showMenu(e.clientX, e.clientY, [
      { label: 'Open editor', onClick: openEditor },
      { label: 'Piano roll', onClick: () => setUI({ pianoChannelId: channel.id, selectedChannelId: channel.id, dockTab: 'piano' }) },
      { label: 'Rename…', onClick: () => { const n = prompt('Channel name', channel.name); if (n) updateChannel(channel.id, { name: n }, 'Rename channel'); } },
      { label: 'Colour', submenu: colorMenu(channel.color, (c) => updateChannel(channel.id, { color: c }, 'Channel colour')) },
      { label: 'Route to mixer insert', submenu: p.mixer.map((m, i) => ({ label: i === 0 ? 'Master' : `${i} · ${m.name}`, checked: m.id === channel.mixerTrackId, onClick: () => updateChannel(channel.id, { mixerTrackId: m.id }, 'Route channel') })) },
      { separator: true },
      { label: 'Fill every 2 steps', onClick: () => fillSteps(patternId, channel.id, 2) },
      { label: 'Fill every 4 steps', onClick: () => fillSteps(patternId, channel.id, 4) },
      { label: 'Fill every 8 steps', onClick: () => fillSteps(patternId, channel.id, 8) },
      { label: 'Shift steps right', onClick: () => shiftSteps(patternId, channel.id, 1) },
      { label: 'Shift steps left', onClick: () => shiftSteps(patternId, channel.id, -1) },
      { label: 'Clear steps', onClick: () => fillSteps(patternId, channel.id, 0) },
      { separator: true },
      { label: 'Move up', onClick: () => moveChannel(channel.id, -1) },
      { label: 'Move down', onClick: () => moveChannel(channel.id, 1) },
      { label: 'Duplicate', onClick: () => duplicateChannel(channel.id) },
      { label: 'Delete', danger: true, onClick: () => removeChannel(channel.id) },
    ]);
  };

  const onDrop = async (e: DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setDrop(false);
    const file = Array.from(e.dataTransfer.files ?? [])[0];
    let assetId: string | null = null;
    let name = '';
    if (file) {
      const res = await decodeAudioFile(file);
      if (res) { assetId = res.id; name = res.name; }
    } else {
      const p = readDragPayload(e);
      setUI({ dragPayload: null });
      if (p?.type === 'sample') { assetId = p.assetId; name = p.name; }
      else if (p?.type === 'synthPreset' && channel.kind === 'synth') {
        const preset = SYNTH_PRESETS.find((x) => x.name === p.preset);
        if (preset) { updateChannel(channel.id, { synth: structuredClone(preset.params), name: preset.name }, 'Load preset'); toast(`Loaded ${preset.name}`, 'ok'); }
        return;
      } else {
        // let the rack handle it (adds a new channel)
        return rackDrop(e);
      }
    }
    if (!assetId) return;
    if (channel.kind === 'sampler') {
      updateSampler(channel.id, { assetId });
      updateChannel(channel.id, { name }, 'Load sample');
    } else {
      updateChannel(channel.id, { kind: 'sampler', sampler: { assetId, rootNote: 60, tune: 0, attack: 0.002, release: 0.1, reverse: false, level: 0.9 }, name }, 'Replace with sample');
    }
    toast(`Loaded “${name}” into ${channel.name}`, 'ok');
  };

  const n = lengthSteps;
  const hasNotes = !!notes?.length;
  return (
    <div
      className={`rack-row ${selected ? 'selected' : ''} ${channel.mute ? 'muted' : ''} ${drop ? 'drop-target' : ''}`}
      onDragOver={(e) => { e.preventDefault(); setDrop(true); }}
      onDragLeave={() => setDrop(false)}
      onDrop={onDrop}
    >
      <span className={`led ${channel.mute ? '' : 'on'}`} title="Mute (M) · right-click: solo" onClick={() => toggleChannelMute(channel.id)} onContextMenu={(e) => { e.preventDefault(); toggleChannelSolo(channel.id); }} />
      <button className={`toggle-ms solo ${channel.solo ? 'on' : ''}`} title="Solo (S)" onClick={() => toggleChannelSolo(channel.id)}>S</button>
      <Knob value={channel.pan} min={-1} max={1} defaultValue={0} size={22} bipolar onChange={(v) => updateChannel(channel.id, { pan: v }, 'Pan')} format={formatPan} title="Pan" />
      <Knob value={channel.volume} min={0} max={1.25} defaultValue={0.8} size={22} onChange={(v) => updateChannel(channel.id, { volume: v }, 'Volume')} format={formatDb} title="Volume" />
      <button
        className="rack-mixer"
        title={`Mixer insert: ${mixerName}`}
        onClick={(e) => {
          const p = getProject();
          showMenu(e.clientX, e.clientY, p.mixer.map((m, i) => ({ label: i === 0 ? 'Master' : `${i} · ${m.name}`, checked: m.id === channel.mixerTrackId, onClick: () => updateChannel(channel.id, { mixerTrackId: m.id }, 'Route channel') })));
        }}
      >
        {mixerIndex === 0 ? '–' : mixerIndex}
      </button>
      <button
        className="rack-name"
        style={{ ['--cc' as string]: channel.color }}
        onClick={() => { setUI({ selectedChannelId: channel.id, pianoChannelId: channel.id }); engine.previewNote(channel.id, channel.rootNote, 0.8, 0.3); }}
        onDoubleClick={openEditor}
        onContextMenu={menu}
        title={`${channel.name} (${channel.kind}) – click to select, double-click to edit`}
      >
        <span className="rack-name-level" ref={levelRef} />
        <span className="rack-name-text">{channel.name}</span>
        <Icon name={channel.kind === 'drum' ? 'drums' : channel.kind === 'sampler' ? 'wave' : 'synth'} size={10} style={{ opacity: 0.6 }} />
      </button>
      {hasNotes && !steps?.some((v) => v > 0) ? (
        <MiniNotes notes={notes!} length={patternLen} color={channel.color} onOpen={() => setUI({ pianoChannelId: channel.id, selectedChannelId: channel.id, dockTab: 'piano' })} />
      ) : (
        <div className="steps" data-channel={channel.id}>
          {Array.from({ length: n }, (_, i) => (
            <StepButton key={i} i={i} value={steps?.[i] ?? 0} color={channel.color} patternId={patternId} channelId={channel.id} />
          ))}
        </div>
      )}
    </div>
  );
});

async function rackDrop(e: DragEvent) {
  e.preventDefault();
  const files = Array.from(e.dataTransfer.files ?? []);
  for (const f of files) {
    const res = await decodeAudioFile(f);
    if (res) setUI({ selectedChannelId: addChannel('sampler', { assetId: res.id, name: res.name }) });
  }
  if (files.length) return;
  const p = readDragPayload(e);
  setUI({ dragPayload: null });
  if (!p) return;
  if (p.type === 'instrument') setUI({ selectedChannelId: addChannel(p.kind) });
  else if (p.type === 'drum') {
    if (p.drum === 'kit') addDrumKit();
    else setUI({ selectedChannelId: addChannel('drum', { drum: p.drum as DrumType }) });
  } else if (p.type === 'synthPreset') setUI({ selectedChannelId: addChannel('synth', { synthPreset: p.preset }) });
  else if (p.type === 'sample') setUI({ selectedChannelId: addChannel('sampler', { assetId: p.assetId, name: p.name }) });
}

export function ChannelRack() {
  const channels = useProject((s) => s.project.channels);
  const patterns = useProject((s) => s.project.patterns);
  const mixer = useProject((s) => s.project.mixer);
  const swing = useProject((s) => s.project.swing);
  const timeSig = useProject((s) => s.project.timeSig);
  const selectedPatternId = useUI((s) => s.selectedPatternId);
  const selectedChannelId = useUI((s) => s.selectedChannelId);
  const pattern = patterns.find((p) => p.id === selectedPatternId) ?? patterns[0];
  const rackRef = useRef<HTMLDivElement>(null);
  const [drop, setDrop] = useState(false);

  useEffect(() => {
    if (pattern && pattern.id !== selectedPatternId) setUI({ selectedPatternId: pattern.id });
  }, [pattern, selectedPatternId]);

  // playhead column highlight (direct DOM, no re-render)
  useEffect(() => {
    let last = -1;
    return onFrame(() => {
      const root = rackRef.current;
      if (!root) return;
      const step = engine.transport?.playing ? currentStepFor(ui().selectedPatternId) : -1;
      if (step === last) return;
      root.querySelectorAll('.step.playing').forEach((el) => el.classList.remove('playing'));
      if (step >= 0) root.querySelectorAll(`.step[data-i="${step}"]`).forEach((el) => el.classList.add('playing'));
      last = step;
    });
  }, []);

  if (!pattern) return null;
  const mixerIdx = new Map(mixer.map((m, i) => [m.id, i]));
  const plen = patternLength(pattern, timeSig);
  return (
    <div className="rack" onPointerDown={() => setUI({ focus: 'rack' })}>
      <div className="rack-head">
        <select className="input" value={pattern.id} onChange={(e) => { setUI({ selectedPatternId: e.target.value }); if (engine.transport) engine.transport.patternId = e.target.value; }} title="Pattern">
          {patterns.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
        </select>
        <button className="btn icon small" title="New pattern" onClick={() => { const id = addPattern(); setUI({ selectedPatternId: id }); }}><Icon name="plus" size={11} /></button>
        <button className="btn icon small" title="Clone pattern" onClick={() => setUI({ selectedPatternId: clonePattern(pattern.id) })}><Icon name="copy" size={11} /></button>
        <span className="label" style={{ marginLeft: 8 }}>Steps</span>
        <select className="input" value={pattern.lengthSteps} onChange={(e) => updatePattern(pattern.id, { lengthSteps: Number(e.target.value) })}>
          {[8, 12, 16, 24, 32, 48, 64].map((n) => <option key={n} value={n}>{n}</option>)}
        </select>
        <Knob value={swing} min={0} max={1} defaultValue={0} size={24} onChange={setSwing} format={(v) => `${Math.round(v * 100)}%`} title="Swing" />
        <span className="label">Swing</span>
        <span className="spacer" />
        <span className="label">{channels.length} channels · drag sounds here</span>
        <button className="btn small" onClick={(e) => { const r = (e.currentTarget as HTMLElement).getBoundingClientRect(); showMenu(r.left, r.bottom + 2, addChannelMenu()); }}>
          <Icon name="plus" size={11} /> Add channel
        </button>
      </div>
      <div
        ref={rackRef}
        className={`rack-rows scroll-y ${drop ? 'drop-target' : ''}`}
        onDragOver={(e) => { e.preventDefault(); setDrop(true); }}
        onDragLeave={(e) => { if (e.currentTarget === e.target) setDrop(false); }}
        onDrop={(e) => { setDrop(false); void rackDrop(e); }}
      >
        {channels.map((c) => (
          <ChannelRow
            key={c.id}
            channel={c}
            steps={pattern.steps[c.id]}
            notes={pattern.notes[c.id]}
            lengthSteps={pattern.lengthSteps}
            patternLen={plen}
            patternId={pattern.id}
            selected={c.id === selectedChannelId}
            mixerIndex={mixerIdx.get(c.mixerTrackId) ?? 0}
            mixerName={mixer.find((m) => m.id === c.mixerTrackId)?.name ?? 'Master'}
          />
        ))}
        <div className="rack-add" onClick={(e) => showMenu(e.clientX, e.clientY, addChannelMenu())}><Icon name="plus" size={12} /> Add channel / drop instruments & samples here</div>
      </div>
    </div>
  );
}

