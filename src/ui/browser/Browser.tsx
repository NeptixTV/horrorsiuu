import { useEffect, useMemo, useState, type DragEvent } from 'react';
import { LIBRARY } from '../../audio/library';
import { EFFECTS, EFFECT_PRESETS, EFFECT_TYPES } from '../../audio/effects/descriptors';
import { DRUM_LABELS, SYNTH_PRESETS } from '../../state/defaults';
import { setUI, toast, ui, useUI, type DragPayload } from '../../state/ui';
import { useProject } from '../../state/store';
import { addChannel, addInsert } from '../../state/actions';
import type { DrumType, EffectType } from '../../state/types';
import { assets } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { getProject } from '../../state/store';
import { placeAudioClip } from '../../project/manager';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { askText } from '../dialogs/askText';
import { addToLibrary, deleteLibrarySample, loadLibrary, renameLibrarySample, useLibrary } from '../library/libraryStore';
import { FX_CATEGORIES, FX_CHAINS } from '../../audio/effects/fxPresets';
import { fxApplyChain } from '../../state/actions';
import { openWindow } from '../../state/ui';
import { Icon } from '../components/Icon';
import { pickFile } from '../dialogs/filePick';
import { applyVocalChain, VOCAL_CHAINS } from './vocalChains';
import './browser.css';

interface Item {
  key: string;
  label: string;
  icon: string;
  color?: string;
  payload: DragPayload;
  sub?: string;
  onActivate: () => void;
  onClick?: () => void;
  menu?: () => MenuItem[];
  noDrag?: boolean;
}

interface Category {
  key: string;
  label: string;
  icon: string;
  color: string;
  items: Item[];
  groups?: { label: string; items: Item[] }[];
}

export const DRAG_MIME = 'application/x-goofy';

export function setDragPayload(e: DragEvent, payload: DragPayload) {
  e.dataTransfer.setData(DRAG_MIME, JSON.stringify(payload));
  e.dataTransfer.setData('text/plain', 'goofy');
  e.dataTransfer.effectAllowed = 'copy';
  setUI({ dragPayload: payload });
}

export function readDragPayload(e: DragEvent): DragPayload | null {
  try {
    const raw = e.dataTransfer.getData(DRAG_MIME);
    if (raw) return JSON.parse(raw);
  } catch { /* ignore */ }
  return ui().dragPayload;
}

let previewSrc: AudioBufferSourceNode | null = null;
export function previewSample(assetId: string) {
  const b = assets.get(assetId);
  if (!b || !engine.initialized) return;
  void engine.resume();
  try { previewSrc?.stop(); } catch { /* ignore */ }
  const src = engine.ctx.createBufferSource();
  src.buffer = b;
  const g = engine.ctx.createGain();
  g.gain.value = 0.8;
  src.connect(g).connect(engine.masterOut);
  src.start();
  previewSrc = src;
}

function addEffectToSelected(type: EffectType, presetName?: string) {
  const trackId = ui().selectedMixerId;
  const preset = EFFECT_PRESETS.find((p) => p.name === presetName);
  addInsert(trackId, type, preset?.params);
  setUI({ dockTab: 'mixer', dockOpen: true });
  toast(`${presetName ?? EFFECTS[type].name} added to selected mixer insert`, 'ok');
}

/** Double-click on a Sound FX: selected audio clip first, otherwise the selected mixer insert. */
export function applyFxChainToSelection(name: string) {
  const chain = FX_CHAINS.find((c) => c.name === name);
  if (!chain) return;
  const p = getProject();
  const clip = p.clips.find((c) => ui().selectedClipIds.includes(c.id) && c.kind === 'audio');
  if (clip) {
    fxApplyChain({ kind: 'clip', id: clip.id }, chain.effects, 'replace', chain.name);
    toast(`${chain.icon} ${chain.name} → selected clip`, 'ok');
  } else {
    fxApplyChain({ kind: 'mixer', id: ui().selectedMixerId }, chain.effects, 'add', chain.name);
    toast(`${chain.icon} ${chain.name} → mixer insert “${p.mixer.find((m) => m.id === ui().selectedMixerId)?.name}”`, 'ok');
  }
}

