// Sample editor: select a region of the waveform, cut / trim / fade /
// normalize / reverse / change pitch, preview effect chains live and print
// them into the sample, then save to "My Library".
import { useEffect, useMemo, useRef, useState } from 'react';
import type { EffectInstance, EffectType, ParamValue } from '../../state/types';
import { assets, computePeaks } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { InsertChain } from '../../audio/mixgraph';
import { renderWithEffects, reverseBuffer } from '../../audio/offlineFx';
import { makeEffect } from '../../state/defaults';
import { closeWindow, openWindow, setUI, toast, ui, type FloatingWindow } from '../../state/ui';
import { getProject } from '../../state/store';
import { addChannel, ensureTrackInsert } from '../../state/actions';
import { placeAudioClip } from '../../project/manager';
import { clamp, linToDb, rgba } from '../../state/utils';
import { theme } from '../../styles/themes';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { Knob } from '../components/Knob';
import { useCanvas } from '../visualizers/Visualizers';
import { FxChainEditor, type ChainOps } from '../effects/FxChainEditor';
import { askText } from '../dialogs/askText';
import { addToLibrary, isLibrarySample, updateLibrarySample, useLibrary } from './libraryStore';
import './library.css';

function cloneBuffer(b: AudioBuffer, from = 0, to = b.length): AudioBuffer {
  const len = Math.max(1, to - from);
  const out = new AudioBuffer({ length: len, numberOfChannels: b.numberOfChannels, sampleRate: b.sampleRate });
  for (let c = 0; c < b.numberOfChannels; c++) out.copyToChannel(b.getChannelData(c).subarray(from, from + len), c);
  return out;
}

function concat(parts: AudioBuffer[]): AudioBuffer {
  const ch = Math.max(...parts.map((p) => p.numberOfChannels));
  const len = parts.reduce((n, p) => n + p.length, 0);
  const out = new AudioBuffer({ length: Math.max(1, len), numberOfChannels: ch, sampleRate: parts[0].sampleRate });
  for (let c = 0; c < ch; c++) {
    const d = out.getChannelData(c);
    let o = 0;
    for (const p of parts) {
      d.set(p.getChannelData(Math.min(c, p.numberOfChannels - 1)), o);
      o += p.length;
    }
  }
  return out;
}

/** Applies `fn` to every sample in [s0, s1) of a copy of the buffer. */
function mapRange(b: AudioBuffer, s0: number, s1: number, fn: (v: number, i: number, n: number) => number): AudioBuffer {
  const out = cloneBuffer(b);
  for (let c = 0; c < out.numberOfChannels; c++) {
    const d = out.getChannelData(c);
    const n = s1 - s0;
    for (let i = s0; i < s1; i++) d[i] = fn(d[i], i - s0, n);
  }
  return out;
}

async function resample(b: AudioBuffer, rate: number): Promise<AudioBuffer> {
  const ctx = new OfflineAudioContext(b.numberOfChannels, Math.max(1, Math.ceil(b.length / rate)), b.sampleRate);
  const src = ctx.createBufferSource();
  src.buffer = b;
  src.playbackRate.value = rate;
  src.connect(ctx.destination);
  src.start();
  return ctx.startRendering();
}

