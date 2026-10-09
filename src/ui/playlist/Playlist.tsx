import { useCallback, useEffect, useRef, useState, type DragEvent, type PointerEvent as RPE } from 'react';
import { getProject, beginGesture, endGesture, update, useProject } from '../../state/store';
import { setUI, ui, useUI, type PlaylistTool } from '../../state/ui';
import type { Clip, Project } from '../../state/types';
import { PPQ, TICKS_PER_STEP } from '../../state/types';
import { clamp, secondsToTicks, snapTicks, ticksPerBar, uid } from '../../state/utils';
import {
  addPattern, addTrack, clonePattern, deleteClips, duplicateClips, removePattern, removeTrack, setLoop, sliceClip, updateClips, updatePattern, updateTrack,
} from '../../state/actions';
import { patternLength, songLength } from '../../audio/sequencer';
import { engine } from '../../audio/engine';
import { assets } from '../../audio/assets';
import { theme, themeVersion } from '../../styles/themes';
import { onFrame } from '../components/frameLoop';
import { Icon } from '../components/Icon';
import { showMenu, type MenuItem } from '../components/ContextMenu';
import { colorMenu } from '../components/ColorPicker';
import { readDragPayload } from '../browser/Browser';
import { decodeAudioFile, placeAudioClip } from '../../project/manager';
import { addChannel } from '../../state/actions';
import { drawClips, drawGrid, drawLoop, drawRuler, RULER_H, type View } from './draw';
import { rgba } from '../../state/utils';
import './playlist.css';
import { askText } from '../dialogs/askText';

type Drag =
  | { kind: 'move'; startTick: number; startTrack: number; anchorId: string; orig: Map<string, { start: number; track: number }>; moved: boolean }
  | { kind: 'resize'; id: string; origStart: number; origLen: number; maxLen: number }
  | { kind: 'trim'; id: string; origStart: number; origLen: number; origOffset: number; audio: boolean }
  | { kind: 'marquee'; x0: number; y0: number; x1: number; y1: number; add: boolean }
  | { kind: 'seek' }
  | { kind: 'loop'; anchor: number };

const SNAPS: { label: string; value: number }[] = [
  { label: 'None', value: 1 },
  { label: '1/16 (Step)', value: TICKS_PER_STEP },
  { label: '1/8', value: PPQ / 2 },
  { label: '1/4 (Beat)', value: PPQ },
  { label: '1/2 Bar', value: PPQ * 2 },
  { label: 'Bar', value: PPQ * 4 },
];

function trackIndexOf(project: Project, trackId: string) {
  return project.tracks.findIndex((t) => t.id === trackId);
}

