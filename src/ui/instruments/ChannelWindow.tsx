import { useEffect, useRef, useState } from 'react';
import { useProject } from '../../state/store';
import { closeWindow, type FloatingWindow } from '../../state/ui';
import type { Channel, DrumType, FilterType, LfoTarget, OscParams, SynthParams, Waveform } from '../../state/types';
import { updateChannel, updateDrum, updateSampler, updateSynth } from '../../state/actions';
import { DRUM_LABELS, SYNTH_PRESETS, defaultDrum } from '../../state/defaults';
import { DRUM_BUILDERS } from '../../audio/instruments/drums';
import { LIBRARY } from '../../audio/library';
import { assets, computePeaks } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { formatDb, formatHz, formatPan, rgba } from '../../state/utils';
import { isBlackKey, noteName } from '../../midi/music';
import { theme } from '../../styles/themes';
import { Window } from '../components/Window';
import { Knob } from '../components/Knob';
import { Icon } from '../components/Icon';
import { showMenu } from '../components/ContextMenu';
import { useCanvas } from '../visualizers/Visualizers';
import { readDragPayload } from '../browser/Browser';
import { decodeAudioFile } from '../../project/manager';
import './instruments.css';

const WAVES: Waveform[] = ['sine', 'sawtooth', 'square', 'triangle'];
const WAVE_LABEL: Record<Waveform, string> = { sine: 'Sine', sawtooth: 'Saw', square: 'Square', triangle: 'Tri' };

function WaveIcon({ wave }: { wave: Waveform }) {
  const d = { sine: 'M2 12 C6 2, 10 2, 12 12 S18 22, 22 12', sawtooth: 'M2 18 L12 6 L12 18 L22 6', square: 'M2 18 V6 H12 V18 H22 V6', triangle: 'M2 18 L7 6 L12 18 L17 6 L22 18' }[wave];
  return <svg width="22" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"><path d={d} /></svg>;
}

function MiniKeyboard({ channelId, root = 48, octaves = 3 }: { channelId: string; root?: number; octaves?: number }) {
  const [held, setHeld] = useState<number | null>(null);
  const down = useRef(false);
  const keys = Array.from({ length: octaves * 12 + 1 }, (_, i) => root + i);
  const whites = keys.filter((k) => !isBlackKey(k));
  const play = (p: number) => {
    if (held !== null) engine.previewNoteOff(channelId, held);
    engine.previewNote(channelId, p, 0.85);
    setHeld(p);
  };
  const release = () => {
    if (held !== null) engine.previewNoteOff(channelId, held);
    setHeld(null);
    down.current = false;
  };
  return (
    <div className="mini-kb" onPointerUp={release} onPointerLeave={release}>
      {whites.map((k) => {
        const black = isBlackKey(k + 1) && k + 1 <= keys[keys.length - 1] ? k + 1 : null;
        return (
          <div
            key={k}
            className={`mk-white ${held === k ? 'held' : ''}`}
            onPointerDown={(e) => { e.preventDefault(); down.current = true; play(k); }}
            onPointerEnter={() => { if (down.current) play(k); }}
          >
            {k % 12 === 0 && <span>{noteName(k)}</span>}
            {black && (
              <div
                className={`mk-black ${held === black ? 'held' : ''}`}
                onPointerDown={(e) => { e.preventDefault(); e.stopPropagation(); down.current = true; play(black); }}
                onPointerEnter={(e) => { e.stopPropagation(); if (down.current) play(black); }}
              />
            )}
          </div>
        );
      })}
    </div>
  );
}

