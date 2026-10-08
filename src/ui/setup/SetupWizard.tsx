import { useEffect, useState } from 'react';
import logo from '../../assets/logo.png';
import { engine } from '../../audio/engine';
import { AudioInput, type DeviceInfo } from '../../audio/input';
import { setUI, useUI, type ThemeName } from '../../state/ui';
import { THEMES } from '../../styles/themes';
import { Icon } from '../components/Icon';
import './setup.css';

const STEPS = ['Welcome', 'Audio Devices', 'Audio Engine', 'Theme'];

export function SetupWizard({ onDone }: { onDone: () => void }) {
  const [step, setStep] = useState(0);
  const [inputs, setInputs] = useState<DeviceInfo[]>([]);
  const [outputs, setOutputs] = useState<DeviceInfo[]>([]);
  const [permission, setPermission] = useState<'unknown' | 'granted' | 'denied'>('unknown');
  const inputDeviceId = useUI((s) => s.inputDeviceId);
  const outputDeviceId = useUI((s) => s.outputDeviceId);
  const themeName = useUI((s) => s.theme);
  const latencyHint = useUI((s) => s.latencyHint);

  const refresh = async () => {
    const d = await AudioInput.listDevices();
    setInputs(d.inputs);
    setOutputs(d.outputs);
  };
  useEffect(() => { void refresh(); }, []);

  const askPermission = async () => {
    await engine.resume();
    const ok = await engine.input.open(inputDeviceId);
    setPermission(ok ? 'granted' : 'denied');
    if (ok) setUI({ inputDeviceId: engine.input.deviceId });
    await refresh();
  };

  const finish = async () => {
    await engine.resume();
    setUI({ setupDone: true });
    onDone();
  };

  return (
    <div className="setup-backdrop">
      <div className="setup">
        <div className="setup-side">
          <img src={logo} alt="" className="setup-logo" />
          <div className="setup-steps">
            {STEPS.map((s, i) => (
              <div key={s} className={`setup-step ${i === step ? 'active' : ''} ${i < step ? 'done' : ''}`}>
                <span className="setup-step-n">{i < step ? '✓' : i + 1}</span>
                {s}
              </div>
            ))}
          </div>
        </div>
        <div className="setup-main">
          {step === 0 && (
            <div className="setup-page">
              <h1>Welcome to <span>Goofy Studio</span></h1>
              <p>
                A complete music production studio in your browser: step sequencer, playlist, piano roll, mixer with insert
                effects and sends, a built-in synthesizer, a drum machine, microphone recording and real-time vocal pitch correction.
              </p>
              <ul className="setup-features">
                <li><Icon name="rack" /> Channel rack & 11 synthesized drum sounds</li>
                <li><Icon name="piano" /> Piano roll with velocity, snap and scale highlighting</li>
                <li><Icon name="mixer" /> Mixer with EQ, compressor, reverb, delay, filter, drive, chorus</li>
                <li><Icon name="vocal" /> Mic recording with live monitoring & Goofy Tune pitch correction</li>
                <li><Icon name="export" /> WAV export, autosave, undo/redo</li>
              </ul>
              <p className="dim">Let's set up your audio in three quick steps.</p>
            </div>
          )}
          {step === 1 && (
            <div className="setup-page">
              <h2>Audio Devices</h2>
              <label className="setup-field">
                <span>Audio Input (Microphone)</span>
                <select className="input" value={inputDeviceId} onChange={(e) => setUI({ inputDeviceId: e.target.value })}>
                  <option value="">System default</option>
                  {inputs.map((d) => <option key={d.deviceId} value={d.deviceId}>{d.label}</option>)}
                </select>
              </label>
              <div className="setup-perm">
                <button className="btn" onClick={askPermission}><Icon name="mic" /> {permission === 'granted' ? 'Microphone connected' : 'Allow microphone access'}</button>
                {permission === 'granted' && <span className="ok">✓ Access granted – device names are visible now.</span>}
                {permission === 'denied' && <span className="err">Access was denied or no device was found. You can still use everything except recording.</span>}
                {permission === 'unknown' && <span className="dim">Optional – needed for recording and device names.</span>}
              </div>
              <label className="setup-field">
                <span>Audio Output</span>
                <select
                  className="input"
                  value={outputDeviceId}
                  disabled={!engine.supportsOutputSelection}
                  onChange={async (e) => {
                    const id = e.target.value;
                    setUI({ outputDeviceId: id });
                    await engine.setOutputDevice(id);
                  }}
                >
                  <option value="">System default</option>
                  {outputs.filter((d) => d.deviceId !== 'default').map((d) => <option key={d.deviceId} value={d.deviceId}>{d.label}</option>)}
                </select>
                {!engine.supportsOutputSelection && <small className="dim">Output selection is not supported by this browser – the system default is used.</small>}
              </label>
            </div>
          )}
          {step === 2 && (
            <div className="setup-page">
              <h2>Audio Engine</h2>
              <div className="setup-stats">
                <div><span className="label">Sample Rate</span><b>{engine.ctx.sampleRate.toLocaleString()} Hz</b></div>
                <div><span className="label">Buffer Size</span><b>{engine.bufferSize} samples</b></div>
                <div><span className="label">Base Latency</span><b>{((engine.ctx.baseLatency || 0) * 1000).toFixed(1)} ms</b></div>
                <div><span className="label">AudioWorklet</span><b>{engine.worklets ? 'available' : 'unavailable'}</b></div>
              </div>
              <label className="setup-field">
                <span>Latency mode (applies after restart)</span>
                <select className="input" value={latencyHint} onChange={(e) => setUI({ latencyHint: e.target.value as typeof latencyHint })}>
                  <option value="interactive">Interactive – lowest latency (recommended)</option>
                  <option value="balanced">Balanced</option>
                  <option value="playback">Playback – largest buffer, most stable</option>
                </select>
              </label>
              <p className="dim">The browser chooses the hardware buffer size. If you hear crackles, pick "Balanced" or "Playback".</p>
            </div>
          )}
          {step === 3 && (
            <div className="setup-page">
              <h2>Choose a Theme</h2>
              <div className="setup-themes">
                {(Object.keys(THEMES) as Exclude<ThemeName, 'custom'>[]).map((k) => {
                  const t = THEMES[k];
                  return (
                    <button key={k} className={`theme-card ${themeName === k ? 'active' : ''}`} onClick={() => setUI({ theme: k, accent: null })}>
                      <div className="theme-preview" style={{ background: t.bg }}>
                        <div style={{ background: t.panel2, height: 10 }} />
                        <div style={{ display: 'flex', gap: 3, padding: 4, background: t.grid, flex: 1 }}>
                          <div style={{ background: t.accent, width: 24, borderRadius: 2 }} />
                          <div style={{ background: t.accent2, width: 14, borderRadius: 2, opacity: 0.7 }} />
                          <div style={{ background: t.led, width: 6, borderRadius: 2 }} />
                        </div>
                      </div>
                      <span>{t.label}</span>
                    </button>
                  );
                })}
              </div>
              <p className="dim">You can change the theme, accent colour and a fully custom palette later in Options → Settings.</p>
            </div>
          )}
          <div className="setup-nav">
            {step > 0 && <button className="btn" onClick={() => setStep(step - 1)}>Back</button>}
            <span className="spacer" />
            <button className="btn ghost" onClick={finish}>Skip</button>
            {step < STEPS.length - 1 ? (
              <button className="btn primary" onClick={() => setStep(step + 1)}>Next</button>
            ) : (
              <button className="btn primary" onClick={finish}>Start making music</button>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