export function Playlist() {
  const vpRef = useRef<HTMLDivElement>(null);
  const gridRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);
  const rulerRef = useRef<HTMLCanvasElement>(null);
  const headersRef = useRef<HTMLDivElement>(null);
  const view = useRef<View>({ w: 0, h: 0, scrollX: 0, scrollY: 0, pxPerTick: 22 / PPQ, trackH: 44 });
  const dirty = useRef(true);
  const drag = useRef<Drag | null>(null);
  const hoverCursor = useRef('default');
  const [dropHint, setDropHint] = useState(false);

  const tracks = useProject((s) => s.project.tracks);
  const patterns = useProject((s) => s.project.patterns);
  const songLen = useProject((s) => songLength(s.project));
  const zoom = useUI((s) => s.playlistZoom);
  const trackH = useUI((s) => s.trackHeight);
  const tool = useUI((s) => s.playlistTool);
  const snap = useUI((s) => s.playlistSnap);
  const selectedPatternId = useUI((s) => s.selectedPatternId);
  const selectedTrackId = useUI((s) => s.selectedTrackId);
  const follow = useUI((s) => s.followPlayhead);
  const mode = useUI((s) => s.mode);
  const recording = useUI((s) => s.recording);

  view.current.pxPerTick = zoom / PPQ;
  view.current.trackH = trackH;

  const totalW = (songLen + ticksPerBar([4, 4]) * 32) * (zoom / PPQ);
  const totalH = tracks.length * trackH + 80;

  // mark dirty on any relevant change
  useEffect(() => {
    const unsubP = useProject.subscribe((s, prev) => {
      if (s.project !== prev.project) dirty.current = true;
    });
    const unsubU = useUI.subscribe((s, prev) => {
      if (s.selectedClipIds !== prev.selectedClipIds || s.playlistZoom !== prev.playlistZoom || s.trackHeight !== prev.trackHeight || s.theme !== prev.theme || s.accent !== prev.accent || s.custom !== prev.custom) dirty.current = true;
    });
    const unsubA = assets.subscribe(() => { dirty.current = true; });
    return () => { unsubP(); unsubU(); unsubA(); };
  }, []);
  useEffect(() => { dirty.current = true; }, [zoom, trackH, tracks]);

  // canvas sizing
  useEffect(() => {
    const vp = vpRef.current!;
    const resize = () => {
      const w = vp.clientWidth;
      const h = vp.clientHeight;
      const dpr = window.devicePixelRatio || 1;
      for (const cv of [gridRef.current!, overlayRef.current!]) {
        cv.width = Math.round(w * dpr);
        cv.height = Math.round(h * dpr);
        cv.style.width = `${w}px`;
        cv.style.height = `${h}px`;
      }
      const r = rulerRef.current!;
      r.width = Math.round(w * dpr);
      r.height = Math.round(RULER_H * dpr);
      r.style.width = `${w}px`;
      view.current.w = w;
      view.current.h = h;
      dirty.current = true;
    };
    resize();
    const ro = new ResizeObserver(resize);
    ro.observe(vp);
    return () => ro.disconnect();
  }, []);

  // render loop
  useEffect(() => {
    let lastTheme = -1;
    return onFrame(() => {
      const v = view.current;
      const project = getProject();
      const t = theme();
      const dpr = window.devicePixelRatio || 1;
      if (themeVersion() !== lastTheme) {
        lastTheme = themeVersion();
        dirty.current = true;
      }
      if (dirty.current && gridRef.current && rulerRef.current) {
        dirty.current = false;
        const g = gridRef.current.getContext('2d')!;
        g.setTransform(dpr, 0, 0, dpr, 0, 0);
        drawGrid(g, v, project, t);
        drawLoop(g, v, project, t);
        drawClips(g, v, project, new Set(ui().selectedClipIds), t);
        if (headersRef.current) headersRef.current.style.transform = `translateY(${-v.scrollY}px)`;
      }
      const o = overlayRef.current?.getContext('2d');
      const r = rulerRef.current?.getContext('2d');
      if (!o || !r) return;
      r.setTransform(dpr, 0, 0, dpr, 0, 0);
      drawRuler(r, v.w, RULER_H, v.scrollX, v.pxPerTick, project, t);
      o.setTransform(dpr, 0, 0, dpr, 0, 0);
      o.clearRect(0, 0, v.w, v.h);
      // recording clip
      const rec = engine.recording;
      const cap = engine.input?.capture;
      if (rec && cap) {
        const ti = trackIndexOf(project, rec.trackId);
        const x = rec.startTick * v.pxPerTick - v.scrollX;
        const secs = cap.samples / engine.ctx.sampleRate;
        const w = secondsToTicks(secs, project.bpm) * v.pxPerTick;
        const y = ti * v.trackH - v.scrollY + 1;
        const h = v.trackH - 3;
        o.fillStyle = rgba(t.rec, 0.25);
        o.fillRect(x, y, w, h);
        o.fillStyle = t.rec;
        o.fillRect(x, y, w, 11);
        o.fillStyle = '#fff';
        o.font = '600 9px Inter, sans-serif';
        o.fillText('● REC', x + 3, y + 8.5);
        o.fillStyle = rgba('#ffffff', 0.85);
        const per = engine.input.peakBlock / engine.ctx.sampleRate;
        const pxPer = secondsToTicks(per, project.bpm) * v.pxPerTick;
        const mid = y + 11 + (h - 11) / 2;
        const amp = (h - 13) / 2;
        for (let i = 0; i < cap.peaks.length; i++) {
          const px = x + i * pxPer;
          if (px < -2 || px > v.w) continue;
          const a = Math.min(1, cap.peaks[i]) * amp;
          o.fillRect(px, mid - a, Math.max(1, pxPer - 0.3), Math.max(1, a * 2));
        }
      }
      // marquee
      const d = drag.current;
      if (d?.kind === 'marquee') {
        o.strokeStyle = t.accent;
        o.fillStyle = rgba(t.accent, 0.12);
        const x = Math.min(d.x0, d.x1) - v.scrollX;
        const y = Math.min(d.y0, d.y1) - v.scrollY;
        o.fillRect(x, y, Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0));
        o.strokeRect(x + 0.5, y + 0.5, Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0));
      }
      // playhead
      if (engine.transport && (engine.transport.mode === 'song' || !engine.transport.playing)) {
        const tick = engine.transport.getTick();
        const x = tick * v.pxPerTick - v.scrollX;
        if (x >= -6 && x <= v.w + 6) {
          const col = engine.recording ? t.rec : t.accent2;
          o.fillStyle = col;
          o.shadowColor = col;
          o.shadowBlur = 6;
          o.fillRect(Math.round(x), 0, 1.5, v.h);
          o.shadowBlur = 0;
          // ruler marker
          r.fillStyle = col;
          r.beginPath();
          r.moveTo(x - 6, RULER_H - 11);
          r.lineTo(x + 6, RULER_H - 11);
          r.lineTo(x, RULER_H - 1);
          r.closePath();
          r.fill();
        }
        if (engine.transport.playing && ui().followPlayhead && engine.transport.mode === 'song' && vpRef.current) {
          if (x > v.w * 0.92 || x < 0) vpRef.current.scrollLeft = Math.max(0, tick * v.pxPerTick - v.w * 0.08);
        }
      }
    });
  }, []);

  const onScroll = () => {
    const vp = vpRef.current!;
    view.current.scrollX = vp.scrollLeft;
    view.current.scrollY = vp.scrollTop;
    dirty.current = true;
  };

  // ---------- coordinate helpers ----------
  const toContent = (e: { clientX: number; clientY: number }) => {
    const r = gridRef.current!.getBoundingClientRect();
    const v = view.current;
    const x = e.clientX - r.left + v.scrollX;
    const y = e.clientY - r.top + v.scrollY;
    return { x, y, tick: x / v.pxPerTick, track: Math.floor(y / v.trackH) };
  };

  const hitClip = (x: number, y: number): { clip: Clip; edge: 'left' | 'right' | null } | null => {
    const project = getProject();
    const v = view.current;
    for (let i = project.clips.length - 1; i >= 0; i--) {
      const c = project.clips[i];
      const ti = trackIndexOf(project, c.trackId);
      const cx = c.start * v.pxPerTick;
      const cw = c.length * v.pxPerTick;
      const cy = ti * v.trackH;
      if (x >= cx && x <= cx + cw && y >= cy && y < cy + v.trackH) {
        const edgeW = Math.min(8, cw / 4);
        return { clip: c, edge: x > cx + cw - edgeW ? 'right' : x < cx + edgeW ? 'left' : null };
      }
    }
    return null;
  };

  const snapOf = (e: { altKey: boolean }) => (e.altKey ? 1 : ui().playlistSnap);

  // ---------- pointer handling ----------
  const onPointerDown = (e: RPE) => {
    setUI({ focus: 'playlist' });
    void engine.resume();
    const { x, y, tick, track } = toContent(e);
    const project = getProject();
    const hit = hitClip(x, y);
    const s = ui();
    if (e.button === 2) return; // context menu handler
    if (e.button === 1) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    if (track >= 0 && track < project.tracks.length) setUI({ selectedTrackId: project.tracks[track].id });

    if (s.playlistTool === 'slice') {
      if (hit) sliceClip(hit.clip.id, snapTicks(tick, snapOf(e)));
      return;
    }
    if (hit) {
      const { clip, edge } = hit;
      let selected = s.selectedClipIds.includes(clip.id) ? s.selectedClipIds : [clip.id];
      if (e.ctrlKey || e.metaKey) selected = s.selectedClipIds.includes(clip.id) ? s.selectedClipIds.filter((i) => i !== clip.id) : [...s.selectedClipIds, clip.id];
      if (clip.kind === 'pattern' && clip.patternId) setUI({ selectedPatternId: clip.patternId });
      beginGesture();
      if (edge === 'right') {
        const buf = clip.kind === 'audio' && clip.assetId ? assets.get(clip.assetId) : undefined;
        const maxLen = buf ? secondsToTicks(buf.duration - clip.offset, project.bpm) : Infinity;
        drag.current = { kind: 'resize', id: clip.id, origStart: clip.start, origLen: clip.length, maxLen };
        setUI({ selectedClipIds: [clip.id] });
        return;
      }
      if (edge === 'left') {
        drag.current = { kind: 'trim', id: clip.id, origStart: clip.start, origLen: clip.length, origOffset: clip.offset, audio: clip.kind === 'audio' };
        setUI({ selectedClipIds: [clip.id] });
        return;
      }
      let ids = selected;
      if (e.shiftKey) {
        // shift-drag duplicates
        const copies = project.clips.filter((c) => selected.includes(c.id)).map((c) => ({ ...c, id: uid('c') }));
        update('Duplicate clips', (d) => { d.clips.push(...copies); });
        ids = copies.map((c) => c.id);
      }
      setUI({ selectedClipIds: ids });
      const p2 = getProject();
      const orig = new Map(p2.clips.filter((c) => ids.includes(c.id)).map((c) => [c.id, { start: c.start, track: trackIndexOf(p2, c.trackId) }]));
      const anchorId = e.shiftKey ? ids[selected.indexOf(clip.id)] ?? ids[0] : clip.id;
      drag.current = { kind: 'move', startTick: tick, startTrack: track, anchorId, orig, moved: false };
      return;
    }
    // empty area
    if (s.playlistTool === 'select' || e.ctrlKey || e.metaKey) {
      drag.current = { kind: 'marquee', x0: x, y0: y, x1: x, y1: y, add: e.ctrlKey || e.metaKey };
      return;
    }
    // draw tool: place the selected pattern
    if (track < 0 || track >= project.tracks.length) {
      setUI({ selectedClipIds: [] });
      return;
    }
    const pattern = project.patterns.find((p) => p.id === s.selectedPatternId) ?? project.patterns[0];
    if (!pattern) return;
    beginGesture();
    const start = Math.max(0, snapTicks(tick, snapOf(e), 'floor'));
    const id = uid('c');
    const clip: Clip = { id, trackId: project.tracks[track].id, kind: 'pattern', start, length: patternLength(pattern, project.timeSig), offset: 0, patternId: pattern.id };
    update('Add clip', (d) => { d.clips.push(clip); });
    setUI({ selectedClipIds: [id] });
    drag.current = { kind: 'move', startTick: start, startTrack: track, anchorId: id, orig: new Map([[id, { start, track }]]), moved: false };
  };

  const onPointerMove = (e: RPE) => {
    const d = drag.current;
    const { x, y, tick, track } = toContent(e);
    const project = getProject();
    if (!d) {
      const hit = hitClip(x, y);
      const cur = ui().playlistTool === 'slice' && hit ? 'crosshair' : hit?.edge ? 'ew-resize' : hit ? 'grab' : ui().playlistTool === 'draw' ? 'copy' : 'default';
      if (cur !== hoverCursor.current) {
        hoverCursor.current = cur;
        (e.currentTarget as HTMLElement).style.cursor = cur;
      }
      return;
    }
    const sn = snapOf(e);
    if (d.kind === 'move') {
      const a = d.orig.get(d.anchorId);
      if (!a) return;
      const newAnchor = Math.max(0, snapTicks(a.start + (tick - d.startTick), sn));
      let dt = newAnchor - a.start;
      let dTrack = track - d.startTrack;
      const origs = [...d.orig.values()];
      const minStart = Math.min(...origs.map((o) => o.start));
      if (minStart + dt < 0) dt = -minStart;
      const minT = Math.min(...origs.map((o) => o.track));
      const maxT = Math.max(...origs.map((o) => o.track));
      dTrack = clamp(dTrack, -minT, project.tracks.length - 1 - maxT);
      if (dt === 0 && dTrack === 0 && !d.moved) return;
      d.moved = true;
      update('Move clips', (dr) => {
        for (const c of dr.clips) {
          const o = d.orig.get(c.id);
          if (!o) continue;
          c.start = o.start + dt;
          c.trackId = dr.tracks[o.track + dTrack].id;
        }
      });
    } else if (d.kind === 'resize') {
      const end = Math.max(d.origStart + sn, snapTicks(tick, sn));
      const len = Math.min(d.maxLen, Math.max(sn > 1 ? sn : 6, end - d.origStart));
      update('Resize clip', (dr) => {
        const c = dr.clips.find((k) => k.id === d.id);
        if (c) c.length = Math.round(len);
      });
    } else if (d.kind === 'trim') {
      const end = d.origStart + d.origLen;
      let start = snapTicks(tick, sn);
      const minStart = d.audio ? d.origStart - secondsToTicks(d.origOffset, project.bpm) : 0;
      start = clamp(start, Math.max(0, minStart), end - Math.max(6, sn));
      const delta = start - d.origStart;
      update('Trim clip', (dr) => {
        const c = dr.clips.find((k) => k.id === d.id);
        if (!c) return;
        c.start = start;
        c.length = end - start;
        c.offset = d.audio ? Math.max(0, d.origOffset + (delta / PPQ) * (60 / project.bpm)) : d.origOffset + delta;
      });
    } else if (d.kind === 'marquee') {
      d.x1 = x;
      d.y1 = y;
      const v = view.current;
      const x0 = Math.min(d.x0, d.x1) / v.pxPerTick;
      const x1 = Math.max(d.x0, d.x1) / v.pxPerTick;
      const t0 = Math.floor(Math.min(d.y0, d.y1) / v.trackH);
      const t1 = Math.floor(Math.max(d.y0, d.y1) / v.trackH);
      const ids = project.clips.filter((c) => {
        const ti = trackIndexOf(project, c.trackId);
        return ti >= t0 && ti <= t1 && c.start < x1 && c.start + c.length > x0;
      }).map((c) => c.id);
      setUI({ selectedClipIds: d.add ? [...new Set([...ui().selectedClipIds, ...ids])] : ids });
    }
  };

  const onPointerUp = () => {
    const d = drag.current;
    drag.current = null;
    if (d && d.kind !== 'marquee') endGesture();
    dirty.current = true;
  };

  const onDoubleClick = (e: React.MouseEvent) => {
    const { x, y } = toContent(e);
    const hit = hitClip(x, y);
    if (hit?.clip.kind === 'pattern' && hit.clip.patternId) {
      const pat = getProject().patterns.find((p) => p.id === hit.clip.patternId);
      const hasNotes = pat && Object.values(pat.notes).some((l) => l.length);
      setUI({ selectedPatternId: hit.clip.patternId, dockOpen: true, dockTab: hasNotes ? 'piano' : 'rack' });
      if (hasNotes && pat) {
        const ch = Object.keys(pat.notes).find((k) => pat.notes[k].length);
        if (ch) setUI({ pianoChannelId: ch });
      }
    }
  };

  const onContextMenu = (e: React.MouseEvent) => {
    e.preventDefault();
    const { x, y, tick } = toContent(e);
    const hit = hitClip(x, y);
    const project = getProject();
    if (!hit) {
      showMenu(e.clientX, e.clientY, [
        { label: 'Paste selected pattern here', disabled: !ui().selectedPatternId, onClick: () => onPointerDownPlace(e) },
        { label: 'Select all', shortcut: 'Ctrl+A', onClick: () => setUI({ selectedClipIds: project.clips.map((c) => c.id) }) },
        { label: 'Set loop to this bar', onClick: () => { const bar = ticksPerBar(project.timeSig); const s = Math.floor(tick / bar) * bar; setLoop({ enabled: true, start: s, end: s + bar }); } },
      ]);
      return;
    }
    const c = hit.clip;
    const ids = ui().selectedClipIds.includes(c.id) ? ui().selectedClipIds : [c.id];
    if (!ui().selectedClipIds.includes(c.id)) setUI({ selectedClipIds: [c.id] });
    const items: MenuItem[] = [
      { label: ids.length > 1 ? `Delete ${ids.length} clips` : 'Delete', shortcut: 'Del', danger: true, onClick: () => { deleteClips(new Set(ids)); setUI({ selectedClipIds: [] }); } },
      { label: 'Duplicate', shortcut: 'Ctrl+D', onClick: () => setUI({ selectedClipIds: duplicateClips(new Set(ids)) }) },
      { label: 'Slice here', onClick: () => sliceClip(c.id, snapTicks(tick, ui().playlistSnap)) },
      { separator: true },
    ];
    if (c.kind === 'pattern' && c.patternId) {
      const pat = project.patterns.find((p) => p.id === c.patternId);
      items.push(
        { label: 'Open in piano roll', onClick: () => setUI({ selectedPatternId: c.patternId!, dockTab: 'piano', dockOpen: true }) },
        { label: 'Open in channel rack', onClick: () => setUI({ selectedPatternId: c.patternId!, dockTab: 'rack', dockOpen: true }) },
        { label: 'Make unique', onClick: () => { const id = clonePattern(c.patternId!); if (id) { updateClips(new Set([c.id]), (k) => { k.patternId = id; }, 'Make unique'); setUI({ selectedPatternId: id }); } } },
        { label: 'Rename pattern…', onClick: () => { void askText('Pattern name', pat?.name).then((n) => { if (n && pat) updatePattern(pat.id, { name: n }); }); } },
        { label: 'Pattern colour', submenu: colorMenu(pat?.color ?? '', (col) => pat && updatePattern(pat.id, { color: col })) },
        { label: 'Fit length to pattern', onClick: () => pat && updateClips(new Set([c.id]), (k) => { k.length = patternLength(pat, project.timeSig); k.offset = 0; }, 'Fit clip') },
      );
    } else {
      items.push(
        { label: 'Gain +3 dB', onClick: () => updateClips(new Set(ids), (k) => { k.gain = Math.min(4, (k.gain ?? 1) * 1.413); }, 'Clip gain') },
        { label: 'Gain −3 dB', onClick: () => updateClips(new Set(ids), (k) => { k.gain = (k.gain ?? 1) / 1.413; }, 'Clip gain') },
        { label: 'Reset gain', onClick: () => updateClips(new Set(ids), (k) => { k.gain = 1; }, 'Clip gain') },
        { label: 'Create sampler channel from clip', onClick: () => { if (c.assetId) setUI({ selectedChannelId: addChannel('sampler', { assetId: c.assetId, name: assets.name(c.assetId) }) }); } },
      );
    }
    showMenu(e.clientX, e.clientY, items);
  };

  const onPointerDownPlace = (e: { clientX: number; clientY: number }) => {
    const { tick, track } = toContent(e);
    const project = getProject();
    const pattern = project.patterns.find((p) => p.id === ui().selectedPatternId);
    if (!pattern || track < 0 || track >= project.tracks.length) return;
    update('Add clip', (d) => {
      d.clips.push({ id: uid('c'), trackId: project.tracks[track].id, kind: 'pattern', start: Math.max(0, snapTicks(tick, ui().playlistSnap, 'floor')), length: patternLength(pattern, project.timeSig), offset: 0, patternId: pattern.id });
    });
  };

  const onWheel = (e: React.WheelEvent) => {
    const vp = vpRef.current!;
    if (e.ctrlKey || e.metaKey) {
      const { tick } = toContent(e);
      const r = gridRef.current!.getBoundingClientRect();
      const mouseX = e.clientX - r.left;
      const z = clamp(ui().playlistZoom * (e.deltaY < 0 ? 1.15 : 1 / 1.15), 3, 400);
      setUI({ playlistZoom: z });
      requestAnimationFrame(() => { vp.scrollLeft = tick * (z / PPQ) - mouseX; });
    } else if (e.altKey) {
      setUI({ trackHeight: clamp(ui().trackHeight + (e.deltaY < 0 ? 4 : -4), 24, 140) });
    } else if (e.shiftKey) {
      vp.scrollLeft += e.deltaY || e.deltaX;
    } else {
      vp.scrollLeft += e.deltaX;
      vp.scrollTop += e.deltaY;
    }
  };

  // ---------- ruler ----------
  const rulerTick = (e: { clientX: number }) => {
    const r = rulerRef.current!.getBoundingClientRect();
    return Math.max(0, (e.clientX - r.left + view.current.scrollX) / view.current.pxPerTick);
  };
  const onRulerDown = (e: RPE) => {
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    const tick = rulerTick(e);
    if (e.button === 2 || e.shiftKey || e.ctrlKey) {
      const s = snapTicks(tick, Math.max(PPQ, ui().playlistSnap), 'floor');
      drag.current = { kind: 'loop', anchor: s };
      beginGesture();
      setLoop({ enabled: true, start: s, end: s + Math.max(PPQ, ui().playlistSnap) });
      return;
    }
    drag.current = { kind: 'seek' };
    if (engine.transport.mode !== 'song') {
      engine.transport.setMode('song');
    }
    engine.transport.setPosition(snapTicks(tick, e.altKey ? 1 : ui().playlistSnap, 'floor'));
  };
  const onRulerMove = (e: RPE) => {
    const d = drag.current;
    if (!d) return;
    const tick = rulerTick(e);
    if (d.kind === 'seek' && !engine.transport.playing) engine.transport.setPosition(snapTicks(tick, e.altKey ? 1 : ui().playlistSnap, 'floor'));
    if (d.kind === 'loop') {
      const sn = Math.max(TICKS_PER_STEP, ui().playlistSnap);
      const b = snapTicks(tick, sn);
      const start = Math.min(d.anchor, b);
      const end = Math.max(d.anchor + (b >= d.anchor ? 0 : sn), b);
      if (end > start) setLoop({ enabled: true, start, end });
    }
    dirty.current = true;
  };
  const onRulerUp = () => {
    if (drag.current?.kind === 'loop') endGesture();
    drag.current = null;
  };

  // ---------- drag & drop from browser / OS ----------
  const onDragOver = (e: DragEvent) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
    if (!dropHint) setDropHint(true);
  };
  const onDrop = async (e: DragEvent) => {
    e.preventDefault();
    setDropHint(false);
    const { tick, track } = toContent(e);
    const project = getProject();
    const tIdx = clamp(track, 0, project.tracks.length - 1);
    const trackId = project.tracks[tIdx].id;
    const start = Math.max(0, snapTicks(tick, ui().playlistSnap, 'floor'));
    const files = Array.from(e.dataTransfer.files ?? []).filter((f) => f.type.startsWith('audio') || /\.(wav|mp3|ogg|flac|m4a|aac|webm)$/i.test(f.name));
    if (files.length) {
      let at = start;
      for (const f of files) {
        const res = await decodeAudioFile(f);
        if (res) {
          placeAudioClip(res.id, trackId, at);
          at += Math.round(secondsToTicks(res.buffer.duration, project.bpm));
        }
      }
      return;
    }
    const payload = readDragPayload(e);
    setUI({ dragPayload: null });
    if (!payload) return;
    if (payload.type === 'pattern') {
      const pat = project.patterns.find((p) => p.id === payload.patternId);
      if (pat) update('Add clip', (d) => { d.clips.push({ id: uid('c'), trackId, kind: 'pattern', start, length: patternLength(pat, project.timeSig), offset: 0, patternId: pat.id }); });
    } else if (payload.type === 'sample') {
      placeAudioClip(payload.assetId, trackId, start);
    }
  };

  const setTool = (t: PlaylistTool) => setUI({ playlistTool: t });
  const pattern = patterns.find((p) => p.id === selectedPatternId);

  const headerMenu = useCallback((e: React.MouseEvent, trackId: string) => {
    e.preventDefault();
    const project = getProject();
    const tr = project.tracks.find((t) => t.id === trackId);
    if (!tr) return;
    showMenu(e.clientX, e.clientY, [
      { label: 'Rename…', onClick: () => { void askText('Track name', tr.name).then((n) => { if (n) updateTrack(tr.id, { name: n }, 'Rename track'); }); } },
      { label: 'Colour', submenu: colorMenu(tr.color, (c) => updateTrack(tr.id, { color: c }, 'Track colour')) },
      { label: 'Record into this track', checked: ui().selectedTrackId === tr.id, onClick: () => setUI({ selectedTrackId: tr.id }) },
      { label: 'Route audio clips to', submenu: project.mixer.map((m, i) => ({ label: i === 0 ? 'Master' : `${i} · ${m.name}`, checked: tr.mixerTrackId === m.id, onClick: () => updateTrack(tr.id, { mixerTrackId: m.id }, 'Route track') })) },
      { separator: true },
      { label: tr.mute ? 'Unmute' : 'Mute', onClick: () => updateTrack(tr.id, { mute: !tr.mute }, 'Mute track') },
      { label: 'Add track', onClick: () => addTrack() },
      { label: 'Delete track', danger: true, disabled: project.tracks.length <= 1, onClick: () => removeTrack(tr.id) },
    ]);
  }, []);

  return (
    <div className="playlist" onPointerDown={() => setUI({ focus: 'playlist' })}>
      <div className="panel-title">
        <Icon name="playlist" size={13} />
        Playlist <span className="crumb">– Arrangement › {pattern?.name ?? '—'}</span>
        <div className="pl-tools">
          <button className={`btn icon small ${tool === 'draw' ? 'on' : ''}`} title="Draw / paint pattern clips" onClick={() => setTool('draw')}><Icon name="pencil" size={12} /></button>
          <button className={`btn icon small ${tool === 'select' ? 'on' : ''}`} title="Select (marquee)" onClick={() => setTool('select')}><Icon name="pointer" size={12} /></button>
          <button className={`btn icon small ${tool === 'slice' ? 'on' : ''}`} title="Slice clips" onClick={() => setTool('slice')}><Icon name="scissors" size={12} /></button>
        </div>
        <div className="pl-tools">
          <Icon name="magnet" size={12} style={{ color: 'var(--dim)' }} />
          <select className="input small-select" value={snap} onChange={(e) => setUI({ playlistSnap: Number(e.target.value) })} title="Snap to grid (hold Alt to bypass)">
            {SNAPS.map((s) => <option key={s.value} value={s.value}>{s.label}</option>)}
          </select>
        </div>
        <div className="pl-tools" title="Horizontal zoom (Ctrl + wheel)">
          <Icon name="search" size={12} style={{ color: 'var(--dim)' }} />
          <input type="range" min={3} max={200} value={zoom} onChange={(e) => setUI({ playlistZoom: Number(e.target.value) })} className="zoom-range" />
        </div>
        <button className={`btn icon small ${follow ? 'on' : ''}`} title="Follow playhead" onClick={() => setUI({ followPlayhead: !follow })}><Icon name="follow" size={12} /></button>
        <span className="spacer" />
        {mode !== 'song' && <span className="pl-mode-hint" onClick={() => engine.transport.setMode('song')}>Pattern mode – click for SONG mode</span>}
        {recording && <span className="pl-rec-hint">● Recording</span>}
        <button className="btn small" onClick={() => addTrack()} title="Add playlist track"><Icon name="plus" size={11} /> Track</button>
      </div>
      <div className="pl-body">
        <div className="pl-patterns">
          <div className="pl-patterns-head">
            Patterns
            <button className="btn ghost icon small" title="New pattern" onClick={() => setUI({ selectedPatternId: addPattern() })}><Icon name="plus" size={11} /></button>
          </div>
          <div className="scroll-y" style={{ flex: 1 }}>
            {patterns.map((p) => (
              <div
                key={p.id}
                className={`pl-pattern ${p.id === selectedPatternId ? 'active' : ''}`}
                style={{ ['--pc' as string]: p.color }}
                draggable
                onDragStart={(e) => { e.dataTransfer.setData('application/x-goofy', JSON.stringify({ type: 'pattern', patternId: p.id })); setUI({ dragPayload: { type: 'pattern', patternId: p.id } }); }}
                onClick={() => { setUI({ selectedPatternId: p.id }); engine.transport.patternId = p.id; }}
                onDoubleClick={() => { void askText('Pattern name', p.name).then((n) => { if (n) updatePattern(p.id, { name: n }); }); }}
                onContextMenu={(e) => {
                  e.preventDefault();
                  showMenu(e.clientX, e.clientY, [
                    { label: 'Rename…', onClick: () => { void askText('Pattern name', p.name).then((n) => { if (n) updatePattern(p.id, { name: n }); }); } },
                    { label: 'Clone', onClick: () => setUI({ selectedPatternId: clonePattern(p.id) }) },
                    { label: 'Colour', submenu: colorMenu(p.color, (c) => updatePattern(p.id, { color: c })) },
                    { label: 'Delete', danger: true, disabled: patterns.length <= 1, onClick: () => removePattern(p.id) },
                  ]);
                }}
              >
                <span className="pl-pattern-color" />
                {p.name}
              </div>
            ))}
          </div>
        </div>
        <div className="pl-main">
          <div className="pl-ruler-row">
            <div className="pl-corner">
              <span className="label">Tracks</span>
            </div>
            <canvas
              ref={rulerRef}
              className="pl-ruler"
              style={{ height: RULER_H }}
              onPointerDown={onRulerDown}
              onPointerMove={onRulerMove}
              onPointerUp={onRulerUp}
              onContextMenu={(e) => e.preventDefault()}
              onDoubleClick={() => setLoop({ enabled: !getProject().loop.enabled })}
              title="Click to set the play position · Shift/right-drag to set a loop region · double-click toggles the loop"
            />
          </div>
          <div className="pl-area">
            <div className="pl-headers">
              <div ref={headersRef} className="pl-headers-inner">
                {tracks.map((t) => (
                  <div
                    key={t.id}
                    className={`pl-header ${selectedTrackId === t.id ? 'active' : ''}`}
                    style={{ height: trackH }}
                    onClick={() => setUI({ selectedTrackId: t.id })}
                    onContextMenu={(e) => headerMenu(e, t.id)}
                  >
                    <span className="pl-header-color" style={{ background: t.color }} />
                    <span className="pl-header-name" onDoubleClick={() => { void askText('Track name', t.name).then((n) => { if (n) updateTrack(t.id, { name: n }, 'Rename track'); }); }}>{t.name}</span>
                    {selectedTrackId === t.id && <Icon name="mic" size={10} style={{ color: 'var(--rec)', opacity: 0.8 }} />}
                    <span
                      className={`led ${t.mute ? '' : 'on'}`}
                      title={t.mute ? 'Unmute track' : 'Mute track'}
                      onClick={(e) => { e.stopPropagation(); updateTrack(t.id, { mute: !t.mute }, 'Mute track'); }}
                    />
                  </div>
                ))}
                <div className="pl-header add" onClick={() => addTrack()}><Icon name="plus" size={11} /> Add track</div>
              </div>
            </div>
            <div className={`pl-grid-wrap ${dropHint ? 'drop-target' : ''}`} onDragOver={onDragOver} onDragLeave={() => setDropHint(false)} onDrop={onDrop}>
              <div ref={vpRef} className="pl-viewport" onScroll={onScroll}>
                <div style={{ width: totalW, height: totalH }} />
              </div>
              <canvas ref={gridRef} className="pl-canvas" />
              <canvas
                ref={overlayRef}
                className="pl-canvas pl-overlay"
                onPointerDown={onPointerDown}
                onPointerMove={onPointerMove}
                onPointerUp={onPointerUp}
                onPointerCancel={onPointerUp}
                onDoubleClick={onDoubleClick}
                onContextMenu={onContextMenu}
                onWheel={onWheel}
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