const DRUM_KIT: DrumType[] = ['kick', 'snare', 'clap', 'hihat', 'openhat', 'tom', 'perc'];

export function addDrumKit() {
  let first: string | null = null;
  for (const d of DRUM_KIT) {
    const id = addChannel('drum', { drum: d });
    first ??= id;
  }
  setUI({ selectedChannelId: first, dockTab: 'drums', dockOpen: true });
  toast('Drum kit added (7 channels)', 'ok');
}

export function Browser() {
  const [query, setQuery] = useState('');
  const [open, setOpen] = useState<Record<string, boolean>>({ instruments: true, drums: true });
  const userSamples = useLibrary((s) => s.samples);
  const patterns = useProject((s) => s.project.patterns);
  const projectAssets = useProject((s) => s.project.assets);
  const selectedPatternId = useUI((s) => s.selectedPatternId);

  useEffect(() => { void loadLibrary(); }, []);

  const importSamples = () => pickFile('audio/*', async (f) => {
    try {
      const buffer = await engine.ctx.decodeAudioData(await f.arrayBuffer());
      const name = f.name.replace(/\.[^.]+$/, '');
      await addToLibrary(buffer, name);
      setOpen((o) => ({ ...o, library: true }));
      toast(`Imported “${name}” into My Library`, 'ok');
    } catch {
      toast(`Could not read “${f.name}”`, 'error');
    }
  }, true);

  const categories = useMemo<Category[]>(() => {
    const toPlaylist = (id: string) => {
      const p = getProject();
      const trackId = ui().selectedTrackId ?? p.tracks[0].id;
      placeAudioClip(id, trackId, Math.round(engine.transport?.getTick() ?? 0));
      toast('Added to the playlist at the playhead', 'ok');
    };
    const sampleItem = (id: string, name: string, color: string, sub?: string, own = false): Item => ({
      key: id, label: name, icon: 'wave', color, sub, payload: { type: 'sample', assetId: id, name },
      onClick: () => previewSample(id),
      onActivate: () => {
        const s = LIBRARY.find((x) => x.id === id);
        setUI({ selectedChannelId: addChannel('sampler', { assetId: id, name, rootNote: s?.root ?? 60 }) });
        toast(`Sampler channel “${name}” added`, 'ok');
      },
      menu: () => [
        { label: '▶ Preview', onClick: () => previewSample(id) },
        { label: own ? 'Edit (cut, effects, pitch)…' : 'Edit a copy…', onClick: () => openWindow({ kind: 'sampleEditor', assetId: id }) },
        { label: 'Add to playlist (at playhead)', onClick: () => toPlaylist(id) },
        { label: 'New sampler channel', onClick: () => setUI({ selectedChannelId: addChannel('sampler', { assetId: id, name, rootNote: LIBRARY.find((x) => x.id === id)?.root ?? 60 }) }) },
        ...(own ? [
          { separator: true },
          { label: 'Rename…', onClick: () => { void askText('Sample name', name).then((n) => { if (n) void renameLibrarySample(id, n); }); } },
          { label: 'Delete from My Library', danger: true, onClick: () => { if (confirm(`Delete “${name}”?`)) void deleteLibrarySample(id); } },
        ] as MenuItem[] : []),
      ],
    });
    const libGroup = (cat: string) => LIBRARY.filter((s) => s.category === cat).map((s) => sampleItem(s.id, s.name, '#56ccf2', cat));
    const libCats = ['Drums', 'Percussion', 'Bass', 'Melodic', 'FX', 'Funny', 'Game', 'Loops'];
    const action = (key: string, label: string, icon: string, fn: () => void): Item => ({ key, label, icon, color: 'var(--accent)', payload: { type: 'instrument', kind: 'synth' }, onActivate: fn, onClick: fn, noDrag: true });
    return [
      {
        key: 'library', label: 'My Library', icon: 'star', color: '#9be15d',
        items: [
          action('lib-import', 'Import audio files…', 'import', importSamples),
          action('lib-split', 'Song Splitter (vocals / beat)…', 'scissors', () => openWindow({ kind: 'stems' })),
          ...userSamples.map((s) => sampleItem(s.id, s.name, '#9be15d', s.duration ? `${s.duration.toFixed(1)}s` : undefined, true)),
        ],
      },
      {
        key: 'soundfx', label: 'Sound FX', icon: 'fx', color: '#ff6bb3', items: [],
        groups: FX_CATEGORIES.map((cat) => ({
          label: cat,
          items: FX_CHAINS.filter((c) => c.category === cat).map((c) => ({
            key: `sfx-${c.name}`, label: `${c.icon} ${c.name}`, icon: 'fx', color: '#ff6bb3', sub: cat, payload: { type: 'fxchain', name: c.name } as DragPayload,
            onActivate: () => applyFxChainToSelection(c.name),
          })),
        })),
      },
      {
        key: 'instruments', label: 'Instruments', icon: 'synth', color: '#f28c28',
        items: [
          { key: 'i-synth', label: 'Goofy Synth', icon: 'synth', color: '#f28c28', payload: { type: 'instrument', kind: 'synth' }, onActivate: () => setUI({ selectedChannelId: addChannel('synth') }) },
          { key: 'i-sampler', label: 'Goofy Sampler', icon: 'sample', color: '#56ccf2', payload: { type: 'instrument', kind: 'sampler' }, onActivate: () => setUI({ selectedChannelId: addChannel('sampler') }) },
          { key: 'i-kit', label: 'Goofy Drum Kit (7 pads)', icon: 'drums', color: '#ff5c5c', payload: { type: 'drum', drum: 'kit' }, onActivate: addDrumKit },
        ],
      },
      {
        key: 'synths', label: 'Synthesizers', icon: 'piano', color: '#5b8cff', items: [],
        groups: ['Bass', 'Lead', 'Pad', 'Pluck', 'Keys', 'FX'].map((g) => ({
          label: g,
          items: SYNTH_PRESETS.filter((p) => p.category === g).map((p) => ({
            key: `sp-${p.name}`, label: p.name, icon: 'preset', color: '#5b8cff', sub: g, payload: { type: 'synthPreset', preset: p.name },
            onActivate: () => setUI({ selectedChannelId: addChannel('synth', { synthPreset: p.name }) }),
          })),
        })),
      },
      {
        key: 'drums', label: 'Drums', icon: 'drums', color: '#ff5c5c',
        items: (Object.keys(DRUM_LABELS) as DrumType[]).map((d) => ({
          key: `d-${d}`, label: DRUM_LABELS[d], icon: 'drums', color: '#ff5c5c', payload: { type: 'drum', drum: d },
          onActivate: () => setUI({ selectedChannelId: addChannel('drum', { drum: d }) }),
        })),
      },
      {
        key: 'samples', label: 'Samples', icon: 'sample', color: '#56ccf2', items: [],
        groups: libCats.filter((c) => c !== 'Vocals').map((c) => ({ label: c, items: libGroup(c) })),
      },
      {
        key: 'vocals', label: 'Vocals', icon: 'vocal', color: '#f28c28', items: [],
        groups: [
          { label: 'Vocal Samples', items: libGroup('Vocals') },
          {
            label: 'Vocal Chains',
            items: VOCAL_CHAINS.map((c) => ({
              key: `vc-${c.name}`, label: c.name, icon: 'mic', color: '#f28c28', sub: 'Chain', payload: { type: 'effect', effect: 'pitch', preset: `chain:${c.name}` } as DragPayload,
              onActivate: () => applyVocalChain(c.name),
            })),
          },
          {
            label: 'Recordings',
            items: Object.values(projectAssets).filter((a) => !a.builtin).map((a) => sampleItem(a.id, a.name, '#ff6bb3', 'Project')),
          },
        ],
      },
      {
        key: 'effects', label: 'Effects', icon: 'fx', color: '#8f6bff',
        items: EFFECT_TYPES.map((t) => ({
          key: `fx-${t}`, label: EFFECTS[t].name, icon: 'fx', color: EFFECTS[t].color, sub: EFFECTS[t].category, payload: { type: 'effect', effect: t },
          onActivate: () => addEffectToSelected(t),
        })),
      },
      {
        key: 'presets', label: 'Presets', icon: 'preset', color: '#ffbe3d', items: [],
        groups: EFFECT_TYPES.map((t) => ({
          label: EFFECTS[t].name,
          items: EFFECT_PRESETS.filter((p) => p.type === t).map((p) => ({
            key: `pr-${p.name}`, label: p.name, icon: 'preset', color: EFFECTS[t].color, sub: EFFECTS[t].short, payload: { type: 'effect', effect: t, preset: p.name } as DragPayload,
            onActivate: () => addEffectToSelected(t, p.name),
          })),
        })).filter((g) => g.items.length),
      },
      {
        key: 'project', label: 'Current Project', icon: 'folder', color: '#9be15d',
        items: patterns.map((p) => ({
          key: `pt-${p.id}`, label: p.name, icon: 'rack', color: p.color, sub: p.id === selectedPatternId ? 'selected' : undefined, payload: { type: 'pattern', patternId: p.id },
          onActivate: () => setUI({ selectedPatternId: p.id }),
        })),
      },
    ];
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [userSamples, patterns, projectAssets, selectedPatternId]);

  const q = query.trim().toLowerCase();
  const match = (it: Item) => !q || it.label.toLowerCase().includes(q) || (it.sub ?? '').toLowerCase().includes(q);

  const renderItem = (it: Item) => (
    <div
      key={it.key}
      className={`br-item ${it.noDrag ? 'br-action' : ''}`}
      draggable={!it.noDrag}
      onContextMenu={(e) => { if (it.menu) { e.preventDefault(); showMenu(e.clientX, e.clientY, it.menu()); } }}
      onDragStart={(e) => setDragPayload(e, it.payload)}
      onDragEnd={() => setUI({ dragPayload: null })}
      onClick={it.onClick}
      onDoubleClick={it.onActivate}
      title={`${it.label} – double-click to add, drag into the project`}
    >
      <Icon name={it.icon} size={12} style={{ color: it.color }} />
      <span className="br-item-label">{it.label}</span>
      {it.sub && <span className="br-item-sub">{it.sub}</span>}
    </div>
  );

  return (
    <div className="browser">
      <div className="panel-title">
        <Icon name="browser" size={13} /> Browser
        <span className="spacer" />
        <button className="btn ghost icon small" title="Import audio files into My Samples" onClick={importSamples}><Icon name="import" size={12} /></button>
      </div>
      <div className="br-search">
        <Icon name="search" size={12} />
        <input placeholder="Search sounds, presets, effects…" value={query} onChange={(e) => setQuery(e.target.value)} onKeyDown={(e) => e.stopPropagation()} />
        {query && <button className="btn ghost icon small" onClick={() => setQuery('')}><Icon name="close" size={10} /></button>}
      </div>
      <div className="br-tree scroll-y">
        {categories.map((c) => {
          const items = c.items.filter(match);
          const groups = (c.groups ?? []).map((g) => ({ ...g, items: g.items.filter(match) })).filter((g) => g.items.length);
          const count = items.length + groups.reduce((n, g) => n + g.items.length, 0);
          if (q && count === 0) return null;
          const isOpen = q ? true : !!open[c.key];
          return (
            <div key={c.key} className="br-cat">
              <div className={`br-cat-head ${isOpen ? 'open' : ''}`} onClick={() => setOpen((o) => ({ ...o, [c.key]: !o[c.key] }))}>
                <Icon name={isOpen ? 'chevronDown' : 'chevronRight'} size={11} />
                <Icon name={c.icon} size={13} style={{ color: c.color }} />
                <span>{c.label}</span>
                <span className="br-count">{count}</span>
              </div>
              {isOpen && (
                <div className="br-cat-body">
                  {items.map(renderItem)}
                  {groups.map((g) => (
                    <div key={g.label} className="br-group">
                      <div className="br-group-head">{g.label}</div>
                      {g.items.map(renderItem)}
                    </div>
                  ))}
                </div>
              )}
            </div>
          );
        })}
      </div>
      <div className="br-hint">Click a sample to preview · double-click to add · right-click for edit options · drag Sound FX onto a clip</div>
    </div>
  );
}
