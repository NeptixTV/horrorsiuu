// Editor for Pro Sounds channels: pick the sound (with quick previous / next
// for auditioning), shape it with a few big knobs.
import { useEffect, useState } from 'react';
import type { Channel } from '../../state/types';
import { setProSound, updatePro } from '../../state/actions';
import { PRO_GROUPS, PRO_SOUNDS, proSound, proZones } from '../../audio/pro/catalog';
import { coreZones, preloadSound, useProLoading, zoneBuffer } from '../../audio/pro/loader';
import { computePeaks } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { theme } from '../../styles/themes';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { useCanvas } from '../visualizers/Visualizers';
import { Select } from '../components/Select';

function SoundWave({ ch }: { ch: Channel }) {
  const sound = ch.pro!.sound;
  const pending = useProLoading((s) => s.pending);
  const [peaks, setPeaks] = useState<Float32Array | null>(null);
  useEffect(() => {
    const zones = coreZones(proZones(sound));
    // the zone nearest middle C shows what the sound looks like
    const z = zones.reduce<typeof zones[number] | null>((b, x) => (!b || Math.abs(x.r - 60) < Math.abs(b.r - 60) ? x : b), null);
    const buf = z ? zoneBuffer(z) : null;
    setPeaks(buf ? computePeaks(buf, Math.max(64, Math.floor(buf.length / 600))).data : null);
  }, [sound, pending]);
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    if (!peaks) {
      ctx.fillStyle = t.dim;
      ctx.font = '11px Inter, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('Loading sound…', w / 2, h / 2 + 4);
      ctx.textAlign = 'left';
      return;
    }
    const n = peaks.length / 2;
    let max = 0.001;
    for (let i = 0; i < peaks.length; i++) max = Math.max(max, Math.abs(peaks[i]));
    ctx.fillStyle = ch.color;
    for (let x = 0; x < w; x++) {
      const i0 = Math.floor((x / w) * n);
      const i1 = Math.max(i0 + 1, Math.floor(((x + 1) / w) * n));
      let mn = 0;
      let mx = 0;
      for (let i = i0; i < i1; i++) { mn = Math.min(mn, peaks[i * 2]); mx = Math.max(mx, peaks[i * 2 + 1]); }
      ctx.fillRect(x, h / 2 - (mx / max) * (h / 2 - 3), 1, Math.max(1, ((mx - mn) / max) * (h / 2 - 3)));
    }
  }, [peaks, ch.color]);
  return <canvas ref={ref} className="drum-wave" />;
}

const pct = (v: number) => `${Math.round(v * 100)}%`;
const ms = (v: number) => (v < 1 ? `${Math.round(v * 1000)} ms` : `${v.toFixed(2)} s`);

export function ProEditor({ ch }: { ch: Channel }) {
  const p = ch.pro!;
  const def = proSound(p.sound);
  const sameGroup = PRO_SOUNDS.filter((s) => s.group === def?.group);
  const idx = sameGroup.findIndex((s) => s.id === p.sound);
  const hit = () => engine.previewNote(ch.id, def?.oneShot ? 60 : 60, 0.9, def?.oneShot ? undefined : 0.6);
  const step = (dir: number) => {
    if (!sameGroup.length) return;
    const next = sameGroup[(idx + dir + sameGroup.length) % sameGroup.length];
    setProSound(ch.id, next.id);
    void preloadSound(next.id, false).then(() => setTimeout(hit, 30));
  };
  return (
    <div className="drum-edit pro-edit">
      <div className="pro-head">
        <button className="btn icon" title="Previous sound in this group" onClick={() => step(-1)}><Icon name="chevronLeft" size={12} /></button>
        <Select className="input pro-select" value={p.sound} onChange={(e) => { setProSound(ch.id, e.target.value); void preloadSound(e.target.value, false).then(() => setTimeout(hit, 30)); }}>
          {!def && <option value={p.sound}>{p.sound} (not available in this version)</option>}
          {PRO_GROUPS.map((g) => (
            <optgroup key={g} label={g}>
              {PRO_SOUNDS.filter((s) => s.group === g).map((s) => <option key={s.id} value={s.id}>{s.name}</option>)}
            </optgroup>
          ))}
        </Select>
        <button className="btn icon" title="Next sound in this group" onClick={() => step(1)}><Icon name="chevronRight" size={12} /></button>
        <span className={`pro-badge ${def?.real ? 'real' : ''}`} title={def?.real ? 'Recorded from a real instrument (VSCO 2 Community Edition, CC0)' : 'Synthesized for Goofy Studio'}>
          {def?.real ? 'REAL' : 'SYNTH'}
        </span>
        <span className="label">{def?.group}{def?.oneShot ? ' · one-shot' : ''}</span>
      </div>
      <SoundWave ch={ch} />
      <div className="syn-row center">
        <Knob value={p.tune} min={-24} max={24} defaultValue={0} size={38} label="Tune" bipolar onChange={(v) => updatePro(ch.id, { tune: v })} format={(v) => `${v > 0 ? '+' : ''}${v.toFixed(1)} st`} />
        <Knob value={p.attack} min={0.001} max={2} log defaultValue={0.002} size={38} label="Attack" onChange={(v) => updatePro(ch.id, { attack: v })} format={ms} />
        {!def?.oneShot && <Knob value={p.release} min={0.01} max={6} log defaultValue={0.25} size={38} label="Release" onChange={(v) => updatePro(ch.id, { release: v })} format={ms} />}
        <Knob value={p.tone} min={0} max={1} defaultValue={1} size={38} label="Tone" onChange={(v) => updatePro(ch.id, { tone: v })} format={(v) => (v >= 0.999 ? 'open' : pct(v))} />
        <Knob value={p.drive} min={0} max={1} defaultValue={0} size={38} label="Drive" onChange={(v) => updatePro(ch.id, { drive: v })} format={pct} />
        <Knob value={p.velocity} min={0} max={1} defaultValue={0.5} size={38} label="Velocity" onChange={(v) => updatePro(ch.id, { velocity: v })} format={pct} />
        <Knob value={p.level} min={0} max={1.5} defaultValue={0.9} size={38} label="Level" onChange={(v) => updatePro(ch.id, { level: v })} format={pct} />
        <button className="drum-hit" onPointerDown={hit}>{def?.oneShot ? 'HIT' : 'PLAY'}</button>
      </div>
      <div className="syn-row center">
        <button className={`btn small ${p.mono ? 'on' : ''}`} title="Only one note at a time (808 style)" onClick={() => updatePro(ch.id, { mono: !p.mono })}>Mono</button>
        {p.mono && !def?.oneShot && (
          <Knob value={p.glide} min={0} max={0.6} defaultValue={0.06} size={30} label="Glide" onChange={(v) => updatePro(ch.id, { glide: v })} format={(v) => (v < 0.002 ? 'off' : ms(v))} title="Overlapping notes slide into each other (808 slides)" />
        )}
        <button className={`btn small ${p.reverse ? 'on' : ''}`} onClick={() => updatePro(ch.id, { reverse: !p.reverse })}>Reverse</button>
        {p.mono && !def?.oneShot && <span className="label">Tip: let notes overlap in the piano roll to slide.</span>}
      </div>
    </div>
  );
}