function ChannelStrip({ ch }: { ch: Channel }) {
  const mixer = useProject((s) => s.project.mixer);
  return (
    <div className="inst-strip">
      <Knob value={ch.volume} min={0} max={1.25} defaultValue={0.8} size={30} label="Volume" format={formatDb} onChange={(v) => updateChannel(ch.id, { volume: v }, 'Volume')} />
      <Knob value={ch.pan} min={-1} max={1} defaultValue={0} size={30} label="Pan" bipolar format={formatPan} onChange={(v) => updateChannel(ch.id, { pan: v }, 'Pan')} />
      <label className="inst-field">
        <span className="knob-label">Mixer</span>
        <select className="input small-select" value={ch.mixerTrackId} onChange={(e) => updateChannel(ch.id, { mixerTrackId: e.target.value }, 'Route channel')}>
          {mixer.map((m, i) => <option key={m.id} value={m.id}>{i === 0 ? 'Master' : `${i} · ${m.name}`}</option>)}
        </select>
      </label>
      <label className="inst-field">
        <span className="knob-label">Step note</span>
        <select className="input small-select" value={ch.rootNote} onChange={(e) => updateChannel(ch.id, { rootNote: Number(e.target.value) }, 'Root note')}>
          {Array.from({ length: 61 }, (_, i) => 24 + i).map((n) => <option key={n} value={n}>{noteName(n)}</option>)}
        </select>
      </label>
    </div>
  );
}

// ---------------- synth ----------------
function OscSection({ title, o, onChange }: { title: string; o: OscParams; onChange: (p: Partial<OscParams>) => void }) {
  return (
    <div className="syn-section">
      <div className="syn-title">{title}</div>
      <div className="syn-waves">
        {WAVES.map((w) => (
          <button key={w} className={`syn-wave ${o.wave === w ? 'on' : ''}`} title={WAVE_LABEL[w]} onClick={() => onChange({ wave: w })}><WaveIcon wave={w} /></button>
        ))}
      </div>
      <div className="syn-row">
        <Knob value={o.octave} min={-3} max={3} step={1} defaultValue={0} size={30} label="Octave" bipolar onChange={(v) => onChange({ octave: Math.round(v) })} format={(v) => `${v > 0 ? '+' : ''}${Math.round(v)}`} />
        <Knob value={o.semi} min={-12} max={12} step={1} defaultValue={0} size={30} label="Semi" bipolar onChange={(v) => onChange({ semi: Math.round(v) })} format={(v) => `${v > 0 ? '+' : ''}${Math.round(v)} st`} />
        <Knob value={o.detune} min={-100} max={100} defaultValue={0} size={30} label="Detune" bipolar onChange={(v) => onChange({ detune: v })} format={(v) => `${Math.round(v)} ct`} />
        <Knob value={o.level} min={0} max={1} defaultValue={0.7} size={30} label="Level" onChange={(v) => onChange({ level: v })} format={(v) => `${Math.round(v * 100)}%`} />
      </div>
    </div>
  );
}

function EnvGraph({ p }: { p: SynthParams }) {
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const total = p.attack + p.decay + 0.4 + p.release;
    const x = (s: number) => 4 + (s / total) * (w - 8);
    const y = (v: number) => h - 4 - v * (h - 10);
    ctx.beginPath();
    ctx.moveTo(x(0), y(0));
    ctx.lineTo(x(p.attack), y(1));
    ctx.lineTo(x(p.attack + p.decay), y(p.sustain));
    ctx.lineTo(x(p.attack + p.decay + 0.4), y(p.sustain));
    ctx.lineTo(x(total), y(0));
    ctx.strokeStyle = t.accent;
    ctx.lineWidth = 2;
    ctx.stroke();
    ctx.lineTo(x(0), y(0));
    ctx.fillStyle = rgba(t.accent, 0.15);
    ctx.fill();
  });
  return <canvas ref={ref} className="syn-env" />;
}

