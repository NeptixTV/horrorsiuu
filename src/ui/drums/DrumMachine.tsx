import { useEffect, useRef, useState } from 'react';
import { useProject, beginGesture, endGesture } from '../../state/store';
import { setUI, ui, useUI } from '../../state/ui';
import { setStep, setSwing, updateChannel, updateDrum, updatePattern, fillSteps } from '../../state/actions';
import { DRUM_LABELS } from '../../state/defaults';
import { formatPan } from '../../state/utils';
import { engine } from '../../audio/engine';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { onFrame } from '../components/frameLoop';
import { currentStepFor } from '../channelrack/playhead';
import { addDrumKit } from '../browser/Browser';
import { togglePlay } from '../commands';
import './drums.css';

let paint: { ch: string; v: number } | null = null;
window.addEventListener('pointerup', () => {
  if (paint) { paint = null; endGesture(); }
});

export function DrumMachine() {
  const channels = useProject((s) => s.project.channels);
  const patterns = useProject((s) => s.project.patterns);
  const swing = useProject((s) => s.project.swing);
  const patternId = useUI((s) => s.selectedPatternId);
  const playing = useUI((s) => s.playing);
  const pattern = patterns.find((p) => p.id === patternId) ?? patterns[0];
  const drums = channels.filter((c) => c.kind === 'drum' || (c.kind === 'sampler' && /kick|snare|clap|hat|tom|perc|drum/i.test(c.name)));
  const [sel, setSel] = useState<string | null>(null);
  const rootRef = useRef<HTMLDivElement>(null);
  const selected = drums.find((d) => d.id === sel) ?? drums[0];

  useEffect(() => {
    let last = -1;
    return onFrame(() => {
      const root = rootRef.current;
      if (!root) return;
      const step = currentStepFor(ui().selectedPatternId);
      if (step === last) return;
      last = step;
      root.querySelectorAll('.dm-step.playing').forEach((el) => el.classList.remove('playing'));
      root.querySelectorAll('.dm-pad.hit').forEach((el) => el.classList.remove('hit'));
      root.querySelectorAll('.dm-led.on').forEach((el) => el.classList.remove('on'));
      if (step < 0) return;
      root.querySelectorAll(`.dm-step[data-i="${step}"]`).forEach((el) => {
        el.classList.add('playing');
        if (el.classList.contains('on')) {
          const ch = (el as HTMLElement).dataset.ch;
          root.querySelector(`.dm-pad[data-ch="${ch}"]`)?.classList.add('hit');
        }
      });
      root.querySelector(`.dm-led[data-i="${step}"]`)?.classList.add('on');
    });
  }, []);

  if (!pattern) return null;
  if (!drums.length) {
    return (
      <div className="empty-hint">
        No drum channels in this project yet.<br />
        <button className="btn primary" style={{ marginTop: 10 }} onClick={addDrumKit}><Icon name="drums" /> Add Goofy drum kit</button>
      </div>
    );
  }
  const n = pattern.lengthSteps;
  return (
    <div className="dm" ref={rootRef} onPointerDown={() => setUI({ focus: 'other' })}>
      <div className="dm-left">
        <div className="dm-brand">
          <span className="dm-logo">GOOFY</span><span className="dm-model">DM-7</span>
          <span className="spacer" />
          <button className={`btn icon ${playing ? 'on' : ''}`} onClick={() => void togglePlay()} title="Play / Pause"><Icon name={playing ? 'pause' : 'play'} /></button>
        </div>
        <div className="dm-pads">
          {drums.slice(0, 12).map((d, i) => (
            <button
              key={d.id}
              data-ch={d.id}
              className={`dm-pad ${selected?.id === d.id ? 'selected' : ''}`}
              style={{ ['--pc' as string]: d.color }}
              onPointerDown={() => {
                setSel(d.id);
                engine.previewNote(d.id, 60, 1, 0.2);
                const el = rootRef.current?.querySelector(`.dm-pad[data-ch="${d.id}"]`);
                el?.classList.remove('hit');
                void (el as HTMLElement | undefined)?.offsetWidth;
                el?.classList.add('hit');
              }}
            >
              <span className="dm-pad-n">{i + 1}</span>
              <span className="dm-pad-name">{d.name}</span>
            </button>
          ))}
        </div>
        {selected && (
          <div className="dm-voice">
            <div className="dm-voice-title" style={{ color: selected.color }}>{selected.name}{selected.drum ? ` · ${DRUM_LABELS[selected.drum.type]}` : ''}</div>
            <div className="dm-knobs">
              {selected.drum && (
                <>
                  <Knob value={selected.drum.tune} min={-24} max={24} defaultValue={0} size={34} label="Tune" bipolar color={selected.color} onChange={(v) => updateDrum(selected.id, { tune: v })} format={(v) => `${v.toFixed(1)} st`} />
                  <Knob value={selected.drum.decay} min={0.1} max={3} log defaultValue={1} size={34} label="Decay" color={selected.color} onChange={(v) => updateDrum(selected.id, { decay: v })} format={(v) => `${Math.round(v * 100)}%`} />
                  <Knob value={selected.drum.tone} min={0} max={1} defaultValue={0.5} size={34} label="Tone" color={selected.color} onChange={(v) => updateDrum(selected.id, { tone: v })} format={(v) => `${Math.round(v * 100)}%`} />
                  <Knob value={selected.drum.snap} min={0} max={1} defaultValue={0.5} size={34} label="Snap" color={selected.color} onChange={(v) => updateDrum(selected.id, { snap: v })} format={(v) => `${Math.round(v * 100)}%`} />
                </>
              )}
              <Knob value={selected.volume} min={0} max={1.25} defaultValue={0.8} size={34} label="Level" color={selected.color} onChange={(v) => updateChannel(selected.id, { volume: v }, 'Volume')} format={(v) => `${Math.round(v * 100)}%`} />
              <Knob value={selected.pan} min={-1} max={1} defaultValue={0} size={34} label="Pan" bipolar color={selected.color} onChange={(v) => updateChannel(selected.id, { pan: v }, 'Pan')} format={formatPan} />
            </div>
          </div>
        )}
      </div>
      <div className="dm-right">
        <div className="dm-top">
          <select className="input" value={pattern.id} onChange={(e) => { setUI({ selectedPatternId: e.target.value }); if (engine.transport) engine.transport.patternId = e.target.value; }}>
            {patterns.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
          </select>
          <select className="input" value={n} onChange={(e) => updatePattern(pattern.id, { lengthSteps: Number(e.target.value) })}>
            {[8, 16, 32, 64].map((x) => <option key={x} value={x}>{x} steps</option>)}
          </select>
          <Knob value={swing} min={0} max={1} defaultValue={0} size={24} onChange={setSwing} format={(v) => `${Math.round(v * 100)}%`} title="Swing" />
          <span className="label">Swing</span>
          <span className="spacer" />
          <span className="label">click / drag to paint · right-click to erase · scroll for velocity</span>
        </div>
        <div className="dm-leds">
          <span className="dm-row-label" />
          {Array.from({ length: n }, (_, i) => <span key={i} data-i={i} className={`dm-led ${i % 4 === 0 ? 'beat' : ''}`} />)}
        </div>
        <div className="dm-grid scroll-y">
          {drums.map((d) => {
            const steps = pattern.steps[d.id] ?? [];
            return (
              <div key={d.id} className={`dm-row ${d.mute ? 'muted' : ''}`}>
                <button className={`dm-row-label ${selected?.id === d.id ? 'selected' : ''}`} style={{ borderLeftColor: d.color }} onClick={() => setSel(d.id)}
                  onContextMenu={(e) => { e.preventDefault(); fillSteps(pattern.id, d.id, 0); }}
                  title="Click to select · right-click clears the row"
                >
                  <span className={`led ${d.mute ? '' : 'on'}`} onClick={(e) => { e.stopPropagation(); updateChannel(d.id, { mute: !d.mute }, 'Mute'); }} />
                  {d.name}
                </button>
                {Array.from({ length: n }, (_, i) => {
                  const v = steps[i] ?? 0;
                  return (
                    <button
                      key={i}
                      data-i={i}
                      data-ch={d.id}
                      className={`dm-step ${v > 0 ? 'on' : ''} ${Math.floor(i / 4) % 2 ? 'alt' : ''}`}
                      style={v > 0 ? { ['--sc' as string]: d.color, opacity: 0.45 + v * 0.55 } : undefined}
                      onPointerDown={(e) => {
                        e.preventDefault();
                        beginGesture();
                        const nv = e.button === 2 ? 0 : v > 0 ? 0 : 1;
                        paint = { ch: d.id, v: nv };
                        setStep(pattern.id, d.id, i, nv);
                        if (nv) engine.previewNote(d.id, 60, 0.9, 0.15);
                      }}
                      onPointerEnter={() => { if (paint?.ch === d.id) setStep(pattern.id, d.id, i, paint.v); }}
                      onContextMenu={(e) => e.preventDefault()}
                      onWheel={(e) => { if (v > 0) setStep(pattern.id, d.id, i, Math.max(0.05, Math.min(1, v + (e.deltaY < 0 ? 0.1 : -0.1)))); }}
                    />
                  );
                })}
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}

