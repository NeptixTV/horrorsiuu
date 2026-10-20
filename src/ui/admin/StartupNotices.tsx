import { useEffect, useState } from 'react';
import { APP_VERSION, markSeen, skipVersion, type FeedMessage } from '../../admin/feed';
import { desktop } from '../../admin/desktop';
import { toast } from '../../state/ui';
import { Modal } from '../components/Window';
import { checkNotices, useNotices } from './notices';
import './admin.css';

const KIND_ICON: Record<FeedMessage['kind'], string> = { info: '📢', warning: '⚠️', party: '🎉' };

function NewsModal() {
  const messages = useNotices((s) => s.messages);
  const update = useNotices((s) => s.update);
  const [i, setI] = useState(0);
  const m = messages[i];
  if (!m) return null;
  const done = () => {
    markSeen(messages.map((x) => x.id));
    useNotices.setState({ show: update ? 'update' : null });
  };
  return (
    <Modal width={480} title={<>{KIND_ICON[m.kind] ?? '📢'} News from Goofy Studio</>} onClose={done}>
      <div className={`notice ${m.kind}`}>
        <div className="notice-date">{new Date(m.date).toLocaleString()}</div>
        <h2>{m.title}</h2>
        <div className="notice-body">{m.body}</div>
        <div className="notice-nav">
          {messages.length > 1 && <span className="label">{i + 1} / {messages.length}</span>}
          <span className="spacer" />
          {i > 0 && <button className="btn" onClick={() => setI(i - 1)}>Back</button>}
          {i < messages.length - 1 ? <button className="btn primary" onClick={() => setI(i + 1)}>Next</button> : <button className="btn primary" onClick={done}>Got it</button>}
        </div>
      </div>
    </Modal>
  );
}

function UpdateModal() {
  const upd = useNotices((s) => s.update)!;
  const [skip, setSkip] = useState(false);
  const [state, setState] = useState<'idle' | 'downloading' | 'ready' | 'error'>('idle');
  const [progress, setProgress] = useState(0);
  const [file, setFile] = useState<{ path: string; kind: 'setup' | 'portable' } | null>(null);
  const [error, setError] = useState('');

  useEffect(() => desktop?.onProgress((p) => { if (p.id === 'update') setProgress(p.total ? p.done / p.total : 0); }), []);

  const later = () => {
    if (skip && !upd.mandatory) skipVersion(upd.version);
    useNotices.setState({ show: null });
  };

  const download = async () => {
    if (!desktop) {
      // the feed parser only lets https links through; checked again here
      const url = upd.assets.setup ?? upd.assets.portable ?? upd.assets.page;
      if (url && url.startsWith('https://')) window.open(url, '_blank', 'noopener,noreferrer');
      useNotices.setState({ show: null });
      return;
    }
    const info = await desktop.info();
    let kind: 'setup' | 'portable' = info.portable ? 'portable' : 'setup';
    let url = kind === 'portable' ? upd.assets.portable ?? upd.assets.setup : upd.assets.setup ?? upd.assets.portable;
    if (url === upd.assets.setup) kind = 'setup';
    else if (url === upd.assets.portable) kind = 'portable';
    if (!url) {
      if (upd.assets.page) void desktop.openExternal(upd.assets.page);
      useNotices.setState({ show: null });
      return;
    }
    setState('downloading');
    setProgress(0);
    const name = kind === 'setup' ? `GoofyStudio-${upd.version}-Setup.exe` : `GoofyStudio-${upd.version}-portable.exe`;
    const r = await desktop.download(url, name, kind, kind === 'setup' ? upd.assets.setupSha256 : upd.assets.portableSha256);
    if (r.ok && r.path) {
      setFile({ path: r.path, kind });
      setState('ready');
    } else {
      setError(r.error ?? 'Download failed');
      setState('error');
    }
  };

  const install = async () => {
    if (!desktop || !file) return;
    const ok = await desktop.install(file.path, file.kind);
    if (!ok) toast('Could not start the update – open it manually from your Downloads folder', 'error');
  };

  return (
    <Modal width={520} title={<>⬆️ Update available</>} onClose={upd.mandatory ? undefined : later}>
      <div className="notice update">
        <div className="notice-versions"><span>{APP_VERSION}</span> → <b>{upd.version}</b>{upd.mandatory && <span className="notice-badge">important</span>}</div>
        <h2>What's new</h2>
        <div className="notice-body">{upd.notes || '—'}</div>
        {state === 'downloading' && (
          <div className="notice-progress"><div style={{ transform: `scaleX(${Math.max(0.02, progress)})` }} /><span>Downloading… {Math.round(progress * 100)}%</span></div>
        )}
        {state === 'error' && <div className="notice-error">{error}</div>}
        <div className="notice-nav">
          {!upd.mandatory && state === 'idle' && (
            <label className="notice-skip"><input type="checkbox" checked={skip} onChange={(e) => setSkip(e.target.checked)} /> Don't remind me about {upd.version}</label>
          )}
          <span className="spacer" />
          {state === 'idle' && !upd.mandatory && <button className="btn" onClick={later}>Not now</button>}
          {(state === 'idle' || state === 'error') && <button className="btn primary" onClick={() => void download()}>⬇ Download{desktop ? ' & update' : ''}</button>}
          {state === 'ready' && <button className="btn primary" onClick={() => void install()}>Install & restart</button>}
        </div>
        {state === 'ready' && file?.kind === 'portable' && <p className="set-hint">The new version was saved next to your current Goofy Studio file. You can delete the old .exe afterwards.</p>}
      </div>
    </Modal>
  );
}

export function StartupNotices() {
  const show = useNotices((s) => s.show);
  useEffect(() => {
    const t = setTimeout(() => void checkNotices(false), 1200);
    return () => clearTimeout(t);
  }, []);
  if (show === 'news') return <NewsModal />;
  if (show === 'update') return <UpdateModal />;
  return null;
}
