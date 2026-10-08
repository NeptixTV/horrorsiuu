import { setUI, useUI, type FloatingWindow, type ThemeName } from '../../state/ui';
import { THEMES } from '../../styles/themes';
import { engine } from '../../audio/engine';
import { Window } from '../components/Window';
import { PALETTE } from '../../state/utils';
import './dialogs.css';

export function SettingsWindow({ w }: { w: FloatingWindow }) {
  const s = useUI();
  const themes: ThemeName[] = ['dark', 'darkblue', 'purple', 'neon', 'custom'];
  const setCustom = (k: keyof typeof s.custom, v: string) => setUI({ custom: { ...s.custom, [k]: v }, theme: 'custom' });
  return (
    <Window w={w} title={<b>Settings</b>} width={480}>
      <div className="dlg">
        <section>
          <h3>Theme</h3>
          <div className="dlg-themes">
            {themes.map((t) => {
              const v = t === 'custom' ? { bg: s.custom.bg, panel2: s.custom.panel, accent: s.custom.accent, grid: s.custom.grid, label: 'Custom' } : THEMES[t];
              return (
                <button key={t} className={`theme-card ${s.theme === t ? 'active' : ''}`} onClick={() => setUI({ theme: t })}>
                  <div className="theme-preview" style={{ background: v.bg }}>
                    <div style={{ background: v.panel2, height: 8 }} />
                    <div style={{ flex: 1, background: v.grid, display: 'flex', alignItems: 'center', padding: 4 }}>
                      <div style={{ width: '60%', height: 8, background: v.accent, borderRadius: 2 }} />
                    </div>
                  </div>
                  <span>{v.label}</span>
                </button>
              );
            })}
          </div>
        </section>
        <section>
          <h3>Accent colour</h3>
          <div className="dlg-swatches">
            <button className={`dlg-swatch reset ${!s.accent ? 'active' : ''}`} onClick={() => setUI({ accent: null })} title="Theme default">A</button>
            {PALETTE.slice(0, 10).map((c) => (
              <button key={c} className={`dlg-swatch ${s.accent === c ? 'active' : ''}`} style={{ background: c }} onClick={() => (s.theme === 'custom' ? setCustom('accent', c) : setUI({ accent: c }))} />
            ))}
            <input type="color" value={s.accent ?? (s.theme === 'custom' ? s.custom.accent : '#f28c28')} onChange={(e) => (s.theme === 'custom' ? setCustom('accent', e.target.value) : setUI({ accent: e.target.value }))} title="Pick any colour" />
          </div>
        </section>
        <section>
          <h3>Custom interface colours</h3>
          <div className="dlg-colors">
            {(['bg', 'panel', 'grid', 'text', 'accent'] as const).map((k) => (
              <label key={k}>
                <input type="color" value={s.custom[k]} onChange={(e) => setCustom(k, e.target.value)} />
                <span>{{ bg: 'Background', panel: 'Panels', grid: 'Grid / Editors', text: 'Text', accent: 'Accent' }[k]}</span>
              </label>
            ))}
          </div>
        </section>
        <section>
          <h3>Audio</h3>
          <div className="dlg-stats">
            <span>Sample rate</span><b>{engine.initialized ? engine.ctx.sampleRate : '—'} Hz</b>
            <span>Buffer (approx.)</span><b>{engine.initialized ? engine.bufferSize : '—'} samples</b>
            <span>Output latency</span><b>{engine.initialized ? (((engine.ctx as AudioContext & { outputLatency?: number }).outputLatency ?? 0) * 1000).toFixed(1) : '—'} ms</b>
            <span>AudioWorklet</span><b>{engine.worklets ? 'yes' : 'no'}</b>
          </div>
          <label className="dlg-check">
            <span>Latency mode (restart required)</span>
            <select className="input" value={s.latencyHint} onChange={(e) => setUI({ latencyHint: e.target.value as typeof s.latencyHint })}>
              <option value="interactive">Interactive</option>
              <option value="balanced">Balanced</option>
              <option value="playback">Playback</option>
            </select>
          </label>
          <button className="btn" onClick={() => setUI({ setupDone: false })}>Run audio setup wizard again</button>
        </section>
        <section>
          <h3>Project</h3>
          <label className="dlg-check"><input type="checkbox" checked={s.autosave} onChange={(e) => setUI({ autosave: e.target.checked })} /> Autosave every 45 seconds</label>
          <label className="dlg-check"><input type="checkbox" checked={s.followPlayhead} onChange={(e) => setUI({ followPlayhead: e.target.checked })} /> Follow playhead while playing</label>
        </section>
      </div>
    </Window>
  );
}