export function SampleEditorWindow({ w, assetId }: { w: FloatingWindow; assetId: string }) {
  const [buffer, setBuffer] = useState<AudioBuffer | null>(() => assets.get(assetId) ?? null);
  const [history, setHistory] = useState<AudioBuffer[]>([]);
  const [sel, setSel] = useState<[number, number] | null>(null); // seconds
  const [effects, setEffects] = useState<EffectInstance[]>([]);
  const [semis, setSemis] = useState(0);
  const [busy, setBusy] = useState(false);
  const [dirty, setDirty] = useState(false);
  const own = useLibrary((s) => s.samples.some((x) => x.id === assetId));
  const name = useLibrary((s) => s.samples.find((x) => x.id === assetId)?.name) ?? assets.name(assetId);
  const play = useRef<{ src: AudioBufferSourceNode; start: number; offset: number } | null>(null);
  const chain = useRef<InsertChain | null>(null);
  const drag = useRef<number | null>(null);

  useEffect(() => { if (!buffer) closeWindow(w.id); }, [buffer, w.id]);

  // live preview chain (hear the effects before applying them)
  useEffect(() => {
    if (!engine.initialized) return;
    const c = new InsertChain(engine.ctx);
    c.output.connect(engine.masterOut);
    chain.current = c;
    return () => {
      try { play.current?.src.stop(); } catch { /* ignore */ }
      c.dispose();
      chain.current = null;
    };
  }, []);
  useEffect(() => { chain.current?.apply(effects, { bpm: getProject().bpm }); }, [effects]);

  const peaks = useMemo(() => (buffer ? computePeaks(buffer, Math.max(16, Math.floor(buffer.length / 4000))) : null), [buffer]);

  const ref = useCanvas((ctx, cw, ch) => {
    const t = theme();
    ctx.fillStyle = '#07080a';
    ctx.fillRect(0, 0, cw, ch);
    if (!buffer || !peaks) return;
    const n = peaks.data.length / 2;
    const mid = ch / 2;
    if (sel) {
      ctx.fillStyle = rgba(t.accent, 0.18);
      const x0 = (sel[0] / buffer.duration) * cw;
      const x1 = (sel[1] / buffer.duration) * cw;
      ctx.fillRect(x0, 0, x1 - x0, ch);
      ctx.fillStyle = t.accent;
      ctx.fillRect(x0, 0, 1, ch);
      ctx.fillRect(x1, 0, 1, ch);
    }
    ctx.fillStyle = rgba(t.text, 0.08);
    ctx.fillRect(0, mid, cw, 1);
    ctx.fillStyle = t.accent2;
    for (let x = 0; x < cw; x++) {
      const i0 = Math.floor((x / cw) * n);
      const i1 = Math.max(i0 + 1, Math.floor(((x + 1) / cw) * n));
      let mn = 0;
      let mx = 0;
      for (let i = i0; i < i1; i++) { mn = Math.min(mn, peaks.data[i * 2]); mx = Math.max(mx, peaks.data[i * 2 + 1]); }
      ctx.fillRect(x, mid - mx * (mid - 4), 1, Math.max(1, (mx - mn) * (mid - 4)));
    }
    const pl = play.current;
    if (pl && engine.initialized) {
      const pos = pl.offset + (engine.ctx.currentTime - pl.start);
      if (pos <= buffer.duration) {
        ctx.fillStyle = '#fff';
        ctx.fillRect((pos / buffer.duration) * cw, 0, 1.5, ch);
      }
    }
    ctx.fillStyle = t.dim;
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(`${buffer.duration.toFixed(3)} s · ${buffer.numberOfChannels} ch · ${buffer.sampleRate} Hz`, 6, 13);
    if (sel) ctx.fillText(`selection ${sel[0].toFixed(3)} – ${sel[1].toFixed(3)} s (${(sel[1] - sel[0]).toFixed(3)} s)`, 6, ch - 6);
  }, [buffer, peaks, sel]);

  if (!buffer) return null;
  const sr = buffer.sampleRate;
  const range = (): [number, number] => (sel ? [Math.floor(sel[0] * sr), Math.ceil(sel[1] * sr)] : [0, buffer.length]);

  const commit = (b: AudioBuffer, keepSel = false) => {
    setHistory((h) => [...h.slice(-19), buffer]);
    setBuffer(b);
    setDirty(true);
    if (!keepSel) setSel(null);
  };

  const stop = () => {
    try { play.current?.src.stop(); } catch { /* ignore */ }
    play.current = null;
  };
  const preview = () => {
    if (!engine.initialized || !chain.current) return;
    void engine.resume();
    stop();
    const [s0, s1] = sel ?? [0, buffer.duration];
    const src = engine.ctx.createBufferSource();
    src.buffer = buffer;
    src.connect(chain.current.input);
    const start = engine.ctx.currentTime + 0.02;
    src.start(start, s0, Math.max(0.01, s1 - s0));
    src.onended = () => { if (play.current?.src === src) play.current = null; };
    play.current = { src, start, offset: s0 };
  };

  const posOf = (e: React.PointerEvent) => {
    const r = (e.currentTarget as HTMLElement).getBoundingClientRect();
    return clamp(((e.clientX - r.left) / r.width) * buffer.duration, 0, buffer.duration);
  };

  const ops: ChainOps = {
    add: (type: EffectType) => setEffects((l) => [...l, makeEffect(type)]),
    remove: (id) => setEffects((l) => l.filter((e) => e.id !== id)),
    move: (id, dir) => setEffects((l) => {
      const i = l.findIndex((e) => e.id === id);
      const j = i + dir;
      if (i < 0 || j < 0 || j >= l.length) return l;
      const c = [...l];
      const [x] = c.splice(i, 1);
      c.splice(j, 0, x);
      return c;
    }),
    bypass: (id) => setEffects((l) => l.map((e) => (e.id === id ? { ...e, bypass: !e.bypass } : e))),
    param: (id, k, v: ParamValue) => setEffects((l) => l.map((e) => (e.id === id ? { ...e, params: { ...e.params, [k]: v } } : e))),
    setParams: (id, params) => setEffects((l) => l.map((e) => (e.id === id ? { ...e, params: { ...e.params, ...params } } : e))),
    clear: () => setEffects([]),
    applyChain: (list, mode) => setEffects((l) => [...(mode === 'replace' ? [] : l), ...list.map(([type, params]) => makeEffect(type, params))]),
  };

  const applyEffects = async () => {
    if (!effects.length) return;
    setBusy(true);
    stop();
    try {
      const [s0, s1] = range();
      const part = cloneBuffer(buffer, s0, s1);
      const wet = await renderWithEffects(part, effects, { bpm: getProject().bpm });
      const before = s0 > 0 ? [cloneBuffer(buffer, 0, s0)] : [];
      const after = s1 < buffer.length ? [cloneBuffer(buffer, s1, buffer.length)] : [];
      commit(sel ? concat([...before, wet, ...after]) : wet);
      setEffects([]);
      toast('Effects applied', 'ok');
    } finally {
      setBusy(false);
    }
  };

  const save = async (asNew: boolean) => {
    stop();
    if (!asNew && own) {
      await updateLibrarySample(assetId, buffer);
      setDirty(false);
      toast(`Saved “${name}”`, 'ok');
      return;
    }
    const n = await askText('Save sample as', `${name}${own ? ' copy' : ''}`);
    if (!n) return;
    const id = await addToLibrary(buffer, n);
    setDirty(false);
    toast(`Saved “${n}” to My Library`, 'ok');
    closeWindow(w.id);
    openWindow({ kind: 'sampleEditor', assetId: id });
  };

  const sButton = (label: string, title: string, fn: () => void | Promise<void>, disabled = false) => (
    <button className="btn small" title={title} disabled={busy || disabled} onClick={() => void fn()}>{label}</button>
  );

  const [s0, s1] = range();
  const peakDb = (() => {
    let pk = 0;
    for (let c = 0; c < buffer.numberOfChannels; c++) {
      const d = buffer.getChannelData(c);
      for (let i = s0; i < s1; i += 4) pk = Math.max(pk, Math.abs(d[i]));
    }
    return linToDb(pk);
  })();

  return (
    <Window w={w} width={780} color="#9be15d" title={<><b>{name}</b> <span className="crumb">· sample editor{dirty ? ' · edited' : ''}{own ? '' : ' · built-in (saved as a copy)'}</span></>}>
      <div className="se-body">
        <canvas
          ref={ref}
          className="se-wave"
          onPointerDown={(e) => {
            (e.currentTarget as Element).setPointerCapture(e.pointerId);
            const t = posOf(e);
            drag.current = t;
            setSel(null);
          }}
          onPointerMove={(e) => {
            if (drag.current === null) return;
            const t = posOf(e);
            const a = Math.min(drag.current, t);
            const b = Math.max(drag.current, t);
            setSel(b - a > 0.002 ? [a, b] : null);
          }}
          onPointerUp={() => { drag.current = null; }}
          onDoubleClick={() => setSel(null)}
        />
        <div className="se-toolbar">
          <button className="btn small" onClick={preview} title="Play the selection (or everything) through the effects below"><Icon name="play" size={10} /> Play</button>
          <button className="btn small" onClick={stop}><Icon name="stop" size={10} /> Stop</button>
          <button className="btn small" disabled={!history.length || busy} onClick={() => { const prev = history[history.length - 1]; setHistory((h) => h.slice(0, -1)); setBuffer(prev); setSel(null); }}><Icon name="undo" size={10} /> Undo</button>
          <span className="se-sep" />
          {sButton('Trim to selection', 'Keep only the selected part', () => commit(cloneBuffer(buffer, s0, s1)), !sel)}
          {sButton('Cut', 'Delete the selected part', () => commit(concat([cloneBuffer(buffer, 0, Math.max(1, s0)), cloneBuffer(buffer, s1, buffer.length)])), !sel)}
          {sButton('Silence', 'Make the selection silent', () => commit(mapRange(buffer, s0, s1, () => 0), true), !sel)}
          {sButton('Fade in', 'Fade in over the selection (or whole sample)', () => commit(mapRange(buffer, s0, s1, (v, i, n) => v * (i / n)), true))}
          {sButton('Fade out', 'Fade out over the selection (or whole sample)', () => commit(mapRange(buffer, s0, s1, (v, i, n) => v * (1 - i / n)), true))}
          {sButton('Normalize', 'Make the loudest point 0 dB', () => {
            const g = Math.pow(10, -peakDb / 20) * 0.98;
            if (isFinite(g)) commit(mapRange(buffer, s0, s1, (v) => v * g), true);
          })}
          {sButton('+3 dB', 'Louder', () => commit(mapRange(buffer, s0, s1, (v) => v * 1.4125), true))}
          {sButton('−3 dB', 'Quieter', () => commit(mapRange(buffer, s0, s1, (v) => v / 1.4125), true))}
          {sButton('Reverse', 'Play backwards', () => {
            if (!sel) return commit(reverseBuffer(buffer));
            const mid = reverseBuffer(cloneBuffer(buffer, s0, s1));
            commit(concat([...(s0 > 0 ? [cloneBuffer(buffer, 0, s0)] : []), mid, ...(s1 < buffer.length ? [cloneBuffer(buffer, s1, buffer.length)] : [])]));
          })}
        </div>
        <div className="se-toolbar">
          <Knob value={semis} min={-24} max={24} step={1} defaultValue={0} size={30} bipolar label="Pitch" format={(v) => `${v > 0 ? '+' : ''}${Math.round(v)} st`} onChange={(v) => setSemis(Math.round(v))} />
          {sButton('Apply pitch + speed', 'Resample: higher pitch = shorter (like a tape)', async () => {
            if (!semis) return;
            setBusy(true);
            commit(await resample(buffer, Math.pow(2, semis / 12)));
            setSemis(0);
            setBusy(false);
          }, !semis)}
          <span className="label">Peak {isFinite(peakDb) ? peakDb.toFixed(1) : '-∞'} dB</span>
          <span className="spacer" />
          <button className="btn small" title="Put the sample on the selected playlist row at the playhead" onClick={() => {
            const p = getProject();
            const trackId = ui().selectedTrackId ?? p.tracks[0].id;
            const id = assetId;
            if (dirty) { toast('Save first so the playlist uses the edited sound', 'info'); return; }
            ensureTrackInsert(trackId);
            placeAudioClip(id, trackId, Math.round(engine.transport?.getTick() ?? 0));
            toast('Added to the playlist', 'ok');
          }}><Icon name="playlist" size={10} /> To playlist</button>
          <button className="btn small" onClick={() => { if (dirty) { toast('Save first', 'info'); return; } setUI({ selectedChannelId: addChannel('sampler', { assetId, name }) }); toast('Sampler channel added', 'ok'); }}><Icon name="sample" size={10} /> To sampler</button>
          {own && <button className="btn small primary" disabled={!dirty || busy} onClick={() => void save(false)}><Icon name="save" size={10} /> Save</button>}
          <button className="btn small" disabled={busy} onClick={() => void save(true)}>Save as new…</button>
        </div>
        <div className="se-fx">
          <FxChainEditor ops={ops} effects={effects} title="Effects (preview with Play, then apply)" />
          <div className="se-fx-actions">
            <button className="btn primary" disabled={!effects.length || busy} onClick={() => void applyEffects()}>{busy ? 'Working…' : sel ? 'Apply effects to selection' : 'Apply effects to sample'}</button>
          </div>
        </div>
        {!isLibrarySample(assetId) && <p className="set-hint">This is a built-in sound – your changes are saved as a new sample in “My Library”.</p>}
      </div>
    </Window>
  );
}
