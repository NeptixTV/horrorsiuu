// Admin panel (opened by typing 187): send news to every Goofy Studio user and
// publish updates. Writing needs the admin's GitHub token.
import { useEffect, useState } from 'react';
import { APP_VERSION, compareVersions, FEED, type Feed, type MessageKind } from '../../admin/feed';
import { clearUpdate, deleteMessage, hasToken, loadFeedAsAdmin, publishMessage, publishUpdate, setToken, testAccess } from '../../admin/github';
import { desktop } from '../../admin/desktop';
import { toast, type FloatingWindow } from '../../state/ui';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import './admin.css';

type Tab = 'news' | 'update' | 'history' | 'access';

const TOKEN_URL = 'https://github.com/settings/personal-access-tokens/new';

function bumpPatch(v: string) {
  const [a, b, c] = v.split('.').map((x) => parseInt(x, 10) || 0);
  return `${a}.${b}.${c + 1}`;
}

function FileDrop({ label, file, onFile }: { label: string; file: File | null; onFile: (f: File | null) => void }) {
  const [over, setOver] = useState(false);
  return (
    <div
      className={`adm-drop ${over ? 'drop-target' : ''} ${file ? 'has' : ''}`}
      onDragOver={(e) => { e.preventDefault(); setOver(true); }}
      onDragLeave={() => setOver(false)}
      onDrop={(e) => { e.preventDefault(); setOver(false); const f = e.dataTransfer.files?.[0]; if (f) onFile(f); }}
      onClick={() => {
        const i = document.createElement('input');
        i.type = 'file';
        i.accept = '.exe,application/octet-stream,application/x-msdownload';
        i.onchange = () => { const f = i.files?.[0]; if (f) onFile(f); };
        i.click();
      }}
    >
      <b>{label}</b>
      {file ? (
        <span>{file.name} · {(file.size / 1048576).toFixed(1)} MB <button className="btn ghost small" onClick={(e) => { e.stopPropagation(); onFile(null); }}>✕</button></span>
      ) : (
        <span>drag the .exe here or click</span>
      )}
    </div>
  );
}

