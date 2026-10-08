import { useEffect, useRef, useState } from 'react';
import { useProject, getProject } from '../../state/store';
import { setUI, toast, useUI } from '../../state/ui';
import { addInsert, setEffectParam, toggleBypass } from '../../state/actions';
import { engine } from '../../audio/engine';
import { AudioInput, type DeviceInfo } from '../../audio/input';
import { Knob } from '../components/Knob';
import { Meter } from '../components/Meter';
import { Icon } from '../components/Icon';
import { onFrame } from '../components/frameLoop';
import { formatDb } from '../../state/utils';
import { PitchView } from '../effects/views';
import { VOCAL_CHAINS, applyVocalChain } from '../browser/vocalChains';
import { toggleRecord } from '../commands';
import '../effects/effects.css';
import './vocal.css';

function inputLevel() {
  if (!engine.initialized || !engine.input?.isOpen) return null;
  const v = engine.input.level();
  return { l: v, r: v };
}

function RecTimer() {
  const ref = useRef<HTMLSpanElement>(null);
  useEffect(() => onFrame(() => {
    const cap = engine.input?.capture;
    if (ref.current) ref.current.textContent = cap ? `${(cap.samples / engine.ctx.sampleRate).toFixed(1)} s` : '';
  }), []);
  return <span ref={ref} className="vc-timer" />;
}