function WavePreview({ p }: { p: SynthParams }) {
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const osc = (wave: Waveform, ph: number) => {
      const f = ph - Math.floor(ph);
      switch (wave) {
        case 'sine': return Math.sin(f * Math.PI * 2);
        case 'sawtooth': return 2 * f - 1;
        case 'square': return f < 0.5 ? 1 : -1;
        default: return 1 - 4 * Math.abs(f - 0.5);
      }
    };
    const r2 = Math.pow(2, p.osc2.octave - p.osc1.octave + (p.osc2.semi - p.osc1.semi) / 12);
    ctx.beginPath();
    for (let x = 0; x < w; x++) {
      const ph = (x / w) * 2;
      const v = (osc(p.osc1.wave, ph) * p.osc1.level + osc(p.osc2.wave, ph * r2) * p.osc2.level) / Math.max(1, p.osc1.level + p.osc2.level);
      const y = h / 2 - v * (h / 2 - 4);
      if (x === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
    }
    ctx.strokeStyle = t.accent2;
    ctx.lineWidth = 1.5;
    ctx.shadowColor = t.accent;
    ctx.shadowBlur = 6;
    ctx.stroke();
    ctx.shadowBlur = 0;
  });
  return <canvas ref={ref} className="syn-wave-preview" />;
}

function SynthEditor({ ch }: { ch: Channel }) {
  const p = ch.synth!;
  const set = (patch: Partial<SynthParams>) => updateSynth(ch.id, patch);
  return (
    <div className="synth">
      <div className="syn-header">
        <WavePreview p={p} />
        <button className="btn small" onClick={(e) => showMenu(e.clientX, e.clientY, SYNTH_PRESETS.map((pr) => ({
          label: `${pr.category} · ${pr.name}`, onClick: () => updateChannel(ch.id, { synth: structuredClone(pr.params) }, `Preset ${pr.name}`),
        })))}>
          <Icon name="preset" size={11} /> Presets
        </button>
      </div>
      <div className="syn-grid">
        <OscSection title="Oscillator 1" o={p.osc1} onChange={(o) => set({ osc1: { ...p.osc1, ...o } })} />
        <OscSection title="Oscillator 2" o={p.osc2} onChange={(o) => set({ osc2: { ...p.osc2, ...o } })} />
        <div className="syn-section">
          <div className="syn-title">Filter</div>
          <div className="syn-waves">
            {(['lowpass', 'highpass', 'bandpass'] as FilterType[]).map((f) => (
              <button key={f} className={`syn-wave text ${p.filterType === f ? 'on' : ''}`} onClick={() => set({ filterType: f })}>{f === 'lowpass' ? 'LP' : f === 'highpass' ? 'HP' : 'BP'}</button>
            ))}
          </div>
          <div className="syn-row">
            <Knob value={p.cutoff} min={30} max={18000} log defaultValue={2400} size={34} label="Cutoff" onChange={(v) => set({ cutoff: v })} format={formatHz} />
            <Knob value={p.resonance} min={0.1} max={20} log defaultValue={2} size={34} label="Reso" onChange={(v) => set({ resonance: v })} format={(v) => v.toFixed(2)} />
            <Knob value={p.filterEnv} min={0} max={1} defaultValue={0.35} size={34} label="Env Amt" onChange={(v) => set({ filterEnv: v })} format={(v) => `${Math.round(v * 100)}%`} />
          </div>
        </div>
        <div className="syn-section">
          <div className="syn-title">Amp Envelope (ADSR)</div>
          <EnvGraph p={p} />
          <div className="syn-row">
            <Knob value={p.attack} min={0.001} max={4} log defaultValue={0.005} size={30} label="Attack" onChange={(v) => set({ attack: v })} format={(v) => `${(v * 1000).toFixed(0)} ms`} />
            <Knob value={p.decay} min={0.005} max={4} log defaultValue={0.3} size={30} label="Decay" onChange={(v) => set({ decay: v })} format={(v) => `${(v * 1000).toFixed(0)} ms`} />
            <Knob value={p.sustain} min={0} max={1} defaultValue={0.6} size={30} label="Sustain" onChange={(v) => set({ sustain: v })} format={(v) => `${Math.round(v * 100)}%`} />
            <Knob value={p.release} min={0.005} max={6} log defaultValue={0.25} size={30} label="Release" onChange={(v) => set({ release: v })} format={(v) => `${(v * 1000).toFixed(0)} ms`} />
          </div>
        </div>
        <div className="syn-section">
          <div className="syn-title">LFO</div>
          <div className="syn-waves">
            {WAVES.map((w) => (
              <button key={w} className={`syn-wave ${p.lfoWave === w ? 'on' : ''}`} onClick={() => set({ lfoWave: w })}><WaveIcon wave={w} /></button>
            ))}
          </div>
          <div className="syn-waves">
            {(['pitch', 'filter', 'amp'] as LfoTarget[]).map((tg) => (
              <button key={tg} className={`syn-wave text ${p.lfoTarget === tg ? 'on' : ''}`} onClick={() => set({ lfoTarget: tg })}>{tg}</button>
            ))}
          </div>
          <div className="syn-row">
            <Knob value={p.lfoRate} min={0.05} max={20} log defaultValue={5} size={30} label="Rate" onChange={(v) => set({ lfoRate: v })} format={(v) => `${v.toFixed(2)} Hz`} />
            <Knob value={p.lfoDepth} min={0} max={1} defaultValue={0} size={30} label="Depth" onChange={(v) => set({ lfoDepth: v })} format={(v) => `${Math.round(v * 100)}%`} />
          </div>
        </div>
        <div className="syn-section">
          <div className="syn-title">Voice</div>
          <div className="syn-row">
            <Knob value={p.unisonDetune} min={0} max={50} defaultValue={0} size={30} label="Unison" onChange={(v) => set({ unisonDetune: v })} format={(v) => (v < 0.5 ? 'off' : `±${Math.round(v)} ct`)} />
            <Knob value={p.glide} min={0} max={1} defaultValue={0} size={30} label="Glide" onChange={(v) => set({ glide: v })} format={(v) => `${Math.round(v * 1000)} ms`} />
            <Knob value={p.volume} min={0} max={1} defaultValue={0.8} size={30} label="Volume" onChange={(v) => set({ volume: v })} format={(v) => `${Math.round(v * 100)}%`} />
          </div>
        </div>
      </div>
    </div>
  );
}

