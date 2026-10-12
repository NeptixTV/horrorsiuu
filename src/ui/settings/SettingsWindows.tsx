// Right-click → "Settings…" windows for single clips and for playlist rows.
import { useEffect, useState } from 'react';
import { useProject, getProject } from '../../state/store';
import { closeWindow, openWindow, setUI, toast, type FloatingWindow } from '../../state/ui';
import type { Clip } from '../../state/types';
import { MASTER_ID, PPQ } from '../../state/types';
import {
  ensureTrackInsert, setOutput, toggleTrackSolo, updateClip, updateMixerTrack, updateTrack,
} from '../../state/actions';
import { clamp, dbToLin, formatPan, linToDb, PALETTE, ticksToSeconds } from '../../state/utils';
import { assets } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { Window } from '../components/Window';
import { Knob } from '../components/Knob';
import { Meter } from '../components/Meter';
import { Icon } from '../components/Icon';
import { FxChainEditor } from '../effects/FxChainEditor';
import './settings.css';

/** Knob + exact numeric entry in dB. */
export function DbControl({ value, onChange, label = 'Volume', min = -60, max = 12 }: { value: number; onChange: (lin: number) => void; label?: string; min?: number; max?: number }) {
  const db = clamp(isFinite(linToDb(value)) ? linToDb(value) : min, min, max);
  const [text, setText] = useState(db.toFixed(1));
  useEffect(() => setText(db.toFixed(1)), [db]);
  const commit = () => {
    const v = parseFloat(text.replace(',', '.'));
    if (isFinite(v)) onChange(v <= min ? 0 : dbToLin(clamp(v, min, max)));
    else setText(db.toFixed(1));
  };
  return (
    <div className="dbc">
      <Knob value={db} min={min} max={max} defaultValue={0} size={40} label={label} onChange={(v) => onChange(v <= min + 0.05 ? 0 : dbToLin(v))} format={(v) => (v <= min + 0.05 ? '-∞ dB' : `${v > 0 ? '+' : ''}${v.toFixed(1)} dB`)} />
      <div className="dbc-input">
        <input
          className="input"
          value={text}
          onChange={(e) => setText(e.target.value)}
          onBlur={commit}
          onKeyDown={(e) => { e.stopPropagation(); if (e.key === 'Enter') commit(); }}
          title="Exact level in dB"
        />
        <span>dB</span>
      </div>
    </div>
  );
}

function ColorRow({ value, onPick }: { value: string; onPick: (c: string) => void }) {
  return (
    <div className="set-colors">
      {PALETTE.map((c) => <button key={c} className={`set-color ${c === value ? 'on' : ''}`} style={{ background: c }} onClick={() => onPick(c)} />)}
    </div>
  );
}

function clipPeak(clip: Clip): number {
  const b = clip.assetId ? (clip.reverse ? assets.getReversed(clip.assetId) : assets.get(clip.assetId)) : undefined;
  if (!b) return 0;
  const p = getProject();
  const speed = clip.speed ?? 1;
  const s0 = Math.floor(clip.offset * b.sampleRate);
  const s1 = Math.min(b.length, s0 + Math.ceil(ticksToSeconds(clip.length, p.bpm) * speed * b.sampleRate));
  let peak = 0;
  for (let c = 0; c < b.numberOfChannels; c++) {
    const d = b.getChannelData(c);
    for (let i = s0; i < s1; i++) {
      const a = Math.abs(d[i]);
      if (a > peak) peak = a;
    }
  }
  return peak;
}

function previewFrom(tick: number) {
  void engine.resume();
  engine.transport.setMode('song');
  engine.transport.setPosition(tick);
  if (!engine.transport.playing) engine.transport.play(tick);
}

