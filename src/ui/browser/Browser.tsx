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
import { importLibrarySample } from '../../project/manager';
import { storage, recordToBuffer } from '../../project/storage';
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
  const [userSamples, setUserSamples] = useState<{ id: string; name: string }[]>([]);
  const patterns = useProject((s) => s.project.patterns);
  const projectAssets = useProject((s) => s.project.assets);
  const selectedPatternId = useUI((s) => s.selectedPatternId);

  useEffect(() => {
    storage.listLibraryAssets().then((list) => {
      for (const rec of list) if (!assets.has(rec.id)) assets.add(rec.id, recordToBuffer(rec), rec.name);
      setUserSamples(list.map((r) => ({ id: r.id, name: r.name })));
    }).catch(() => undefined);
  }, []);

  const importSamples = () => pickFile('audio/*', async (f) => {
    const res = await importLibrarySample(f);
    if (res) {
      setUserSamples((s) => [...s, { id: res.id, name: res.name }]);
      setOpen((o) => ({ ...o, samples: true }));
      toast(`Imported “${res.name}”`, 'ok');
    }
  }, true);

  const categories = useMemo<Category[]>(() => {
    const sampleItem = (id: string, name: string, color: string, sub?: string): Item => ({
      key: id, label: name, icon: 'wave', color, sub, payload: { type: 'sample', assetId: id, name },
      onClick: () => previewSample(id),
      onActivate: () => {
        const s = LIBRARY.find((x) => x.id === id);
        setUI({ selectedChannelId: addChannel('sampler', { assetId: id, name, rootNote: s?.root ?? 60 }) });
        toast(`Sampler channel “${name}” added`, 'ok');
      },
    });
    const libGroup = (cat: string) => LIBRARY.filter((s) => s.category === cat).map((s) => sampleItem(s.id, s.name, '#56ccf2', cat));
    return [
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
        groups: [
          { label: 'Drum One-Shots', items: libGroup('Drums') },
          { label: 'Bass', items: libGroup('Bass') },
          { label: 'Melodic', items: libGroup('Melodic') },
          { label: 'FX', items: libGroup('FX') },
          { label: 'Loops', items: libGroup('Loops') },
          { label: 'My Samples', items: userSamples.map((s) => sampleItem(s.id, s.name, '#9be15d', 'Imported')) },
        ],
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
  }, [userSamples, patterns, projectAssets, selectedPatternId]);

  const q = query.trim().toLowerCase();
  const match = (it: Item) => !q || it.label.toLowerCase().includes(q) || (it.sub ?? '').toLowerCase().includes(q);

  const renderItem = (it: Item) => (
    <div
      key={it.key}
      className="br-item"
      draggable
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
          const groups = (c.groups ?? []).map((g) => ({ ...g, items: g.items.filter(match) })).filter((g) => g.items.length || (!q && g.label === 'My Samples'));
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
                      {g.label === 'My Samples' && !q && (
                        <div className="br-item br-import" onClick={importSamples}><Icon name="plus" size={11} /> Import audio files…</div>
                      )}
                    </div>
                  ))}
                </div>
              )}
            </div>
          );
        })}
      </div>
      <div className="br-hint">Click a sample to preview · double-click to add · drag into rack, playlist or mixer</div>
    </div>
  );
}