export function VocalPanel() {
  const mixer = useProject((s) => s.project.mixer);
  const tracks = useProject((s) => s.project.tracks);
  const inputDeviceId = useUI((s) => s.inputDeviceId);
  const inputGain = useUI((s) => s.inputGain);
  const monitoring = useUI((s) => s.monitoring);
  const recording = useUI((s) => s.recording);
  const selectedTrackId = useUI((s) => s.selectedTrackId);
  const [devices, setDevices] = useState<DeviceInfo[]>([]);
  const [open, setOpen] = useState(engine.input?.isOpen ?? false);
  const [, force] = useState(0);
  const [latencyMs, setLatencyMs] = useState(() => Math.round((engine.initialized ? engine.roundTripLatency : 0.02) * 1000));

  const targetId = engine.initialized ? engine.monitorTarget(getProject()) : 'master';
  const target = mixer.find((m) => m.id === targetId) ?? mixer[0];
  const pitchFx = target.inserts.find((i) => i.type === 'pitch');

  const refresh = async () => setDevices((await AudioInput.listDevices()).inputs);
  useEffect(() => { void refresh(); }, [open]);

  const openInput = async (id = inputDeviceId) => {
    await engine.resume();
    const ok = await engine.input.open(id);
    setOpen(ok);
    if (ok) {
      setUI({ inputDeviceId: id });
      engine.input.setGain(inputGain);
      engine.input.setMonitoring(monitoring);
      engine.sync(getProject());
      toast('Audio input connected', 'ok');
    } else {
      toast(engine.input.error ?? 'Could not open the input', 'error');
    }
  };

  const recTrack = tracks.find((t) => t.id === selectedTrackId) ?? tracks.find((t) => t.name.toLowerCase().includes('vocal')) ?? tracks[0];

  return (
    <div className="vc">
      <div className="vc-input">
        <div className="vc-title"><Icon name="mic" /> Audio Input</div>
        <label className="vc-field">
          <span className="label">Device</span>
          <div className="vc-row">
            <select className="input" value={inputDeviceId} onChange={(e) => { setUI({ inputDeviceId: e.target.value }); if (open) void openInput(e.target.value); }}>
              <option value="">System default</option>
              {devices.map((d) => <option key={d.deviceId} value={d.deviceId}>{d.label}</option>)}
            </select>
            <button className="btn icon" title="Refresh devices" onClick={() => void refresh()}><Icon name="loop" size={12} /></button>
          </div>
        </label>
        <div className="vc-row">
          <button className={`btn ${open ? 'on' : ''}`} onClick={() => (open ? (engine.input.close(), setOpen(false)) : void openInput())}>
            <Icon name="power" size={12} /> {open ? 'Input on' : 'Enable input'}
          </button>
          <button
            className={`btn ${monitoring ? 'on' : ''}`}
            title="Hear the input through the mixer (use headphones to avoid feedback)"
            onClick={() => { const m = !monitoring; setUI({ monitoring: m }); engine.input?.setMonitoring(m); if (m) toast('Monitoring on – use headphones!', 'info'); }}
          >
            <Icon name="headphones" size={12} /> Monitor
          </button>
        </div>
        <div className="vc-row vc-meter-row">
          <Meter source={inputLevel} width={14} height={92} stereo={false} />
          <Knob value={inputGain} min={0} max={4} defaultValue={1} size={44} label="Input Gain" format={formatDb} onChange={(v) => { setUI({ inputGain: v }); engine.input?.setGain(v); }} />
          <div className="vc-col">
            <label className="vc-field">
              <span className="label">Monitor / FX insert</span>
              <select className="input small-select" value={target.id} onChange={(e) => { engine.inputTrackId = e.target.value; engine.sync(getProject()); force((x) => x + 1); }}>
                {mixer.map((m, i) => <option key={m.id} value={m.id}>{i === 0 ? 'Master' : `${i} · ${m.name}`}</option>)}
              </select>
            </label>
            <label className="vc-field">
              <span className="label">Record to playlist track</span>
              <select className="input small-select" value={recTrack?.id} onChange={(e) => setUI({ selectedTrackId: e.target.value })}>
                {tracks.map((t) => <option key={t.id} value={t.id}>{t.name}</option>)}
              </select>
            </label>
            <label className="vc-field">
              <span className="label">Latency compensation</span>
              <div className="vc-row">
                <input
                  className="input small-select"
                  type="number"
                  value={latencyMs}
                  min={0}
                  max={500}
                  style={{ width: 64 }}
                  onKeyDown={(e) => e.stopPropagation()}
                  onChange={(e) => { const v = Number(e.target.value); setLatencyMs(v); engine.latencyCompensation = v / 1000; }}
                />
                <span className="label">ms</span>
              </div>
            </label>
          </div>
        </div>
        <button className={`vc-rec ${recording ? 'on' : ''}`} onClick={() => void toggleRecord()}>
          <span className="vc-rec-dot" />
          {recording ? 'Stop recording' : 'Record (R)'}
          <RecTimer />
        </button>
        <p className="vc-hint">Recording starts the song from the playhead and creates an audio clip with a live waveform. The raw input is recorded; the insert effects (like Goofy Tune) are applied during monitoring and playback.</p>
      </div>

      <div className="vc-tune">
        <div className="vc-title">
          <Icon name="vocal" /> Goofy Tune <span className="crumb">· on “{target.name}”</span>
          <span className="spacer" />
          {VOCAL_CHAINS.map((c) => <button key={c.name} className="btn small" onClick={() => applyVocalChain(c.name)} title="Load this vocal chain onto the vocal insert">{c.name}</button>)}
        </div>
        {pitchFx ? (
          <div className={`fx-body ${pitchFx.bypass ? 'bypassed' : ''}`} style={{ ['--fxc' as string]: '#f28c28' }}>
            <div className="vc-row" style={{ marginBottom: 6 }}>
              <button className={`btn small ${pitchFx.bypass ? '' : 'on'}`} onClick={() => toggleBypass(target.id, pitchFx.id)}><Icon name="power" size={11} /> {pitchFx.bypass ? 'Bypassed' : 'Active'}</button>
            </div>
            <PitchView
              trackId={target.id}
              fx={pitchFx}
              set={(k, v) => setEffectParam(target.id, pitchFx.id, k, v)}
              effect={() => engine.graph.strips.get(target.id)?.slot(pitchFx.id)?.effect}
            />
          </div>
        ) : (
          <div className="empty-hint">
            No pitch correction on this insert yet.<br />
            <button className="btn primary" style={{ marginTop: 10 }} onClick={() => addInsert(target.id, 'pitch', { key: String(getProject().key), scale: getProject().scale === 'Chromatic' ? 'Major' : getProject().scale }, 0)}>
              <Icon name="plus" size={12} /> Add Goofy Tune to {target.name}
            </button>
          </div>
        )}
      </div>
    </div>
  );
}