export function AdminWindow({ w }: { w: FloatingWindow }) {
  const [tab, setTab] = useState<Tab>('news');
  const [tokenOk, setTokenOk] = useState<boolean | null>(null);
  const [tokenInput, setTokenInput] = useState('');
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState('');
  const [feed, setFeed] = useState<Feed | null>(null);
  // news
  const [title, setTitle] = useState('');
  const [body, setBody] = useState('');
  const [kind, setKind] = useState<MessageKind>('info');
  // update
  const [version, setVersion] = useState(bumpPatch(APP_VERSION));
  const [notes, setNotes] = useState('');
  const [setup, setSetup] = useState<File | null>(null);
  const [portable, setPortable] = useState<File | null>(null);
  const [link, setLink] = useState('');
  const [mandatory, setMandatory] = useState(false);

  const refresh = async () => {
    const has = await hasToken();
    setTokenOk(has);
    if (!has) { setTab('access'); return; }
    try {
      const f = await loadFeedAsAdmin();
      setFeed(f);
      if (f.update && compareVersions(f.update.version, APP_VERSION) >= 0) setVersion(bumpPatch(f.update.version));
    } catch (err) {
      setStatus(err instanceof Error ? err.message : String(err));
    }
  };
  useEffect(() => { void refresh(); }, []);

  const run = async (fn: () => Promise<void>) => {
    setBusy(true);
    setStatus('');
    try {
      await fn();
    } catch (err) {
      setStatus(err instanceof Error ? err.message : String(err));
      toast('Admin action failed – see the panel', 'error');
    } finally {
      setBusy(false);
    }
  };

  const saveToken = () => run(async () => {
    const t = tokenInput.trim();
    if (!t) return;
    await setToken(t);
    const r = await testAccess();
    setStatus(r.text);
    if (!r.ok) { await setToken(null); setTokenOk(false); return; }
    setTokenInput('');
    setTokenOk(true);
    setTab('news');
    await refresh();
  });

  const sendNews = () => run(async () => {
    if (!title.trim()) { setStatus('Please enter a title.'); return; }
    await publishMessage({ title: title.trim(), body: body.trim(), kind });
    setTitle('');
    setBody('');
    setStatus('✓ Sent! Everyone sees the message the next time they start Goofy Studio.');
    toast('News sent', 'ok');
    setFeed(await loadFeedAsAdmin());
  });

  const sendUpdate = () => run(async () => {
    if (!/^\d+\.\d+\.\d+$/.test(version.replace(/^v/, ''))) { setStatus('Version must look like 1.2.0'); return; }
    if (compareVersions(version, APP_VERSION) <= 0 && !confirm(`Version ${version} is not newer than this app (${APP_VERSION}). Users with ${APP_VERSION} will not see it. Publish anyway?`)) return;
    if (!setup && !portable && !link.trim()) { setStatus('Add the new .exe (Setup and/or portable) or a download link.'); return; }
    const upd = await publishUpdate({ version, notes: notes.trim(), mandatory, setupFile: setup, portableFile: portable, link: link.trim() || undefined, onStatus: setStatus });
    setStatus(`✓ Update ${upd.version} published! Users get the popup on their next start.`);
    toast(`Update ${upd.version} published`, 'ok');
    setSetup(null);
    setPortable(null);
    setFeed(await loadFeedAsAdmin());
  });

  const tabBtn = (t: Tab, label: string) => <button className={`adm-tab ${tab === t ? 'on' : ''}`} disabled={!tokenOk && t !== 'access'} onClick={() => setTab(t)}>{label}</button>;

  return (
    <Window w={w} width={640} color="#ff5c5c" title={<><b>Admin Panel</b> <span className="crumb">· {FEED.owner}/{FEED.repo}</span></>}>
      <div className="adm">
        <div className="adm-tabs">
          {tabBtn('news', '📢 Send news')}
          {tabBtn('update', '⬆️ Publish update')}
          {tabBtn('history', '📜 History')}
          {tabBtn('access', '🔑 Access')}
          <span className="spacer" />
          <span className={`adm-conn ${tokenOk ? 'ok' : ''}`}>{tokenOk === null ? '…' : tokenOk ? '● connected' : '○ not connected'}</span>
        </div>

        {tab === 'news' && (
          <div className="adm-page">
            <label className="adm-field"><span className="label">Title</span>
              <input className="input" value={title} maxLength={80} onChange={(e) => setTitle(e.target.value)} onKeyDown={(e) => e.stopPropagation()} placeholder="e.g. New drum kit is out!" />
            </label>
            <label className="adm-field"><span className="label">Message</span>
              <textarea className="input adm-text" value={body} onChange={(e) => setBody(e.target.value)} onKeyDown={(e) => e.stopPropagation()} placeholder="Write what your users should know…" />
            </label>
            <div className="adm-row">
              <span className="label">Style</span>
              {(['info', 'warning', 'party'] as MessageKind[]).map((k) => (
                <button key={k} className={`btn small ${kind === k ? 'on' : ''}`} onClick={() => setKind(k)}>{k === 'info' ? '📢 Info' : k === 'warning' ? '⚠️ Warning' : '🎉 Party'}</button>
              ))}
              <span className="spacer" />
              <button className="btn primary" disabled={busy || !title.trim()} onClick={() => void sendNews()}><Icon name="export" size={11} /> Send to everyone</button>
            </div>
            <div className="adm-preview">
              <span className="label">Preview of the popup</span>
              <div className={`notice ${kind}`}>
                <h2>{title || 'Title'}</h2>
                <div className="notice-body">{body || 'Your message…'}</div>
              </div>
            </div>
          </div>
        )}

        {tab === 'update' && (
          <div className="adm-page">
            <div className="adm-row">
              <label className="adm-field"><span className="label">New version</span>
                <input className="input" style={{ width: 110 }} value={version} onChange={(e) => setVersion(e.target.value.trim())} onKeyDown={(e) => e.stopPropagation()} />
              </label>
              <span className="label">this app: {APP_VERSION} · set the same version in package.json before building</span>
            </div>
            <label className="adm-field"><span className="label">What's new (shown in the popup)</span>
              <textarea className="input adm-text" value={notes} onChange={(e) => setNotes(e.target.value)} onKeyDown={(e) => e.stopPropagation()} placeholder={'- New effects\n- Bug fixes'} />
            </label>
            <div className="adm-drops">
              <FileDrop label="Installer (Setup.exe)" file={setup} onFile={setSetup} />
              <FileDrop label="Portable .exe" file={portable} onFile={setPortable} />
            </div>
            {!desktop && <p className="set-hint">File upload needs the desktop app – in the browser you can paste a download link.</p>}
            <label className="adm-field"><span className="label">…or a download page link (optional)</span>
              <input className="input" value={link} onChange={(e) => setLink(e.target.value)} onKeyDown={(e) => e.stopPropagation()} placeholder="https://…" />
            </label>
            <div className="adm-row">
              <label className="notice-skip"><input type="checkbox" checked={mandatory} onChange={(e) => setMandatory(e.target.checked)} /> Important update (can't be skipped)</label>
              <span className="spacer" />
              <button className="btn primary" disabled={busy} onClick={() => void sendUpdate()}><Icon name="export" size={11} /> Publish update</button>
            </div>
          </div>
        )}

        {tab === 'history' && (
          <div className="adm-page">
            <div className="adm-row">
              <b>Current update:</b>
              <span>{feed?.update ? `${feed.update.version} (${new Date(feed.update.date).toLocaleDateString()})` : 'none'}</span>
              <span className="spacer" />
              {feed?.update && <button className="btn small danger" disabled={busy} onClick={() => void run(async () => { await clearUpdate(); setFeed(await loadFeedAsAdmin()); setStatus('Update withdrawn.'); })}>Withdraw</button>}
              <button className="btn small" disabled={busy} onClick={() => void run(refresh)}>Refresh</button>
            </div>
            <div className="adm-list">
              {!feed?.messages.length && <div className="empty-hint">No news sent yet.</div>}
              {feed?.messages.map((m) => (
                <div key={m.id} className="adm-item">
                  <div className="grow">
                    <b>{m.title}</b> <span className="label">{new Date(m.date).toLocaleString()}</span>
                    <div className="adm-item-body">{m.body}</div>
                  </div>
                  <button className="btn small ghost danger" disabled={busy} title="Delete for everyone" onClick={() => void run(async () => { await deleteMessage(m.id); setFeed(await loadFeedAsAdmin()); })}><Icon name="trash" size={11} /></button>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === 'access' && (
          <div className="adm-page">
            <p className="adm-info">
              To send news and updates to everyone, Goofy Studio writes to your GitHub repository <b>{FEED.owner}/{FEED.repo}</b>.
              Create a <b>fine-grained personal access token</b> for that repository with the permission <b>Contents: Read and write</b> and paste it here.
              It is stored encrypted on this computer only{desktop ? '' : ' (browser storage)'} and is never part of the app you share.
            </p>
            <div className="adm-row">
              <button className="btn" onClick={() => (desktop ? void desktop.openExternal(TOKEN_URL) : window.open(TOKEN_URL, '_blank', 'noopener'))}><Icon name="export" size={11} /> Create token on GitHub</button>
              {tokenOk && <button className="btn" disabled={busy} onClick={() => void run(async () => { const r = await testAccess(); setStatus(r.text); })}>Test connection</button>}
              {tokenOk && <button className="btn danger" disabled={busy} onClick={() => void run(async () => { await setToken(null); setTokenOk(false); setStatus('Token removed from this computer.'); })}>Remove token</button>}
            </div>
            <div className="adm-row">
              <input className="input grow" type="password" value={tokenInput} placeholder={tokenOk ? 'Paste a new token to replace the current one' : 'github_pat_…'} onChange={(e) => setTokenInput(e.target.value)} onKeyDown={(e) => { e.stopPropagation(); if (e.key === 'Enter') void saveToken(); }} />
              <button className="btn primary" disabled={busy || !tokenInput.trim()} onClick={() => void saveToken()}>Save & test</button>
            </div>
          </div>
        )}

        {status && <div className={`adm-status ${status.startsWith('✓') || status.startsWith('Connected') ? 'ok' : ''}`}>{status}</div>}
      </div>
    </Window>
  );
}
