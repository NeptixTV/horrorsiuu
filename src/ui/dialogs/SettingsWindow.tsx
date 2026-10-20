// The settings center: one window with a sidebar of categories. Every option
// is stored on this computer and takes effect immediately (audio engine
// options that need a restart say so and offer the restart).
import { useState, type ReactNode } from 'react';
import logo from '../../assets/logo.png';
import { openWindow, setUI, toast, useUI, SETTING_DEFAULTS, type FloatingWindow, type ThemeName } from '../../state/ui';
import { THEMES } from '../../styles/themes';
import { PALETTE } from '../../state/utils';
import { APP_VERSION } from '../../admin/feed';
import { exportPresets, importPresets, usePresets } from '../../state/userPresets';
import { downloadBlob } from '../../audio/render';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { pickFile } from './filePick';
import { AudioSection, PerformanceSection, RecordingSection } from '../settings/AudioSettings';
import { DiagnosticsSection } from '../settings/DiagnosticsPanel';
import { ProjectSection } from '../settings/ProjectSettings';
import { Row, Section, Select, Slider, Toggle } from '../settings/controls';
import { checkNotices } from '../admin/notices';
import { THIRD_PARTY } from './licenses';
import './dialogs.css';
import '../settings/settings-center.css';

type Page = 'audio' | 'performance' | 'diagnostics' | 'recording' | 'design' | 'projects' | 'general' | 'privacy' | 'about';

const PAGES: { id: Page; label: string; icon: string }[] = [
  { id: 'audio', label: 'Audio & devices', icon: 'headphones' },
  { id: 'performance', label: 'Performance', icon: 'cpu' },
  { id: 'diagnostics', label: 'Diagnostics', icon: 'shield' },
  { id: 'recording', label: 'Recording & MIDI', icon: 'mic' },
  { id: 'design', label: 'Design', icon: 'palette' },
  { id: 'projects', label: 'Projects & backups', icon: 'save' },
  { id: 'general', label: 'General', icon: 'settings' },
  { id: 'privacy', label: 'Privacy & updates', icon: 'info' },
  { id: 'about', label: 'About', icon: 'star' },
];

let lastPage: Page = 'audio';

/** Opens the settings on a given page. */
export function openSettingsPage(page: Page) {
  lastPage = page;
  openWindow({ kind: 'settings' });
}

function DesignSection() {
  const s = useUI();
  const themes: ThemeName[] = ['dark', 'darkblue', 'purple', 'neon', 'custom'];
  const setCustom = (k: keyof typeof s.custom, v: string) => setUI({ custom: { ...s.custom, [k]: v }, theme: 'custom' });
  return (
    <>
      <Section title="Theme">
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
      </Section>
      <Section title="Colours">
        <Row label="Accent colour">
          <div className="dlg-swatches">
            <button className={`dlg-swatch reset ${!s.accent ? 'active' : ''}`} onClick={() => setUI({ accent: null })} title="Theme default">A</button>
            {PALETTE.slice(0, 10).map((c) => (
              <button key={c} className={`dlg-swatch ${s.accent === c ? 'active' : ''}`} style={{ background: c }} onClick={() => (s.theme === 'custom' ? setCustom('accent', c) : setUI({ accent: c }))} />
            ))}
            <input type="color" className="sc-color" value={s.accent ?? (s.theme === 'custom' ? s.custom.accent : '#f28c28')} onChange={(e) => (s.theme === 'custom' ? setCustom('accent', e.target.value) : setUI({ accent: e.target.value }))} title="Pick any colour" />
          </div>
        </Row>
        <div className="dlg-colors">
          {(['bg', 'panel', 'grid', 'text', 'accent'] as const).map((k) => (
            <label key={k}>
              <input type="color" className="sc-color" value={s.custom[k]} onChange={(e) => setCustom(k, e.target.value)} />
              <span>{{ bg: 'Background', panel: 'Panels', grid: 'Grid / editors', text: 'Text', accent: 'Accent' }[k]}</span>
            </label>
          ))}
        </div>
      </Section>
      <Section title="Layout & motion">
        <Row label="Interface size" desc="Makes everything bigger or smaller (also Ctrl + / Ctrl − in the browser).">
          <Slider value={s.uiScale} min={0.7} max={1.6} step={0.05} format={(v) => `${Math.round(v * 100)} %`} onChange={(v) => setUI({ uiScale: v })} />
        </Row>
        <Row label="Reduce motion" desc="No animations or transitions.">
          <Toggle value={s.reduceMotion} onChange={(v) => setUI({ reduceMotion: v })} />
        </Row>
        <Row label="Start-up animation" desc="Show the animated logo while the studio loads.">
          <Toggle value={s.showSplash} onChange={(v) => setUI({ showSplash: v })} />
        </Row>
        <Row label="Hints in the status bar">
          <Toggle value={s.showHints} onChange={(v) => setUI({ showHints: v })} />
        </Row>
      </Section>
    </>
  );
}

