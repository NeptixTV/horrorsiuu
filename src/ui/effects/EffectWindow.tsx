import { useEffect } from 'react';
import { useProject } from '../../state/store';
import type { FloatingWindow } from '../../state/ui';
import { closeWindow } from '../../state/ui';
import { EFFECTS, EFFECT_PRESETS, defaultParams } from '../../audio/effects/descriptors';
import { setEffectParam, setEffectParams, toggleBypass, removeInsert } from '../../state/actions';
import { engine } from '../../audio/engine';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { showMenu } from '../components/ContextMenu';
import { VIEWS } from './views';
import './effects.css';

export function EffectWindow({ w, trackId, effectId }: { w: FloatingWindow; trackId: string; effectId: string }) {
  const track = useProject((s) => s.project.mixer.find((m) => m.id === trackId));
  const fx = track?.inserts.find((e) => e.id === effectId);
  const missing = !track || !fx;
  useEffect(() => {
    if (missing) closeWindow(w.id);
  }, [missing, w.id]);
  if (!track || !fx) return null;
  const d = EFFECTS[fx.type];
  const View = VIEWS[fx.type];
  const presets = EFFECT_PRESETS.filter((p) => p.type === fx.type);
  return (
    <Window
      w={w}
      width={fx.type === 'pitch' || fx.type === 'eq' ? 560 : 470}
      color={d.color}
      title={<><b>{d.name}</b> <span className="crumb">· {track.name}</span></>}
      actions={
        <>
          <button className="btn small" onClick={(e) => showMenu(e.clientX, e.clientY, [
            { label: 'Default', onClick: () => setEffectParams(trackId, effectId, defaultParams(fx.type)) },
            ...presets.map((p) => ({ label: p.name, onClick: () => setEffectParams(trackId, effectId, p.params) })),
          ])}>
            <Icon name="preset" size={11} /> Presets
          </button>
          <button className={`btn small ${fx.bypass ? '' : 'on'}`} title="Bypass" onClick={() => toggleBypass(trackId, effectId)}>
            <Icon name="power" size={11} /> {fx.bypass ? 'Off' : 'On'}
          </button>
          <button className="btn ghost icon small danger" title="Remove effect" onClick={() => { removeInsert(trackId, effectId); closeWindow(w.id); }}><Icon name="trash" size={11} /></button>
        </>
      }
    >
      <div className={`fx-body ${fx.bypass ? 'bypassed' : ''}`} style={{ ['--fxc' as string]: d.color }}>
        <View
          trackId={trackId}
          fx={fx}
          set={(k, v) => setEffectParam(trackId, effectId, k, v)}
          effect={() => engine.initialized ? engine.graph.strips.get(trackId)?.slot(effectId)?.effect : undefined}
        />
      </div>
    </Window>
  );
}
