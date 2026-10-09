import { memo, useState, type DragEvent } from 'react';
import { useProject, getProject } from '../../state/store';
import { setUI, toast, useUI, openWindow } from '../../state/ui';
import type { EffectType, MixerTrack } from '../../state/types';
import { MASTER_ID } from '../../state/types';
import {
  addInsert, addMixerTrack, addSend, moveInsert, removeInsert, removeMixerTrack, removeSend, setOutput, toggleBypass, toggleMixerMute,
  toggleMixerSolo, updateMixerTrack, updateSend,
} from '../../state/actions';
import { EFFECTS, EFFECT_PRESETS, EFFECT_TYPES } from '../../audio/effects/descriptors';
import { engine } from '../../audio/engine';
import { formatDb, formatPan, linToDb } from '../../state/utils';
import { wouldCycle } from '../../state/routing';
import { Fader } from '../components/Fader';
import { Knob } from '../components/Knob';
import { Meter } from '../components/Meter';
import { Icon } from '../components/Icon';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { colorMenu } from '../components/ColorPicker';
import { readDragPayload } from '../browser/Browser';
import { applyVocalChain } from '../browser/vocalChains';
import './mixer.css';
import { askText } from '../dialogs/askText';

function levelsOf(id: string) {
  return () => {
    if (!engine.initialized) return null;
    const s = engine.graph.strips.get(id);
    if (!s) return null;
    const l = s.levels();
    return { l: l.peakL, r: l.peakR };
  };
}

function effectMenu(trackId: string, index?: number): MenuItem[] {
  return EFFECT_TYPES.map((t) => ({
    label: EFFECTS[t].name,
    color: EFFECTS[t].color,
    submenu: [
      { label: 'Default', onClick: () => openFx(trackId, addInsert(trackId, t, undefined, index)) },
      ...EFFECT_PRESETS.filter((p) => p.type === t).map((p) => ({ label: p.name, onClick: () => openFx(trackId, addInsert(trackId, t, p.params, index)) })),
    ],
  }));
}

function openFx(trackId: string, effectId: string) {
  openWindow({ kind: 'effect', trackId, effectId });
}

function handleEffectDrop(e: DragEvent, trackId: string, index?: number) {
  e.preventDefault();
  e.stopPropagation();
  const p = readDragPayload(e);
  setUI({ dragPayload: null });
  if (!p || p.type !== 'effect') return false;
  if (p.preset?.startsWith('chain:')) {
    applyVocalChain(p.preset.slice(6));
    return true;
  }
  const preset = EFFECT_PRESETS.find((x) => x.name === p.preset);
  addInsert(trackId, p.effect as EffectType, preset?.params, index);
  toast(`${preset?.name ?? EFFECTS[p.effect].name} → ${getProject().mixer.find((m) => m.id === trackId)?.name}`, 'ok');
  return true;
}

