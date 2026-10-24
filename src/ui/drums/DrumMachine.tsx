import { useEffect, useRef, useState, type DragEvent } from 'react';
import { useProject, beginGesture, endGesture } from '../../state/store';
import { setUI, toast, ui, useUI } from '../../state/ui';
import { addChannel, setRoll, setStep, setSwing, updateChannel, updateDrum, updatePattern, updatePro, fillSteps } from '../../state/actions';
import { DRUM_LABELS } from '../../state/defaults';
import type { Channel } from '../../state/types';
import { formatPan } from '../../state/utils';
import { engine } from '../../audio/engine';
import { DRUM_GROUPS, PRO_KITS, PRO_SOUNDS, proSound } from '../../audio/pro/catalog';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { showMenu } from '../components/ContextMenu';
import { onFrame } from '../components/frameLoop';
import { currentStepFor } from '../channelrack/playhead';
import { addDrumKit, readDragPayload } from '../browser/Browser';
import { togglePlay } from '../commands';
import { addProChannel, loadKit, loadProIntoChannel, previewPro } from '../proSounds';
import { decodeAudioFile } from '../../project/manager';
import './drums.css';

let paint: { ch: string; v: number; roll: number } | null = null;
window.addEventListener('pointerup', () => {
  if (paint) { paint = null; endGesture(); }
});

/** Channels that behave like drum pads: synth drums, Pro one-shots, drum-named samples. */
export function isDrumChannel(c: Channel) {
  if (c.kind === 'drum') return true;
  if (c.kind === 'pro') return !!proSound(c.pro?.sound)?.oneShot;
  return c.kind === 'sampler' && /kick|snare|clap|hat|tom|perc|drum|rim|crash|ride|shaker/i.test(c.name);
}

const BRUSHES = [1, 2, 3, 4, 6];

/** Random trap hi-hat line: 8ths with 16th fills and rolls before the snare. */
export function trapHats(len: number): { steps: number[]; rolls: number[] } {
  const steps: number[] = Array.from({ length: len }, (_, i) => (i % 2 === 0 ? (i % 4 === 0 ? 1 : 0.7) : 0));
  const rolls = Array.from({ length: len }, () => 1);
  for (let bar = 0; bar < len; bar += 16) {
    // a fill or roll somewhere in each half bar, mostly leading into beats 2 and 4
    for (const at of [6, 14]) {
      const i = bar + at + (Math.random() < 0.3 ? -2 : 0);
      if (i >= len) continue;
      const r = Math.random();
      if (r < 0.45) { steps[i] = 0.8; rolls[i] = Math.random() < 0.6 ? 3 : 2; }
      else if (r < 0.8) { steps[i] = 0.75; steps[i + 1] = 0.65; }
      else { steps[i] = 0.8; rolls[i] = 4; if (i + 1 < len) { steps[i + 1] = 0.7; rolls[i + 1] = 6; } }
    }
  }
  return { steps, rolls };
}