export function ClipSettingsWindow({ w, clipId }: { w: FloatingWindow; clipId: string }) {
  const clip = useProject((s) => s.project.clips.find((c) => c.id === clipId));
  const bpm = useProject((s) => s.project.bpm);
  const assetName = useProject((s) => (clip?.assetId ? s.project.assets[clip.assetId]?.name : undefined));
  const pattern = useProject((s) => s.project.patterns.find((p) => p.id === clip?.patternId));
  const track = useProject((s) => s.project.tracks.find((t) => t.id === clip?.trackId));
  useEffect(() => { if (!clip) closeWindow(w.id); }, [clip, w.id]);
  if (!clip) return null;
  const set = (patch: Partial<Clip>, label?: string) => updateClip(clip.id, patch, label);
  const lenSec = ticksToSeconds(clip.length, bpm);
  const name = clip.name ?? (clip.kind === 'audio' ? assetName ?? 'Audio' : pattern?.name ?? 'Pattern');
  const speed = clip.speed ?? 1;
  const semis = 12 * Math.log2(speed);

  return (
    <Window w={w} width={clip.kind === 'audio' ? 620 : 440} color={clip.kind === 'audio' ? '#3fa7a0' : pattern?.color} title={<><b>{name}</b> <span className="crumb">· clip settings · {track?.name}</span></>}>
      <div className="set-body">
        <div className="set-row">
          <label className="set-field grow">
            <span className="label">Name</span>
            <input className="input" value={clip.name ?? ''} placeholder={name} onChange={(e) => set({ name: e.target.value || undefined }, 'Rename clip')} onKeyDown={(e) => e.stopPropagation()} />
          </label>
          <button className="btn" onClick={() => previewFrom(clip.start)} title="Play from the start of this clip"><Icon name="play" size={11} /> Preview</button>
        </div>

        {clip.kind === 'audio' ? (
          <>
            <div className="set-controls">
              <DbControl value={clip.gain ?? 1} onChange={(v) => set({ gain: v }, 'Clip volume')} />
              <Knob value={clip.pan ?? 0} min={-1} max={1} defaultValue={0} size={40} bipolar label="Pan" format={formatPan} onChange={(v) => set({ pan: Math.abs(v) < 0.01 ? 0 : v }, 'Clip pan')} />
              <Knob
                value={speed} min={0.25} max={4} log defaultValue={1} size={40} label="Speed"
                format={(v) => `${v.toFixed(2)}× (${12 * Math.log2(v) >= 0 ? '+' : ''}${(12 * Math.log2(v)).toFixed(1)} st)`}
                onChange={(v) => set({ speed: Math.abs(v - 1) < 0.005 ? 1 : v }, 'Clip speed')}
              />
              <Knob value={(clip.fadeIn ?? 0) * 1000} min={0} max={Math.max(10, Math.min(8000, lenSec * 500))} defaultValue={0} size={40} label="Fade In" format={(v) => `${Math.round(v)} ms`} onChange={(v) => set({ fadeIn: v / 1000 }, 'Fade in')} />
              <Knob value={(clip.fadeOut ?? 0) * 1000} min={0} max={Math.max(10, Math.min(8000, lenSec * 500))} defaultValue={0} size={40} label="Fade Out" format={(v) => `${Math.round(v)} ms`} onChange={(v) => set({ fadeOut: v / 1000 }, 'Fade out')} />
            </div>
            <div className="set-row wrap">
              <button className={`btn small ${clip.reverse ? 'on' : ''}`} onClick={() => {
                const b = clip.assetId ? assets.get(clip.assetId) : undefined;
                const used = lenSec * speed;
                set({ reverse: !clip.reverse, offset: b ? Math.max(0, b.duration - clip.offset - used) : 0 }, 'Reverse clip');
              }}>⇆ Reverse</button>
              <button className="btn small" onClick={() => { const pk = clipPeak(clip) / (clip.gain ?? 1); if (pk > 0) { set({ gain: 0.98 / pk }, 'Normalize'); toast(`Normalized (${linToDb(0.98 / pk).toFixed(1)} dB)`, 'ok'); } }}>Normalize</button>
              <button className="btn small" onClick={() => set({ speed: speed / 2 }, 'Clip speed')}>½× slower</button>
              <button className="btn small" onClick={() => set({ speed: 1 }, 'Clip speed')}>1×</button>
              <button className="btn small" onClick={() => set({ speed: speed * 2 }, 'Clip speed')}>2× faster</button>
              <span className="spacer" />
              <span className="label">{lenSec.toFixed(2)} s · {Math.abs(semis) > 0.05 ? `${semis > 0 ? '+' : ''}${semis.toFixed(1)} st` : 'original pitch'}</span>
              <button className="btn small ghost" onClick={() => set({ gain: 1, pan: 0, speed: 1, reverse: false, fadeIn: 0, fadeOut: 0, fx: [] }, 'Reset clip')}>Reset all</button>
            </div>
            <FxChainEditor target={{ kind: 'clip', id: clip.id }} effects={clip.fx ?? []} title="Effects on this clip" />
            <p className="set-hint">Tip: change any knob while the song is playing – you hear it instantly. “Voice” presets change the pitch without changing the speed.</p>
          </>
        ) : (
          <>
            <div className="set-controls">
              <Knob value={clip.gain ?? 1} min={0} max={2} defaultValue={1} size={40} label="Velocity" format={(v) => `${Math.round(v * 100)}%`} onChange={(v) => set({ gain: v }, 'Clip velocity')} />
              <Knob value={clip.transpose ?? 0} min={-24} max={24} step={1} defaultValue={0} size={40} bipolar label="Transpose" format={(v) => `${v > 0 ? '+' : ''}${Math.round(v)} st`} onChange={(v) => set({ transpose: Math.round(v) }, 'Clip transpose')} />
            </div>
            <p className="set-hint">Transpose shifts every note of this clip (drums change their tuning). Effects for instruments live on their mixer channels – open the row settings to edit them.</p>
            {track && <button className="btn" onClick={() => openTrackSettings(track.id)}><Icon name="settings" size={11} /> Row settings “{track.name}”</button>}
          </>
        )}
      </div>
    </Window>
  );
}

export function openTrackSettings(trackId: string) {
  ensureTrackInsert(trackId);
  openWindow({ kind: 'track', trackId });
}

export function openClipSettings(clipId: string) {
  openWindow({ kind: 'clip', clipId });
}