// ---------------- drum ----------------
function DrumWave({ ch }: { ch: Channel }) {
  const [peaks, setPeaks] = useState<Float32Array | null>(null);
  const d = ch.drum!;
  useEffect(() => {
    let cancelled = false;
    const t = setTimeout(async () => {
      const ctx = new OfflineAudioContext(1, 44100 * 1.2, 44100);
      DRUM_BUILDERS[d.type](ctx, ctx.destination, d, 0, 1);
      const buf = await ctx.startRendering();
      if (!cancelled) setPeaks(computePeaks(buf, 128).data);
    }, 120);
    return () => { cancelled = true; clearTimeout(t); };
  }, [d]);
  const ref = useCanvas((ctx, w, h) => {
    const t = theme();
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    if (!peaks) return;
    const n = peaks.length / 2;
    ctx.fillStyle = ch.color;
    for (let x = 0; x < w; x++) {
      const i = Math.floor((x / w) * n);
      const mn = peaks[i * 2];
      const mx = peaks[i * 2 + 1];
      ctx.fillRect(x, h / 2 - mx * (h / 2 - 2), 1, Math.max(1, (mx - mn) * (h / 2 - 2)));
    }
    void t;
  }, [peaks]);
  return <canvas ref={ref} className="drum-wave" />;
}

