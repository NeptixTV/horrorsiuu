import { useEffect } from 'react';
import { useShallow } from 'zustand/react/shallow';
import { useProject } from '../../state/store';
import type { FloatingWindow } from '../../state/ui';
import { closeWindow } from '../../state/ui';
import type { EffectInstance, Project } from '../../state/types';
import { MASTER_ID } from '../../state/types';
import { EFFECTS, EFFECT_PRESETS, defaultParams } from '../../audio/effects/descriptors';
import { fxBypass, fxParam, fxRemove, type FxTarget } from '../../state/actions';
import { update } from '../../state/store';
import { engine } from '../../audio/engine';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { showMenu } from '../components/ContextMenu';
import { VIEWS } from './views';
import { createAutomation } from '../automation/AutomationWindow';
import { userPresetItems } from '../presetMenu';
import { normalizeEffect } from '../../state/normalize';
import './effects.css';

function resolve(p: Project, target: FxTarget, effectId: string): { fx?: EffectInstance; owner: string; meterTrack: string } {
  if (target.kind === 'mixer') {
    const m = p.mixer.find((x) => x.id === target.id);
    return { fx: m?.inserts.find((e) => e.id === effectId), owner: m?.name ?? '', meterTrack: target.id };
  }
  const c = p.clips.find((x) => x.id === target.id);
  const tr = c ? p.tracks.find((t) => t.id === c.trackId) : undefined;
  return { fx: c?.fx?.find((e) => e.id === effectId), owner: `Clip · ${c?.name ?? p.assets[c?.assetId ?? '']?.name ?? 'audio'}`, meterTrack: tr?.mixerTrackId ?? MASTER_ID };
}

/** Engine-side effect object for a target (for live visualisations). */
export function liveEffect(target: FxTarget, effectId: string) {
  if (!engine.initialized) return undefined;
  return target.kind === 'mixer' ? engine.graph.strips.get(target.id)?.slot(effectId)?.effect : engine.graph.clipSlot(target.id, effectId)?.effect;
}

export function EffectWindow({ w, target, effectId }: { w: FloatingWindow; target: FxTarget; effectId: string }) {
  const { fx, owner, meterTrack } = useProject(useShallow((s) => resolve(s.project, target, effectId)));
  useEffect(() => {
    if (!fx) closeWindow(w.id);
  }, [fx, w.id]);
  if (!fx) return null;
  const d = EFFECTS[fx.type];
  const View = VIEWS[fx.type];
  const presets = EFFECT_PRESETS.filter((p) => p.type === fx.type);
  const setAll = (params: EffectInstance['params']) => update('Effect preset', (dr) => {
    const list = target.kind === 'mixer' ? dr.mixer.find((m) => m.id === target.id)?.inserts : dr.clips.find((c) => c.id === target.id)?.fx;
    const e = list?.find((x) => x.id === effectId);
    if (e) e.params = { ...e.params, ...params };
  });
  return (
    <Window
      w={w}
      width={fx.type === 'pitch' || fx.type === 'eq' ? 560 : 470}
      color={d.color}
      title={<><b>{d.name}</b> <span className="crumb">· {owner}</span></>}
      actions={
        <>
          <button className="btn small" onClick={(e) => showMenu(e.clientX, e.clientY, [
            { label: 'Default', onClick: () => setAll(defaultParams(fx.type)) },
            ...presets.map((p) => ({ label: p.name, onClick: () => setAll(p.params) })),
            ...userPresetItems(`effect:${fx.type}`, () => fx.params, (params) => {
              const clean = normalizeEffect({ id: fx.id, type: fx.type, bypass: false, params });
              if (clean) setAll(clean.params);
            }, `My ${d.name}`),
          ])}>
            <Icon name="preset" size={11} /> Presets
          </button>
          {target.kind === 'mixer' && (
            <button className="btn small" title="Automate a parameter of this effect over the song" onClick={(e) => showMenu(e.clientX, e.clientY, d.params.filter((p) => p.kind === 'number').map((p) => ({
              label: p.label, onClick: () => createAutomation({ kind: 'effect', trackId: target.id, effectId, param: p.key }),
            })))}>
              <Icon name="automation" size={11} /> Automate
            </button>
          )}
          <button className={`btn small ${fx.bypass ? '' : 'on'}`} title="Bypass" onClick={() => fxBypass(target, effectId)}>
            <Icon name="power" size={11} /> {fx.bypass ? 'Off' : 'On'}
          </button>
          <button className="btn ghost icon small danger" title="Remove effect" onClick={() => { fxRemove(target, effectId); closeWindow(w.id); }}><Icon name="trash" size={11} /></button>
        </>
      }
    >
      <div className={`fx-body ${fx.bypass ? 'bypassed' : ''}`} style={{ ['--fxc' as string]: d.color }}>
        <View trackId={meterTrack} fx={fx} set={(k, v) => fxParam(target, effectId, k, v)} effect={() => liveEffect(target, effectId)} />
      </div>
    </Window>
  );
}