export function TrackSettingsWindow({ w, trackId }: { w: FloatingWindow; trackId: string }) {
  const track = useProject((s) => s.project.tracks.find((t) => t.id === trackId));
  const mixer = useProject((s) => s.project.mixer);
  const clips = useProject((s) => s.project.clips);
  const channels = useProject((s) => s.project.channels);
  const patterns = useProject((s) => s.project.patterns);
  useEffect(() => { if (!track) closeWindow(w.id); }, [track, w.id]);
  if (!track) return null;
  const insert = mixer.find((m) => m.id === track.mixerTrackId);
  const own = insert && insert.id !== MASTER_ID;
  const rowClips = clips.filter((c) => c.trackId === track.id);
  const usedChannels = new Set<string>();
  for (const c of rowClips) {
    const p = patterns.find((x) => x.id === c.patternId);
    if (!p) continue;
    for (const [cid, steps] of Object.entries(p.steps)) if (steps.some((v) => v > 0)) usedChannels.add(cid);
    for (const [cid, notes] of Object.entries(p.notes)) if (notes.length) usedChannels.add(cid);
  }
  const instruments = channels.filter((c) => usedChannels.has(c.id));
  const levels = () => {
    if (!engine.initialized || !insert) return null;
    const l = engine.graph.strips.get(insert.id)?.levels();
    return l ? { l: l.peakL, r: l.peakR } : null;
  };

  return (
    <Window w={w} width={620} color={insert?.color ?? track.color} title={<><b>{track.name}</b> <span className="crumb">· row settings</span></>}>
      <div className="set-body">
        <div className="set-row">
          <label className="set-field grow">
            <span className="label">Row name</span>
            <input className="input" value={track.name} onChange={(e) => updateTrack(track.id, { name: e.target.value }, 'Rename track')} onKeyDown={(e) => e.stopPropagation()} />
          </label>
          <button className={`toggle-ms mute ${track.mute ? 'on' : ''}`} style={{ width: 28, height: 24 }} onClick={() => updateTrack(track.id, { mute: !track.mute }, 'Mute track')}>M</button>
          <button className={`toggle-ms solo ${track.solo ? 'on' : ''}`} style={{ width: 28, height: 24 }} onClick={() => toggleTrackSolo(track.id)}>S</button>
        </div>
        <ColorRow value={track.color} onPick={(c) => updateTrack(track.id, { color: c }, 'Track colour')} />
        {own && insert ? (
          <>
            <div className="set-controls">
              <Meter source={levels} width={14} height={70} />
              <DbControl value={insert.volume} max={6} onChange={(v) => updateMixerTrack(insert.id, { volume: Math.min(v, 1.995) }, 'Row volume')} />
              <Knob value={insert.pan} min={-1} max={1} defaultValue={0} size={40} bipolar label="Pan" format={formatPan} onChange={(v) => updateMixerTrack(insert.id, { pan: v }, 'Row pan')} />
              <label className="set-field">
                <span className="label">Mixer channel</span>
                <span className="set-mixer-name" style={{ borderColor: insert.color }}>{mixer.indexOf(insert)} · {insert.name}</span>
              </label>
              <label className="set-field">
                <span className="label">Output</span>
                <select className="input small-select" value={insert.output ?? MASTER_ID} onChange={(e) => { if (!setOutput(insert.id, e.target.value)) toast('That would create a feedback loop', 'error'); }}>
                  {mixer.filter((m) => m.id !== insert.id).map((m) => <option key={m.id} value={m.id}>{m.id === MASTER_ID ? 'Master' : `${mixer.indexOf(m)} ${m.name}`}</option>)}
                </select>
              </label>
            </div>
            <FxChainEditor target={{ kind: 'mixer', id: insert.id }} effects={insert.inserts} title="Effects on this row (audio clips)" />
          </>
        ) : (
          <button className="btn primary" onClick={() => ensureTrackInsert(track.id)}>Give this row its own mixer channel</button>
        )}
        {instruments.length > 0 && (
          <div className="set-instruments">
            <span className="label">Instruments playing in this row (each has its own mixer channel)</span>
            {instruments.map((c) => {
              const m = mixer.find((x) => x.id === c.mixerTrackId);
              return (
                <div key={c.id} className="set-inst">
                  <span className="set-inst-color" style={{ background: c.color }} />
                  <span className="grow">{c.name}</span>
                  <span className="label">{m && m.id !== MASTER_ID ? `${mixer.indexOf(m)} · ${m.name}` : 'Master'}</span>
                  <button className="btn small" onClick={() => setUI({ dockTab: 'mixer', dockOpen: true, selectedMixerId: c.mixerTrackId })}>Mixer</button>
                  <button className="btn small" onClick={() => openWindow({ kind: 'channel', channelId: c.id })}>Sound</button>
                </div>
              );
            })}
          </div>
        )}
        <p className="set-hint">{rowClips.length} clip{rowClips.length === 1 ? '' : 's'} in this row · audio clips play through this row's mixer channel · {Math.round((rowClips.reduce((n, c) => Math.max(n, c.start + c.length), 0) / PPQ / 4) * 10) / 10} bars</p>
      </div>
    </Window>
  );
}