function PadEditor({ d }: { d: Channel }) {
  const def = d.kind === 'pro' ? proSound(d.pro?.sound) : undefined;
  const list = PRO_SOUNDS.filter((s) => s.oneShot && (!def || s.group === def.group));
  const step = (dir: number) => {
    if (!def) return;
    const i = list.findIndex((s) => s.id === def.id);
    const next = list[(i + dir + list.length) % list.length];
    loadProIntoChannel(d.id, next.id);
    void previewPro(next.id);
  };
  return (
    <div className="dm-voice">
      <div className="dm-voice-title" style={{ color: d.color }}>
        {d.name}
        {d.kind === 'drum' && d.drum ? <span className="crumb"> · {DRUM_LABELS[d.drum.type]} (classic)</span> : null}
      </div>
      <div className="dm-sound">
        {def && <button className="btn icon small" title="Previous sound" onClick={() => step(-1)}><Icon name="chevronLeft" size={11} /></button>}
        <select
          className="input"
          value={d.kind === 'pro' ? d.pro?.sound : ''}
          onChange={(e) => { if (e.target.value) { loadProIntoChannel(d.id, e.target.value); void previewPro(e.target.value); } }}
          title="Pick a sound for this pad"
        >
          {d.kind !== 'pro' && <option value="">{d.kind === 'drum' ? 'Classic synth drum – pick a Pro sound…' : 'Own sample – pick a Pro sound…'}</option>}
          {DRUM_GROUPS.map((g) => (
            <optgroup key={g} label={g}>
              {PRO_SOUNDS.filter((s) => s.group === g).map((s) => <option key={s.id} value={s.id}>{s.name}</option>)}
            </optgroup>
          ))}
        </select>
        {def && <button className="btn icon small" title="Next sound" onClick={() => step(1)}><Icon name="chevronRight" size={11} /></button>}
      </div>
      <div className="dm-knobs">
        {d.kind === 'drum' && d.drum && (
          <>
            <Knob value={d.drum.tune} min={-24} max={24} defaultValue={0} size={34} label="Tune" bipolar color={d.color} onChange={(v) => updateDrum(d.id, { tune: v })} format={(v) => `${v.toFixed(1)} st`} />
            <Knob value={d.drum.decay} min={0.1} max={3} log defaultValue={1} size={34} label="Decay" color={d.color} onChange={(v) => updateDrum(d.id, { decay: v })} format={(v) => `${Math.round(v * 100)}%`} />
            <Knob value={d.drum.tone} min={0} max={1} defaultValue={0.5} size={34} label="Tone" color={d.color} onChange={(v) => updateDrum(d.id, { tone: v })} format={(v) => `${Math.round(v * 100)}%`} />
            <Knob value={d.drum.snap} min={0} max={1} defaultValue={0.5} size={34} label="Snap" color={d.color} onChange={(v) => updateDrum(d.id, { snap: v })} format={(v) => `${Math.round(v * 100)}%`} />
          </>
        )}
        {d.kind === 'pro' && d.pro && (
          <>
            <Knob value={d.pro.tune} min={-24} max={24} defaultValue={0} size={34} label="Tune" bipolar color={d.color} onChange={(v) => updatePro(d.id, { tune: v })} format={(v) => `${v.toFixed(1)} st`} />
            <Knob value={d.pro.tone} min={0} max={1} defaultValue={1} size={34} label="Tone" color={d.color} onChange={(v) => updatePro(d.id, { tone: v })} format={(v) => (v >= 0.999 ? 'open' : `${Math.round(v * 100)}%`)} />
            <Knob value={d.pro.drive} min={0} max={1} defaultValue={0} size={34} label="Drive" color={d.color} onChange={(v) => updatePro(d.id, { drive: v })} format={(v) => `${Math.round(v * 100)}%`} />
          </>
        )}
        <Knob value={d.volume} min={0} max={1.25} defaultValue={0.8} size={34} label="Level" color={d.color} onChange={(v) => updateChannel(d.id, { volume: v }, 'Volume')} format={(v) => `${Math.round(v * 100)}%`} />
        <Knob value={d.pan} min={-1} max={1} defaultValue={0} size={34} label="Pan" bipolar color={d.color} onChange={(v) => updateChannel(d.id, { pan: v }, 'Pan')} format={formatPan} />
      </div>
    </div>
  );
}

function kitMenu(e: React.MouseEvent) {
  showMenu(e.clientX, e.clientY, [
    ...PRO_KITS.map((k) => ({
      label: `${k.name}  ·  ${k.genre} ${k.bpm}`,
      submenu: [
        { label: 'With starter beat', onClick: () => loadKit(k.id) },
        { label: 'Sounds only', onClick: () => loadKit(k.id, { pattern: false, bass: false }) },
      ],
    })),
    { separator: true },
    { label: 'Classic Goofy kit (synth drums)', onClick: addDrumKit },
  ]);
}

