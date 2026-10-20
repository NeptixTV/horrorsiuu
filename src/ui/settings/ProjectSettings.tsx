// Settings → Projects & backups: autosave, backups (list / restore), project
// information and storage housekeeping.
import { useEffect, useState } from 'react';
import { useProject } from '../../state/store';
import { setUI, toast, useUI } from '../../state/ui';
import { renameProject } from '../../state/actions';
import {
  backupNow, cleanupStorage, listBackups, requestPersistentStorage, restoreBackup, startAutosave, storageInfo, type StorageInfo,
} from '../../project/manager';
import { storage } from '../../project/storage';
import { Icon } from '../components/Icon';
import { askText } from '../dialogs/askText';
import { Row, Section, Select, Stat, Toggle, fmtBytes } from './controls';

type BackupRow = Awaited<ReturnType<typeof listBackups>>[number];
const REASON: Record<BackupRow['reason'], string> = { auto: 'automatic', save: 'on save', 'before-restore': 'before a restore', repair: 'before a repair' };

export function ProjectSection() {
  const s = useUI();
  const project = useProject((x) => x.project);
  const lastSaved = useProject((x) => x.lastSaved);
  const lastAutosave = useProject((x) => x.lastAutosave);
  const dirty = useProject((x) => x.dirty);
  const [backups, setBackups] = useState<BackupRow[] | null>(null);
  const [info, setInfo] = useState<StorageInfo | null>(null);
  const [busy, setBusy] = useState(false);
  const loadBackups = () => void listBackups().then(setBackups).catch(() => setBackups([]));
  const loadInfo = () => void storageInfo().then(setInfo).catch(() => setInfo(null));
  useEffect(() => { loadBackups(); loadInfo(); }, [project.id]);

  const notes = Object.values(project.patterns).reduce((n, p) => n + Object.values(p.notes).reduce((m, l) => m + l.length, 0), 0);
  const audioClips = project.clips.filter((c) => c.kind === 'audio').length;
  const lastSave = Math.max(lastSaved ?? 0, lastAutosave ?? 0);

  return (
    <>
      <Section title="This project">
        <Row label="Name">
          <div className="sc-inline">
            <b className="sc-value">{project.name}</b>
            <button className="btn small" onClick={() => void askText('Project name', project.name).then((n) => { if (n) renameProject(n); })}>Rename</button>
          </div>
        </Row>
        <div className="sc-stats">
          <Stat label="Created" value={new Date(project.createdAt).toLocaleString()} />
          <Stat label="Last saved" value={lastSave ? new Date(lastSave).toLocaleString() : 'not saved yet'} warn={dirty && !lastSave} />
          <Stat label="Tempo / signature" value={`${project.bpm} BPM · ${project.timeSig[0]}/${project.timeSig[1]}`} />
          <Stat label="Channels / patterns" value={`${project.channels.length} / ${project.patterns.length}`} />
          <Stat label="Playlist clips (audio)" value={`${project.clips.length} (${audioClips})`} />
          <Stat label="Notes" value={notes} />
          <Stat label="Mixer inserts" value={project.mixer.length - 1} />
          <Stat label="Unsaved changes" value={dirty ? 'yes' : 'no'} warn={dirty} />
        </div>
      </Section>

      <Section title="Saving">
        <Row label="Autosave" desc="Saves in the background. While the song plays, saving waits until you stop (so playback never stutters).">
          <Toggle value={s.autosave} onChange={(v) => setUI({ autosave: v })} />
        </Row>
        <Row label="Autosave every">
          <Select width={160} value={s.autosaveInterval} options={[30, 45, 60, 120, 300, 600].map((v) => ({ value: v, label: v < 60 ? `${v} seconds` : `${v / 60} minute${v === 60 ? '' : 's'}` }))} onChange={(v) => { setUI({ autosaveInterval: v }); setTimeout(startAutosave, 0); }} />
        </Row>
        <Row label="Open the last project at start">
          <Toggle value={s.openLastProject} onChange={(v) => setUI({ openLastProject: v })} />
        </Row>
        <Row label="Undo steps" desc="How many changes you can undo (more uses a little more memory).">
          <Select width={160} value={s.historyLimit} options={[50, 100, 150, 300, 500].map((v) => ({ value: v, label: `${v}` }))} onChange={(v) => setUI({ historyLimit: v })} />
        </Row>
      </Section>

      <Section title="Backups" hint="Snapshots of your project are kept separately from the project itself. If something goes wrong you can go back to an earlier state.">
        <Row label="Automatic backups">
          <Toggle value={s.backups} onChange={(v) => setUI({ backups: v })} />
        </Row>
        <Row label="Make a backup at most every">
          <Select width={160} value={s.backupInterval} options={[2, 5, 10, 20, 30, 60].map((v) => ({ value: v, label: `${v} minutes` }))} onChange={(v) => setUI({ backupInterval: v })} />
        </Row>
        <Row label="Keep">
          <Select width={160} value={s.backupCount} options={[3, 5, 10, 20, 50].map((v) => ({ value: v, label: `${v} backups per project` }))} onChange={(v) => setUI({ backupCount: v })} />
        </Row>
        <div className="sc-inline">
          <button className="btn small" onClick={() => void backupNow().then(loadBackups)}><Icon name="save" size={11} /> Back up now</button>
        </div>
        <div className="sc-list">
          {backups === null && <div className="sc-log-empty">Loading…</div>}
          {backups?.length === 0 && <div className="sc-log-empty">No backups of this project yet.</div>}
          {backups?.map((b) => (
            <div key={b.id} className="sc-list-row">
              <Icon name="history" size={13} />
              <div className="sc-list-main">
                <div>{new Date(b.time).toLocaleString()}</div>
                <div className="sc-row-desc">{b.name} · {REASON[b.reason]}</div>
              </div>
              <button className="btn small" onClick={async () => { if (confirm('Replace the open project with this backup? (The current state is backed up first.)')) { await restoreBackup(b.id); loadBackups(); } }}>Restore</button>
              <button className="btn small ghost danger" title="Delete backup" onClick={async () => { await storage.deleteBackup(b.id); loadBackups(); }}><Icon name="trash" size={11} /></button>
            </div>
          ))}
        </div>
      </Section>

      <Section title="Storage" hint="Projects, recordings and backups are stored only on this computer.">
        {info ? (
          <div className="sc-stats">
            <Stat label="Used by Goofy Studio" value={info.usage ? fmtBytes(info.usage) : '—'} />
            <Stat label="Available" value={info.quota ? fmtBytes(Math.max(0, info.quota - info.usage)) : '—'} warn={info.quota > 0 && info.quota - info.usage < 500e6} />
            <Stat label="Saved projects" value={info.projects} />
            <Stat label="Audio files" value={`${info.audioFiles} (${fmtBytes(info.audioBytes)})`} />
            <Stat label="Backups" value={info.backups} />
            <Stat label="Unused audio" value={`${info.orphanFiles} (${fmtBytes(info.orphanBytes)})`} warn={info.orphanBytes > 100e6} />
            <Stat label="Protected from clean-up" value={info.persisted ? 'yes' : 'no'} />
          </div>
        ) : <div className="sc-log-empty">Loading…</div>}
        <Row label="Clean up unused audio" desc="Deletes recordings and imports that no saved project, backup or your library uses any more.">
          <button
            className="btn"
            disabled={busy || !info?.orphanFiles}
            onClick={async () => {
              if (!confirm(`Delete ${info?.orphanFiles} unused audio file(s)?`)) return;
              setBusy(true);
              try {
                const freed = await cleanupStorage();
                toast(`Freed ${fmtBytes(freed)}`, 'ok');
              } finally {
                setBusy(false);
                loadInfo();
              }
            }}
          >Clean up</button>
        </Row>
        <Row label="Protect my data" desc="Asks the browser never to delete Goofy Studio's data when the disk gets full.">
          <button className="btn" disabled={info?.persisted} onClick={() => void requestPersistentStorage().then((ok) => { toast(ok ? 'Storage is now protected' : 'The browser declined – keep enough free disk space', ok ? 'ok' : 'info'); loadInfo(); })}>{info?.persisted ? 'Protected' : 'Protect'}</button>
        </Row>
      </Section>
    </>
  );
}