function GeneralSection() {
  const s = useUI();
  const presetCount = usePresets((x) => x.list.length);
  return (
    <>
      <Section title="Behaviour">
        <Row label="Notifications" desc="Which short messages appear at the bottom of the screen. Errors are always shown.">
          <Select width={200} value={s.notifyLevel} options={[{ value: 'all', label: 'All messages' }, { value: 'important', label: 'Important only' }, { value: 'errors', label: 'Errors only' }]} onChange={(v) => setUI({ notifyLevel: v })} />
        </Row>
        <Row label="Ask before deleting" desc="Confirm before channels, patterns, tracks or mixer inserts with content are deleted.">
          <Toggle value={s.confirmDeletes} onChange={(v) => setUI({ confirmDeletes: v })} />
        </Row>
        <Row label="Follow the playhead" desc="The playlist scrolls along while the song plays.">
          <Toggle value={s.followPlayhead} onChange={(v) => setUI({ followPlayhead: v })} />
        </Row>
        <Row label="Keyboard shortcuts">
          <button className="btn" onClick={() => openWindow({ kind: 'shortcuts' })}><Icon name="keyboard" size={12} /> Show list</button>
        </Row>
      </Section>
      <Section title="My presets" hint="Instrument and effect presets you saved yourself.">
        <Row label={`${presetCount} preset${presetCount === 1 ? '' : 's'}`} desc="Export them as a file to use them on another computer.">
          <div className="sc-inline">
            <button className="btn small" disabled={!presetCount} onClick={() => downloadBlob(exportPresets(), 'goofy-presets.json')}><Icon name="export" size={11} /> Export</button>
            <button className="btn small" onClick={() => pickFile('.json,application/json', async (f) => { try { toast(`Imported ${await importPresets(f)} presets`, 'ok'); } catch (e) { toast(e instanceof Error ? e.message : 'Import failed', 'error'); } })}><Icon name="import" size={11} /> Import</button>
          </div>
        </Row>
      </Section>
      <Section title="Reset">
        <Row label="Audio setup wizard" desc="Walk through device and theme selection again.">
          <button className="btn" onClick={() => setUI({ setupDone: false })}>Run again</button>
        </Row>
        <Row label="Reset all settings" desc="Every option on these pages goes back to its default. Projects, presets and the library are not touched.">
          <button className="btn danger" onClick={() => { if (confirm('Reset all settings to their defaults?')) { setUI({ ...SETTING_DEFAULTS }); toast('Settings reset', 'ok'); } }}>Reset</button>
        </Row>
      </Section>
    </>
  );
}

function PrivacySection() {
  const s = useUI();
  return (
    <>
      <Section title="Updates & news">
        <Row label="Check for updates at start" desc="Looks for a newer version of Goofy Studio. Nothing is installed without asking you.">
          <Toggle value={s.checkUpdates} onChange={(v) => setUI({ checkUpdates: v })} />
        </Row>
        <Row label="Show news at start" desc="Messages from the Goofy Studio team (shown once).">
          <Toggle value={s.showNews} onChange={(v) => setUI({ showNews: v })} />
        </Row>
        <Row label="Check now">
          <button className="btn" onClick={() => void checkNotices(true)}>Check for updates &amp; news</button>
        </Row>
      </Section>
      <Section title="What leaves your computer">
        <ul className="sc-facts">
          <li><b>Your music stays here.</b> Projects, recordings, samples, presets and backups are stored only on this computer. There is no account and no cloud upload.</li>
          <li><b>No tracking.</b> Goofy Studio has no analytics, no telemetry and no automatic crash reports. The diagnostics log never leaves the computer unless you copy it yourself.</li>
          <li><b>Update / news check</b> (only if switched on above): one request to GitHub (api.github.com, raw.githubusercontent.com) that downloads a small public file. GitHub sees your IP address, like for any website; nothing about you or your music is sent.</li>
          <li><b>Downloading an update</b> (only when you click “Download”): the file comes from github.com.</li>
          <li><b>Admin panel</b> (only for the publisher with a GitHub token): talks to the GitHub API to publish news and updates.</li>
          <li>Microphone and MIDI are only used inside the app and only after you allowed them.</li>
        </ul>
      </Section>
    </>
  );
}

function AboutSection() {
  return (
    <>
      <div className="sc-about">
        <img src={logo} alt="" />
        <div>
          <h2>Goofy Studio <span className="crumb">v{APP_VERSION}</span></h2>
          <p>A digital audio workstation built on the Web Audio API and AudioWorklets. All instruments and samples are synthesised in real time – no third-party sample content.</p>
        </div>
      </div>
      <Section title="Open-source software" hint="Goofy Studio is built with these open-source projects. Thank you!">
        <div className="sc-list">
          {THIRD_PARTY.map((l) => (
            <div key={l.name} className="sc-list-row">
              <div className="sc-list-main">
                <div><b>{l.name}</b> <span className="crumb">{l.license}</span></div>
                <div className="sc-row-desc">{l.copyright}</div>
              </div>
            </div>
          ))}
        </div>
        <p className="sc-hint">The full license texts are included with the app (file “THIRD_PARTY_NOTICES.txt”; for Electron/Chromium “LICENSES.chromium.html” in the installation folder).</p>
      </Section>
    </>
  );
}

export function SettingsWindow({ w }: { w: FloatingWindow }) {
  const [page, setPage] = useState<Page>(lastPage);
  const go = (p: Page) => {
    lastPage = p;
    setPage(p);
  };
  const content: Record<Page, ReactNode> = {
    audio: <AudioSection />,
    performance: <PerformanceSection />,
    diagnostics: <DiagnosticsSection />,
    recording: <RecordingSection />,
    design: <DesignSection />,
    projects: <ProjectSection />,
    general: <GeneralSection />,
    privacy: <PrivacySection />,
    about: <AboutSection />,
  };
  return (
    <Window w={w} title={<><Icon name="settings" size={12} /> <b>Settings</b></>} width={860}>
      <div className="sc">
        <nav className="sc-nav">
          {PAGES.map((p) => (
            <button key={p.id} className={`sc-nav-item ${page === p.id ? 'on' : ''}`} onClick={() => go(p.id)}>
              <Icon name={p.icon} size={14} /> {p.label}
            </button>
          ))}
        </nav>
        <div className="sc-page" key={page}>
          <h2 className="sc-title">{PAGES.find((p) => p.id === page)?.label}</h2>
          {content[page]}
        </div>
      </div>
    </Window>
  );
}