const Strip = memo(function Strip({ track, index, selected }: { track: MixerTrack; index: number; selected: boolean }) {
  const [drop, setDrop] = useState(false);
  const isMaster = track.id === MASTER_ID;
  const db = linToDb(track.volume);
  const menu = (e: React.MouseEvent) => {
    e.preventDefault();
    const p = getProject();
    showMenu(e.clientX, e.clientY, [
      { label: 'Rename…', onClick: () => { void askText('Insert name', track.name).then((n) => { if (n) updateMixerTrack(track.id, { name: n }, 'Rename insert'); }); } },
      { label: 'Colour', submenu: colorMenu(track.color, (c) => updateMixerTrack(track.id, { color: c }, 'Insert colour')) },
      { label: 'Add effect', submenu: effectMenu(track.id) },
      ...(!isMaster ? [
        { label: 'Route output to', submenu: p.mixer.filter((m) => m.id !== track.id).map((m) => ({ label: m.id === MASTER_ID ? 'Master' : `${p.mixer.indexOf(m)} · ${m.name}`, checked: track.output === m.id, disabled: m.id !== MASTER_ID && wouldCycle(p.mixer.map((x) => (x.id === track.id ? { ...x, output: null } : x)), track.id, m.id), onClick: () => setOutput(track.id, m.id) })) },
        { label: 'Use as vocal / mic input insert', checked: engine.inputTrackId === track.id, onClick: () => { engine.inputTrackId = track.id; engine.sync(getProject()); toast(`Mic monitoring → ${track.name}`, 'ok'); } },
        { separator: true },
        { label: 'Reset fader', onClick: () => updateMixerTrack(track.id, { volume: 0.8, pan: 0 }, 'Reset') },
        { label: 'Delete insert', danger: true, onClick: () => removeMixerTrack(track.id) },
      ] as MenuItem[] : [{ label: 'Reset fader', onClick: () => updateMixerTrack(track.id, { volume: 0.85, pan: 0 }, 'Reset') }]),
    ]);
  };
  return (
    <div
      className={`mx-strip ${isMaster ? 'master' : ''} ${selected ? 'selected' : ''} ${drop ? 'drop-target' : ''}`}
      style={{ ['--tc' as string]: track.color }}
      onPointerDown={() => setUI({ selectedMixerId: track.id, focus: 'mixer' })}
      onContextMenu={menu}
      onDragOver={(e) => { e.preventDefault(); setDrop(true); }}
      onDragLeave={() => setDrop(false)}
      onDrop={(e) => { setDrop(false); if (handleEffectDrop(e, track.id)) setUI({ selectedMixerId: track.id }); }}
    >
      <div className="mx-top">
        <span className="mx-num">{isMaster ? 'M' : index}</span>
        <span className="mx-name" title={track.name} onDoubleClick={() => { void askText('Insert name', track.name).then((n) => { if (n) updateMixerTrack(track.id, { name: n }, 'Rename insert'); }); }}>{track.name}</span>
      </div>
      <div className="mx-fx-dots" title={track.inserts.map((i) => EFFECTS[i.type].name).join(', ') || 'No effects'}>
        {track.inserts.slice(0, 8).map((fx) => <span key={fx.id} className={`mx-dot ${fx.bypass ? 'off' : ''}`} style={{ background: EFFECTS[fx.type].color }} />)}
      </div>
      <Knob value={track.pan} min={-1} max={1} defaultValue={0} size={26} bipolar onChange={(v) => updateMixerTrack(track.id, { pan: v }, 'Pan')} format={formatPan} title="Pan" />
      <div className="mx-fader-area">
        <Meter source={levelsOf(track.id)} width={isMaster ? 18 : 12} height={150} />
        <Fader value={track.volume} defaultValue={isMaster ? 0.85 : 0.8} height={150} onChange={(v) => updateMixerTrack(track.id, { volume: v }, 'Volume')} title={formatDb(track.volume)} />
      </div>
      <div className="mx-db">{isFinite(db) ? db.toFixed(1) : '-∞'}</div>
      <div className="mx-ms">
        <button className={`toggle-ms mute ${track.mute ? 'on' : ''}`} onClick={() => toggleMixerMute(track.id)} title="Mute">M</button>
        {!isMaster && <button className={`toggle-ms solo ${track.solo ? 'on' : ''}`} onClick={(e) => toggleMixerSolo(track.id, e.ctrlKey || e.metaKey)} title="Solo (Ctrl: exclusive)">S</button>}
      </div>
      <div className="mx-out" title="Output">{isMaster ? 'OUT' : track.output === MASTER_ID ? '→ M' : `→ ${getProject().mixer.findIndex((m) => m.id === track.output)}`}</div>
    </div>
  );
});