export function DrumMachine() {
  const channels = useProject((s) => s.project.channels);
  const patterns = useProject((s) => s.project.patterns);
  const swing = useProject((s) => s.project.swing);
  const patternId = useUI((s) => s.selectedPatternId);
  const playing = useUI((s) => s.playing);
  const pattern = patterns.find((p) => p.id === patternId) ?? patterns[0];
  const drums = channels.filter(isDrumChannel);
  const [sel, setSel] = useState<string | null>(null);
  const [brush, setBrush] = useState(1);
  const [dropPad, setDropPad] = useState<string | null>(null);
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

  const hitPad = (id: string) => {
    engine.previewNote(id, 60, 1, 0.2);
    const el = rootRef.current?.querySelector(`.dm-pad[data-ch="${id}"]`);
    el?.classList.remove('hit');
    void (el as HTMLElement | undefined)?.offsetWidth;
    el?.classList.add('hit');
  };

  /** Drop a Pro sound / sample / audio file on a pad (or the empty pad = new pad). */
  const onPadDrop = async (e: DragEvent, target: string | null) => {
    e.preventDefault();
    e.stopPropagation();
    setDropPad(null);
    const file = Array.from(e.dataTransfer.files ?? [])[0];
    if (file) {
      const res = await decodeAudioFile(file);
      if (!res) return;
      if (target) updateChannel(target, { kind: 'sampler', sampler: { assetId: res.id, rootNote: 60, tune: 0, attack: 0.001, release: 0.05, reverse: false, level: 0.9 }, name: res.name }, 'Load sample on pad');
      else setSel(addChannel('sampler', { assetId: res.id, name: res.name }));
      toast(`“${res.name}” is on a pad now`, 'ok');
      return;
    }
    const p = readDragPayload(e);
    setUI({ dragPayload: null });
    if (p?.type === 'pro') {
      if (target) loadProIntoChannel(target, p.sound);
      else setSel(addProChannel(p.sound));
    } else if (p?.type === 'kit') loadKit(p.kit);
    else if (p?.type === 'sample' && target) {
      updateChannel(target, { kind: 'sampler', sampler: { assetId: p.assetId, rootNote: 60, tune: 0, attack: 0.001, release: 0.05, reverse: false, level: 0.9 }, name: p.name }, 'Load sample on pad');
    }
  };

  if (!pattern) return null;
  if (!drums.length) {
    return (
      <div className="empty-hint dm-empty" onDragOver={(e) => e.preventDefault()} onDrop={(e) => void onPadDrop(e, null)}>
        <div>No drums in this project yet – pick a kit:</div>
        <div className="dm-kit-grid">
          {PRO_KITS.map((k) => (
            <button key={k.id} className="dm-kit" style={{ ['--pc' as string]: k.color }} onClick={() => loadKit(k.id)}>
              <b>{k.name}</b><span>{k.genre} · {k.bpm} BPM</span>
            </button>
          ))}
        </div>
      </div>
    );
  }
  const n = pattern.lengthSteps;
  return (
    <div className="dm" ref={rootRef} onPointerDown={() => setUI({ focus: 'other' })}>
      <div className="dm-left">
        <div className="dm-brand">
          <span className="dm-logo">GOOFY</span><span className="dm-model">DM-16</span>
          <span className="spacer" />
          <button className="btn small" onClick={kitMenu} title="Load a Pro drum kit"><Icon name="drums" size={11} /> Kits</button>
          <button className={`btn icon ${playing ? 'on' : ''}`} onClick={() => void togglePlay()} title="Play / Pause"><Icon name={playing ? 'pause' : 'play'} /></button>
        </div>
        <div className="dm-pads">
          {drums.slice(0, 16).map((d, i) => (
            <button
              key={d.id}
              data-ch={d.id}
              className={`dm-pad ${selected?.id === d.id ? 'selected' : ''} ${dropPad === d.id ? 'drop' : ''}`}
              style={{ ['--pc' as string]: d.color }}
              onPointerDown={() => { setSel(d.id); hitPad(d.id); }}
              onDragOver={(e) => { e.preventDefault(); setDropPad(d.id); }}
              onDragLeave={() => setDropPad(null)}
              onDrop={(e) => void onPadDrop(e, d.id)}
              title={`${d.name} – click to play, drop a sound here to replace it`}
            >
              <span className="dm-pad-n">{i + 1}</span>
              {d.kind === 'pro' && proSound(d.pro?.sound)?.real && <span className="dm-pad-real">REAL</span>}
              <span className="dm-pad-name">{d.name}</span>
            </button>
          ))}
          {drums.length < 16 && (
            <button
              className={`dm-pad add ${dropPad === 'new' ? 'drop' : ''}`}
              onClick={(e) => showMenu(e.clientX, e.clientY, DRUM_GROUPS.map((g) => ({ label: g, submenu: PRO_SOUNDS.filter((s) => s.group === g).map((s) => ({ label: s.name, onClick: () => setSel(addProChannel(s.id)) })) })))}
              onDragOver={(e) => { e.preventDefault(); setDropPad('new'); }}
              onDragLeave={() => setDropPad(null)}
              onDrop={(e) => void onPadDrop(e, null)}
              title="Add a pad (or drop a sound / audio file here)"
            >
              <Icon name="plus" size={14} />
            </button>
          )}
        </div>
        {selected && <PadEditor d={selected} />}
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
          <span className="dm-sep" />
          <span className="label">Brush</span>
          <div className="seg">
            {BRUSHES.map((b) => (
              <button key={b} className={`seg-btn ${brush === b ? 'on' : ''}`} onClick={() => setBrush(b)} title={b === 1 ? 'Normal hits' : `Roll: ${b} hits per step (hi-hat rolls)`}>{b === 1 ? 'Hit' : `×${b}`}</button>
            ))}
          </div>
          <span className="spacer" />
          <span className="label">paint · right-click erases · scroll = velocity · Alt+click = roll</span>
        </div>
        <div className="dm-leds">
          <span className="dm-row-label" />
          {Array.from({ length: n }, (_, i) => <span key={i} data-i={i} className={`dm-led ${i % 4 === 0 ? 'beat' : ''}`} />)}
        </div>
        <div className="dm-grid scroll-y">
          {drums.map((d) => {
            const steps = pattern.steps[d.id] ?? [];
            const rolls = pattern.rolls?.[d.id];
            const isHat = d.kind === 'pro' ? proSound(d.pro?.sound)?.group === 'Hi-Hats' : /hat/i.test(d.name);
            return (
              <div key={d.id} className={`dm-row ${d.mute ? 'muted' : ''}`}>
                <button className={`dm-row-label ${selected?.id === d.id ? 'selected' : ''}`} style={{ borderLeftColor: d.color }} onClick={() => setSel(d.id)}
                  onContextMenu={(e) => {
                    e.preventDefault();
                    showMenu(e.clientX, e.clientY, [
                      { label: 'Clear row', onClick: () => fillSteps(pattern.id, d.id, 0) },
                      { label: 'Every beat (4)', onClick: () => fillSteps(pattern.id, d.id, 4) },
                      { label: 'Every 8th (2)', onClick: () => fillSteps(pattern.id, d.id, 2) },
                      { label: 'Every 16th', onClick: () => fillSteps(pattern.id, d.id, 1) },
                      ...(isHat ? [{ separator: true }, {
                        label: '🎲 Trap hi-hats with rolls', onClick: () => {
                          const h = trapHats(n);
                          beginGesture();
                          h.steps.forEach((v, i) => setStep(pattern.id, d.id, i, v));
                          h.rolls.forEach((r, i) => { if (r > 1 || (rolls?.[i] ?? 1) > 1) setRoll(pattern.id, d.id, i, r); });
                          endGesture();
                        },
                      }] : []),
                    ]);
                  }}
                  title="Click to select · right-click: fill / clear / trap hats"
                >
                  <span className={`led ${d.mute ? '' : 'on'}`} onClick={(e) => { e.stopPropagation(); updateChannel(d.id, { mute: !d.mute }, 'Mute'); }} />
                  {d.name}
                  {isHat && (
                    <span
                      className="dm-dice"
                      title="New trap hi-hat pattern with rolls"
                      onClick={(e) => {
                        e.stopPropagation();
                        const h = trapHats(n);
                        beginGesture();
                        h.steps.forEach((v, i) => setStep(pattern.id, d.id, i, v));
                        h.rolls.forEach((r, i) => { if (r > 1 || (rolls?.[i] ?? 1) > 1) setRoll(pattern.id, d.id, i, r); });
                        endGesture();
                      }}
                    >🎲</span>
                  )}
                </button>
                {Array.from({ length: n }, (_, i) => {
                  const v = steps[i] ?? 0;
                  const r = rolls?.[i] ?? 1;
                  return (
                    <button
                      key={i}
                      data-i={i}
                      data-ch={d.id}
                      data-roll={v > 0 && r > 1 ? r : undefined}
                      className={`dm-step ${v > 0 ? 'on' : ''} ${Math.floor(i / 4) % 2 ? 'alt' : ''} ${v > 0 && r > 1 ? 'roll' : ''}`}
                      style={v > 0 ? { ['--sc' as string]: d.color, opacity: 0.45 + v * 0.55 } : undefined}
                      onPointerDown={(e) => {
                        e.preventDefault();
                        if (e.altKey && e.button === 0) {
                          const next = [1, 2, 3, 4, 6, 8][([1, 2, 3, 4, 6, 8].indexOf(v > 0 ? r : 1) + 1) % 6];
                          setRoll(pattern.id, d.id, i, next);
                          return;
                        }
                        beginGesture();
                        const erase = e.button === 2 || (v > 0 && (brush === 1 ? r <= 1 : r === brush));
                        const nv = erase ? 0 : v > 0 ? v : 1;
                        paint = { ch: d.id, v: nv, roll: erase ? 1 : brush };
                        setStep(pattern.id, d.id, i, nv);
                        if (nv && (brush > 1 || r > 1)) setRoll(pattern.id, d.id, i, brush);
                        else if (!nv && r > 1) setRoll(pattern.id, d.id, i, 1);
                        if (nv) engine.previewNote(d.id, 60, 0.9, 0.15);
                      }}
                      onPointerEnter={() => {
                        if (paint?.ch !== d.id) return;
                        setStep(pattern.id, d.id, i, paint.v);
                        if (paint.v && (paint.roll > 1 || r > 1)) setRoll(pattern.id, d.id, i, paint.roll);
                        else if (!paint.v && r > 1) setRoll(pattern.id, d.id, i, 1);
                      }}
                      onContextMenu={(e) => e.preventDefault()}
                      onWheel={(e) => { if (v > 0) setStep(pattern.id, d.id, i, Math.max(0.05, Math.min(1, v + (e.deltaY < 0 ? 0.1 : -0.1)))); }}
                      title={v > 0 ? `Velocity ${Math.round(v * 100)}%${r > 1 ? ` · roll ×${r}` : ''}` : undefined}
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