function DrumEditor({ ch }: { ch: Channel }) {
  const d = ch.drum!;
  return (
    <div className="drum-edit">
      <div className="drum-types">
        {(Object.keys(DRUM_LABELS) as DrumType[]).map((t) => (
          <button key={t} className={`syn-wave text ${d.type === t ? 'on' : ''}`} onClick={() => { updateChannel(ch.id, { drum: { ...defaultDrum(t), level: d.level } }, 'Drum type'); engine.previewNote(ch.id, 60); }}>{DRUM_LABELS[t]}</button>
        ))}
      </div>
      <DrumWave ch={ch} />
      <div className="syn-row center">
        <Knob value={d.tune} min={-24} max={24} defaultValue={0} size={40} label="Tune" bipolar onChange={(v) => updateDrum(ch.id, { tune: v })} format={(v) => `${v > 0 ? '+' : ''}${v.toFixed(1)} st`} />
        <Knob value={d.decay} min={0.1} max={3} log defaultValue={1} size={40} label="Decay" onChange={(v) => updateDrum(ch.id, { decay: v })} format={(v) => `${Math.round(v * 100)}%`} />
        <Knob value={d.tone} min={0} max={1} defaultValue={0.5} size={40} label="Tone" onChange={(v) => updateDrum(ch.id, { tone: v })} format={(v) => `${Math.round(v * 100)}%`} />
        <Knob value={d.snap} min={0} max={1} defaultValue={0.5} size={40} label="Snap" onChange={(v) => updateDrum(ch.id, { snap: v })} format={(v) => `${Math.round(v * 100)}%`} />
        <Knob value={d.level} min={0} max={1.2} defaultValue={0.9} size={40} label="Level" onChange={(v) => updateDrum(ch.id, { level: v })} format={(v) => `${Math.round(v * 100)}%`} />
        <button className="drum-hit" onPointerDown={() => engine.previewNote(ch.id, 60, 1)}>HIT</button>
      </div>
    </div>
  );
}

// ---------------- sampler ----------------
function SampleWave({ assetId, color }: { assetId: string | null; color: string }) {
  const ref = useCanvas((ctx, w, h) => {
    ctx.fillStyle = '#08090b';
    ctx.fillRect(0, 0, w, h);
    const peaks = assetId ? assets.getPeaks(assetId) : undefined;
    if (!peaks) {
      ctx.fillStyle = theme().dim;
      ctx.font = '11px Inter, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('Drop a sample here or choose one below', w / 2, h / 2);
      ctx.textAlign = 'left';
      return;
    }
    const n = peaks.data.length / 2;
    ctx.fillStyle = color;
    for (let x = 0; x < w; x++) {
      const i0 = Math.floor((x / w) * n);
      const i1 = Math.max(i0 + 1, Math.floor(((x + 1) / w) * n));
      let mn = 0;
      let mx = 0;
      for (let i = i0; i < i1; i++) { mn = Math.min(mn, peaks.data[i * 2]); mx = Math.max(mx, peaks.data[i * 2 + 1]); }
      ctx.fillRect(x, h / 2 - mx * (h / 2 - 2), 1, Math.max(1, (mx - mn) * (h / 2 - 2)));
    }
  }, [assetId]);
  return <canvas ref={ref} className="drum-wave tall" />;
}