function InsertPanel({ track }: { track: MixerTrack }) {
  const mixer = useProject((s) => s.project.mixer);
  const [dropIdx, setDropIdx] = useState<number | null>(null);
  const isMaster = track.id === MASTER_ID;
  const slots = Math.max(8, track.inserts.length + 1);
  const sendTargets = mixer.filter((m) => m.id !== track.id && m.id !== MASTER_ID && !track.sends.some((s) => s.target === m.id) && !wouldCycle(mixer, track.id, m.id));
  return (
    <div className="mx-panel">
      <div className="mx-panel-head" style={{ borderTopColor: track.color }}>
        <span className="mx-panel-name">{isMaster ? 'Master' : `${mixer.indexOf(track)} · ${track.name}`}</span>
        <span className="spacer" />
        {!isMaster && (
          <select className="input small-select" value={track.output ?? MASTER_ID} onChange={(e) => { if (!setOutput(track.id, e.target.value)) toast('That routing would create a feedback loop', 'error'); }} title="Output routing">
            {mixer.filter((m) => m.id !== track.id).map((m) => <option key={m.id} value={m.id}>Out → {m.id === MASTER_ID ? 'Master' : `${mixer.indexOf(m)} ${m.name}`}</option>)}
          </select>
        )}
      </div>
      <div className="label mx-section">Insert effects</div>
      <div className="mx-slots">
        {Array.from({ length: slots }, (_, i) => {
          const fx = track.inserts[i];
          return (
            <div
              key={fx?.id ?? `empty${i}`}
              className={`mx-slot ${fx ? 'filled' : ''} ${dropIdx === i ? 'drop-target' : ''}`}
              style={fx ? { ['--fc' as string]: EFFECTS[fx.type].color } : undefined}
              onDragOver={(e) => { e.preventDefault(); setDropIdx(i); }}
              onDragLeave={() => setDropIdx(null)}
              onDrop={(e) => { setDropIdx(null); handleEffectDrop(e, track.id, i); }}
              onClick={(e) => { if (!fx) showMenu(e.clientX, e.clientY, effectMenu(track.id, i)); }}
              onDoubleClick={() => fx && openFx(track.id, fx.id)}
            >
              <span className="mx-slot-n">{i + 1}</span>
              {fx ? (
                <>
                  <span className={`led ${fx.bypass ? '' : 'on'}`} title={fx.bypass ? 'Enable' : 'Bypass'} onClick={(e) => { e.stopPropagation(); toggleBypass(track.id, fx.id); }} />
                  <span className="mx-slot-name" onClick={() => openFx(track.id, fx.id)}>{EFFECTS[fx.type].name}</span>
                  <button className="btn ghost icon small" title="Move up" onClick={(e) => { e.stopPropagation(); moveInsert(track.id, fx.id, -1); }}><Icon name="chevronUp" size={10} /></button>
                  <button className="btn ghost icon small" title="Move down" onClick={(e) => { e.stopPropagation(); moveInsert(track.id, fx.id, 1); }}><Icon name="chevronDown" size={10} /></button>
                  <button className="btn ghost icon small danger" title="Remove" onClick={(e) => { e.stopPropagation(); removeInsert(track.id, fx.id); }}><Icon name="close" size={10} /></button>
                </>
              ) : (
                <span className="mx-slot-empty">(empty – click or drop effect)</span>
              )}
            </div>
          );
        })}
      </div>
      {!isMaster && (
        <>
          <div className="label mx-section">Sends</div>
          <div className="mx-sends">
            {track.sends.map((s) => {
              const target = mixer.find((m) => m.id === s.target);
              return (
                <div key={s.id} className="mx-send">
                  <Knob value={s.amount} min={0} max={1} defaultValue={0.3} size={24} onChange={(v) => updateSend(track.id, s.id, v)} format={(v) => `${Math.round(v * 100)}%`} title="Send level" />
                  <span className="mx-send-name" style={{ color: target?.color }}>→ {target ? `${mixer.indexOf(target)} ${target.name}` : '?'}</span>
                  <button className="btn ghost icon small danger" onClick={() => removeSend(track.id, s.id)}><Icon name="close" size={10} /></button>
                </div>
              );
            })}
            {sendTargets.length > 0 && (
              <select className="input small-select" value="" onChange={(e) => { if (e.target.value && !addSend(track.id, e.target.value)) toast('Feedback loop prevented', 'error'); }}>
                <option value="">+ Add send…</option>
                {sendTargets.map((m) => <option key={m.id} value={m.id}>{mixer.indexOf(m)} · {m.name}</option>)}
              </select>
            )}
          </div>
        </>
      )}
    </div>
  );
}

export function Mixer() {
  const mixer = useProject((s) => s.project.mixer);
  const selectedId = useUI((s) => s.selectedMixerId);
  const selected = mixer.find((m) => m.id === selectedId) ?? mixer[0];
  return (
    <div className="mixer" onPointerDown={() => setUI({ focus: 'mixer' })}>
      <div className="mx-strips">
        {mixer.map((t, i) => <Strip key={t.id} track={t} index={i} selected={t.id === selected.id} />)}
        <button className="mx-add" title="Add insert" onClick={() => setUI({ selectedMixerId: addMixerTrack() })}><Icon name="plus" /></button>
      </div>
      <InsertPanel track={selected} />
    </div>
  );
}

