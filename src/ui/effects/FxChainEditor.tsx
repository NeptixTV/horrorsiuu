// Editable effect chain (used for clips, playlist rows and the live mic).
// Every knob changes the running audio immediately.
import { useState } from 'react';
import type { EffectInstance, EffectType } from '../../state/types';
import { EFFECTS, EFFECT_PRESETS, EFFECT_TYPES } from '../../audio/effects/descriptors';
import { FX_CATEGORIES, FX_CHAINS, type FxChainCategory } from '../../audio/effects/fxPresets';
import { fxAdd, fxApplyChain, fxBypass, fxClear, fxMove, fxParam, fxRemove, type FxTarget } from '../../state/actions';
import type { ParamValue } from '../../state/types';
import { openWindow, toast } from '../../state/ui';
import { update } from '../../state/store';
import { Icon } from '../components/Icon';
import { showMenu } from '../components/ContextMenu';
import { ParamControl } from './ParamKnob';
import './fxchain.css';

/** Operations on an effect chain – backed by the project or by local state. */
export interface ChainOps {
  add(type: EffectType): void;
  remove(id: string): void;
  move(id: string, dir: -1 | 1): void;
  bypass(id: string): void;
  param(id: string, key: string, value: ParamValue): void;
  setParams(id: string, params: Record<string, ParamValue>): void;
  clear(): void;
  applyChain(effects: [EffectType, Record<string, ParamValue>][], mode: 'add' | 'replace', label: string): void;
  openBig?(id: string): void;
}

export function projectChainOps(target: FxTarget): ChainOps {
  return {
    add: (type) => { fxAdd(target, type); },
    remove: (id) => fxRemove(target, id),
    move: (id, dir) => fxMove(target, id, dir),
    bypass: (id) => fxBypass(target, id),
    param: (id, k, v) => fxParam(target, id, k, v),
    setParams: (id, params) => update('Effect preset', (dr) => {
      const list = target.kind === 'mixer' ? dr.mixer.find((m) => m.id === target.id)?.inserts : dr.clips.find((c) => c.id === target.id)?.fx;
      const e = list?.find((x) => x.id === id);
      if (e) e.params = { ...e.params, ...params };
    }),
    clear: () => fxClear(target),
    applyChain: (effects, mode, label) => fxApplyChain(target, effects, mode, label),
    openBig: (id) => openWindow(target.kind === 'mixer' ? { kind: 'effect', trackId: target.id, effectId: id } : { kind: 'clipEffect', clipId: target.id, effectId: id }),
  };
}

export function SoundFxPicker({ ops, categories = FX_CATEGORIES, compact = false }: { ops: ChainOps; categories?: FxChainCategory[]; compact?: boolean }) {
  const [cat, setCat] = useState<FxChainCategory>(categories[0]);
  const [replace, setReplace] = useState(true);
  return (
    <div className={`sfx ${compact ? 'compact' : ''}`}>
      <div className="sfx-head">
        <span className="label">Sound FX</span>
        <div className="sfx-tabs">
          {categories.map((c) => <button key={c} className={`sfx-tab ${c === cat ? 'on' : ''}`} onClick={() => setCat(c)}>{c}</button>)}
        </div>
        <span className="spacer" />
        <label className="sfx-mode" title="Replace the current effects or add on top">
          <input type="checkbox" checked={replace} onChange={(e) => setReplace(e.target.checked)} /> replace
        </label>
      </div>
      <div className="sfx-chips">
        {FX_CHAINS.filter((c) => c.category === cat).map((c) => (
          <button
            key={c.name}
            className="sfx-chip"
            title={c.effects.map(([t]) => EFFECTS[t].name).join(' → ')}
            onClick={() => { ops.applyChain(c.effects, replace ? 'replace' : 'add', c.name); toast(`${c.icon} ${c.name}`, 'ok'); }}
          >
            <span className="sfx-icon">{c.icon}</span>{c.name}
          </button>
        ))}
      </div>
    </div>
  );
}

function FxCard({ ops, fx, index, count }: { ops: ChainOps; fx: EffectInstance; index: number; count: number }) {
  const [open, setOpen] = useState(true);
  const d = EFFECTS[fx.type];
  const presets = EFFECT_PRESETS.filter((p) => p.type === fx.type);
  return (
    <div className={`fxc ${fx.bypass ? 'bypassed' : ''}`} style={{ ['--fc' as string]: d.color }}>
      <div className="fxc-head">
        <span className={`led ${fx.bypass ? '' : 'on'}`} title={fx.bypass ? 'Enable' : 'Bypass'} onClick={() => ops.bypass(fx.id)} />
        <button className="fxc-name" onClick={() => setOpen(!open)} title="Collapse / expand">
          <Icon name={open ? 'chevronDown' : 'chevronRight'} size={10} /> {d.name}
        </button>
        <span className="spacer" />
        {presets.length > 0 && (
          <button className="btn ghost small" title="Presets" onClick={(e) => showMenu(e.clientX, e.clientY, presets.map((p) => ({ label: p.name, onClick: () => ops.setParams(fx.id, p.params) })))}>
            <Icon name="preset" size={10} />
          </button>
        )}
        {ops.openBig && <button className="btn ghost icon small" title="Open large editor" onClick={() => ops.openBig!(fx.id)}><Icon name="maximize" size={10} /></button>}
        <button className="btn ghost icon small" disabled={index === 0} title="Move up" onClick={() => ops.move(fx.id, -1)}><Icon name="chevronUp" size={10} /></button>
        <button className="btn ghost icon small" disabled={index === count - 1} title="Move down" onClick={() => ops.move(fx.id, 1)}><Icon name="chevronDown" size={10} /></button>
        <button className="btn ghost icon small danger" title="Remove" onClick={() => ops.remove(fx.id)}><Icon name="close" size={10} /></button>
      </div>
      {open && (
        <div className="fxc-knobs">
          {d.params.map((p) => (
            <ParamControl key={p.key} def={p} value={fx.params[p.key]} size={30} color={d.color} onChange={(v) => ops.param(fx.id, p.key, v)} />
          ))}
        </div>
      )}
    </div>
  );
}

export function FxChainEditor({ target, ops: givenOps, effects, title = 'Effects', presets = true }: {
  target?: FxTarget; ops?: ChainOps; effects: EffectInstance[]; title?: string; presets?: boolean;
}) {
  const ops = givenOps ?? projectChainOps(target!);
  return (
    <div className="fxchain">
      {presets && <SoundFxPicker ops={ops} />}
      <div className="fxchain-head">
        <span className="label">{title}{effects.length ? ` (${effects.length})` : ''}</span>
        <span className="spacer" />
        {effects.length > 0 && <button className="btn ghost small" onClick={() => ops.clear()}>Clear all</button>}
        <select
          className="input small-select"
          value=""
          onChange={(e) => { if (e.target.value) ops.add(e.target.value as EffectType); }}
          title="Add a single effect"
        >
          <option value="">+ Add effect…</option>
          {EFFECT_TYPES.map((t) => <option key={t} value={t}>{EFFECTS[t].name}</option>)}
        </select>
      </div>
      {effects.length === 0 && <div className="fxchain-empty">No effects yet – pick a Sound FX above or add a single effect.</div>}
      {effects.map((fx, i) => <FxCard key={fx.id} ops={ops} fx={fx} index={i} count={effects.length} />)}
    </div>
  );
}