function SamplerEditor({ ch }: { ch: Channel }) {
  const s = ch.sampler!;
  const projectAssets = useProject((st) => st.project.assets);
  const options = [
    ...LIBRARY.map((l) => ({ id: l.id, name: `${l.category} · ${l.name}` })),
    ...Object.values(projectAssets).filter((a) => !a.builtin).map((a) => ({ id: a.id, name: `Project · ${a.name}` })),
  ];
  if (s.assetId && !options.some((o) => o.id === s.assetId)) options.push({ id: s.assetId, name: assets.name(s.assetId) });
  const buf = s.assetId ? assets.get(s.assetId) : undefined;
  return (
    <div
      className="drum-edit"
      onDragOver={(e) => e.preventDefault()}
      onDrop={async (e) => {
        e.preventDefault();
        const f = e.dataTransfer.files?.[0];
        if (f) {
          const res = await decodeAudioFile(f);
          if (res) { updateSampler(ch.id, { assetId: res.id }); updateChannel(ch.id, { name: res.name }, 'Load sample'); }
          return;
        }
        const p = readDragPayload(e);
        if (p?.type === 'sample') { updateSampler(ch.id, { assetId: p.assetId }); updateChannel(ch.id, { name: p.name }, 'Load sample'); }
      }}
    >
      <SampleWave assetId={s.assetId} color={ch.color} />
      <div className="syn-row center">
        <select className="input" style={{ maxWidth: 260 }} value={s.assetId ?? ''} onChange={(e) => {
          const id = e.target.value || null;
          const lib = LIBRARY.find((l) => l.id === id);
          updateSampler(ch.id, { assetId: id, rootNote: lib?.root ?? s.rootNote });
          if (id) updateChannel(ch.id, { name: lib?.name ?? assets.name(id) }, 'Load sample');
        }}>
          <option value="">— no sample —</option>
          {options.map((o) => <option key={o.id} value={o.id}>{o.name}</option>)}
        </select>
        <span className="label">{buf ? `${buf.duration.toFixed(2)} s · ${buf.numberOfChannels} ch · ${buf.sampleRate} Hz` : ''}</span>
      </div>
      <div className="syn-row center">
        <label className="inst-field">
          <span className="knob-label">Root</span>
          <select className="input small-select" value={s.rootNote} onChange={(e) => updateSampler(ch.id, { rootNote: Number(e.target.value) })}>
            {Array.from({ length: 73 }, (_, i) => 24 + i).map((n) => <option key={n} value={n}>{noteName(n)}</option>)}
          </select>
        </label>
        <Knob value={s.tune} min={-24} max={24} defaultValue={0} size={36} label="Tune" bipolar onChange={(v) => updateSampler(ch.id, { tune: v })} format={(v) => `${v > 0 ? '+' : ''}${v.toFixed(2)} st`} />
        <Knob value={s.attack} min={0.001} max={2} log defaultValue={0.002} size={36} label="Attack" onChange={(v) => updateSampler(ch.id, { attack: v })} format={(v) => `${Math.round(v * 1000)} ms`} />
        <Knob value={s.release} min={0.005} max={4} log defaultValue={0.1} size={36} label="Release" onChange={(v) => updateSampler(ch.id, { release: v })} format={(v) => `${Math.round(v * 1000)} ms`} />
        <Knob value={s.level} min={0} max={1.5} defaultValue={0.9} size={36} label="Level" onChange={(v) => updateSampler(ch.id, { level: v })} format={(v) => `${Math.round(v * 100)}%`} />
        <button className={`btn small ${s.reverse ? 'on' : ''}`} onClick={() => updateSampler(ch.id, { reverse: !s.reverse })}>Reverse</button>
      </div>
    </div>
  );
}

export function ChannelWindow({ w, channelId }: { w: FloatingWindow; channelId: string }) {
  const ch = useProject((s) => s.project.channels.find((c) => c.id === channelId));
  useEffect(() => {
    if (!ch) closeWindow(w.id);
  }, [ch, w.id]);
  if (!ch) return null;
  const title = ch.kind === 'synth' ? 'Goofy Synth' : ch.kind === 'drum' ? 'Goofy Drum' : 'Goofy Sampler';
  return (
    <Window w={w} width={ch.kind === 'synth' ? 640 : 560} color={ch.color} title={<><b>{ch.name}</b> <span className="crumb">· {title}</span></>}>
      <div className="inst-body" style={{ ['--cc' as string]: ch.color }}>
        <ChannelStrip ch={ch} />
        {ch.kind === 'synth' && ch.synth && <SynthEditor ch={ch} />}
        {ch.kind === 'drum' && ch.drum && <DrumEditor ch={ch} />}
        {ch.kind === 'sampler' && ch.sampler && <SamplerEditor ch={ch} />}
        <MiniKeyboard channelId={ch.id} root={ch.kind === 'drum' ? 48 : 48} />
      </div>
    </Window>
  );
}

