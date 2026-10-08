import { useEffect, useRef, useState, type PointerEvent as RPE } from 'react';
import { beginGesture, endGesture, getProject, update, useProject } from '../../state/store';
import { setUI, ui, useUI } from '../../state/ui';
import type { Note } from '../../state/types';
import { PPQ, TICKS_PER_STEP } from '../../state/types';
import { clamp, rgba, shade, snapTicks, ticksPerBar, uid } from '../../state/utils';
import { deleteNotes, quantizeNotes, setKeyScale, updateNotes } from '../../state/actions';
import { isBlackKey, noteName, NOTE_NAMES, scaleMask, SCALE_NAMES } from '../../midi/music';
import { patternLength } from '../../audio/sequencer';
import { engine } from '../../audio/engine';
import { theme, themeVersion } from '../../styles/themes';
import { onFrame } from '../components/frameLoop';
import { Icon } from '../components/Icon';
import { currentPatternTick } from '../channelrack/playhead';
import './pianoroll.css';

const KEYS_W = 56;
const RULER_H = 20;
const VEL_H = 70;
const LOW = 12; // C0
const HIGH = 120; // C9
const ROWS = HIGH - LOW + 1;

const SNAPS = [
  { label: 'None', value: 1 },
  { label: '1/32', value: PPQ / 8 },
  { label: '1/16 (Step)', value: PPQ / 4 },
  { label: '1/8', value: PPQ / 2 },
  { label: '1/4 (Beat)', value: PPQ },
  { label: '1/2', value: PPQ * 2 },
  { label: 'Bar', value: PPQ * 4 },
  { label: '1/12 (Triplet)', value: PPQ / 3 },
];

type Drag =
  | { kind: 'move'; startTick: number; startPitch: number; anchor: string; orig: Map<string, { start: number; pitch: number }>; lastPitch: number }
  | { kind: 'resize'; startTick: number; orig: Map<string, { start: number; length: number }>; anchor: string }
  | { kind: 'marquee'; x0: number; y0: number; x1: number; y1: number; add: boolean }
  | { kind: 'velocity' }
  | { kind: 'keys'; pitch: number };

export function PianoRoll() {
  const vpRef = useRef<HTMLDivElement>(null);
  const gridRef = useRef<HTMLCanvasElement>(null);
  const overlayRef = useRef<HTMLCanvasElement>(null);
  const keysRef = useRef<HTMLCanvasElement>(null);
  const rulerRef = useRef<HTMLCanvasElement>(null);
  const velRef = useRef<HTMLCanvasElement>(null);
  const size = useRef({ w: 0, h: 0, scrollX: 0, scrollY: 0 });
  const dirty = useRef(true);
  const drag = useRef<Drag | null>(null);
  const heldKey = useRef<number | null>(null);
  const [tool, setTool] = useState<'draw' | 'select'>('draw');
  const [ghosts, setGhosts] = useState(true);

  const patterns = useProject((s) => s.project.patterns);
  const channels = useProject((s) => s.project.channels);
  const key = useProject((s) => s.project.key);
  const scale = useProject((s) => s.project.scale);
  const timeSig = useProject((s) => s.project.timeSig);
  const patternId = useUI((s) => s.selectedPatternId);
  const channelId = useUI((s) => s.pianoChannelId);
  const snap = useUI((s) => s.pianoSnap);
  const zoomX = useUI((s) => s.pianoZoomX);
  const zoomY = useUI((s) => s.pianoZoomY);
  const pattern = patterns.find((p) => p.id === patternId) ?? patterns[0];
  const channel = channels.find((c) => c.id === channelId) ?? channels.find((c) => c.kind === 'synth') ?? channels[0];
  const len = pattern ? patternLength(pattern, timeSig) : PPQ * 4;
  const contentW = (len + ticksPerBar(timeSig) * 4) * (zoomX / PPQ);
  const contentH = ROWS * zoomY;

  useEffect(() => {
    if (channel && channel.id !== channelId) setUI({ pianoChannelId: channel.id });
  }, [channel, channelId]);

  useEffect(() => {
    const a = useProject.subscribe((s, p) => { if (s.project !== p.project) dirty.current = true; });
    const b = useUI.subscribe((s, p) => { if (s.selectedNoteIds !== p.selectedNoteIds || s.pianoChannelId !== p.pianoChannelId || s.selectedPatternId !== p.selectedPatternId) dirty.current = true; });
    return () => { a(); b(); };
  }, []);
  useEffect(() => { dirty.current = true; }, [zoomX, zoomY, ghosts, key, scale]);

  // initial vertical scroll to C4-ish
  useEffect(() => {
    const vp = vpRef.current;
    if (vp) vp.scrollTop = (HIGH - 76) * zoomY;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  useEffect(() => {
    const vp = vpRef.current!;
    const resize = () => {
      const w = vp.clientWidth;
      const h = vp.clientHeight;
      const dpr = window.devicePixelRatio || 1;
      const set = (cv: HTMLCanvasElement | null, cw: number, ch: number) => {
        if (!cv) return;
        cv.width = Math.round(cw * dpr);
        cv.height = Math.round(ch * dpr);
        cv.style.width = `${cw}px`;
        cv.style.height = `${ch}px`;
      };
      set(gridRef.current, w, h);
      set(overlayRef.current, w, h);
      set(keysRef.current, KEYS_W, h);
      set(rulerRef.current, w, RULER_H);
      set(velRef.current, w, VEL_H);
      size.current.w = w;
      size.current.h = h;
      dirty.current = true;
    };
    resize();
    const ro = new ResizeObserver(resize);
    ro.observe(vp);
    return () => ro.disconnect();
  }, []);

  const ppt = () => ui().pianoZoomX / PPQ;
  const kh = () => ui().pianoZoomY;

  // drawing
  useEffect(() => {
    let lastTheme = -1;
    return onFrame(() => {
      const s = size.current;
      const t = theme();
      const dpr = window.devicePixelRatio || 1;
      const project = getProject();
      const st = ui();
      const pat = project.patterns.find((p) => p.id === st.selectedPatternId) ?? project.patterns[0];
      const ch = project.channels.find((c) => c.id === st.pianoChannelId);
      if (!pat) return;
      const pxt = st.pianoZoomX / PPQ;
      const rowH = st.pianoZoomY;
      const plen = patternLength(pat, project.timeSig);
      if (themeVersion() !== lastTheme) { lastTheme = themeVersion(); dirty.current = true; }
      if (dirty.current) {
        dirty.current = false;
        const g = gridRef.current!.getContext('2d')!;
        g.setTransform(dpr, 0, 0, dpr, 0, 0);
        const mask = scaleMask(project.key, project.scale);
        g.fillStyle = shade(t.grid, -0.1);
        g.fillRect(0, 0, s.w, s.h);
        const first = Math.floor(s.scrollY / rowH);
        const last = Math.min(ROWS, Math.ceil((s.scrollY + s.h) / rowH));
        for (let r = first; r < last; r++) {
          const pitch = HIGH - r;
          const y = r * rowH - s.scrollY;
          const inScale = mask[pitch % 12];
          g.fillStyle = isBlackKey(pitch) ? shade(t.grid, -0.22) : t.grid;
          g.fillRect(0, y, s.w, rowH);
          if (project.scale !== 'Chromatic' && inScale) {
            g.fillStyle = rgba(t.accent, pitch % 12 === project.key ? 0.1 : 0.04);
            g.fillRect(0, y, s.w, rowH);
          }
          g.fillStyle = pitch % 12 === 0 ? t.gridBar : 'rgba(0,0,0,0.25)';
          g.fillRect(0, y + rowH - 1, s.w, 1);
        }
        const bar = ticksPerBar(project.timeSig);
        const beat = PPQ * (4 / project.timeSig[1]);
        const t0 = s.scrollX / pxt;
        const t1 = (s.scrollX + s.w) / pxt;
        const line = (every: number, col: string) => {
          g.fillStyle = col;
          for (let tk = Math.floor(t0 / every) * every; tk <= t1; tk += every) g.fillRect(Math.round(tk * pxt - s.scrollX), 0, 1, s.h);
        };
        const sn = st.pianoSnap > 1 ? st.pianoSnap : TICKS_PER_STEP;
        if (sn * pxt >= 5) line(sn, rgba(t.gridLine, 0.55));
        line(beat, t.gridLine);
        line(bar, t.gridBar);
        // pattern end
        const endX = plen * pxt - s.scrollX;
        g.fillStyle = 'rgba(0,0,0,0.35)';
        g.fillRect(endX, 0, s.w - endX, s.h);
        g.fillStyle = t.accent;
        g.fillRect(endX, 0, 1, s.h);
        // ghost notes
        if (ghosts) {
          for (const [cid, list] of Object.entries(pat.notes)) {
            if (cid === ch?.id) continue;
            const col = project.channels.find((c) => c.id === cid)?.color ?? '#888';
            g.fillStyle = rgba(col, 0.18);
            for (const n of list) {
              const y = (HIGH - n.pitch) * rowH - s.scrollY;
              g.fillRect(n.start * pxt - s.scrollX, y + 1, n.length * pxt, rowH - 2);
            }
          }
        }
        // notes
        const sel = new Set(st.selectedNoteIds);
        const notes = ch ? pat.notes[ch.id] ?? [] : [];
        const color = ch?.color ?? t.accent;
        g.font = `600 ${Math.min(10, rowH - 3)}px Inter, sans-serif`;
        g.textBaseline = 'middle';
        for (const n of notes) {
          const x = n.start * pxt - s.scrollX;
          const w = Math.max(3, n.length * pxt);
          const y = (HIGH - n.pitch) * rowH - s.scrollY;
          if (x > s.w || x + w < 0 || y > s.h || y + rowH < 0) continue;
          const isSel = sel.has(n.id);
          const base = isSel ? shade(color, 0.45) : color;
          g.fillStyle = shade(base, -0.55 + n.velocity * 0.55);
          g.fillRect(x, y + 1, w, rowH - 2);
          g.fillStyle = base;
          g.fillRect(x, y + 1, w, Math.max(2, (rowH - 2) * 0.35));
          g.strokeStyle = isSel ? '#fff' : shade(color, -0.6);
          g.lineWidth = 1;
          g.strokeRect(x + 0.5, y + 1.5, w - 1, rowH - 3);
          if (w > 26 && rowH >= 11) {
            g.fillStyle = 'rgba(0,0,0,0.75)';
            g.fillText(noteName(n.pitch), x + 3, y + rowH / 2 + 0.5);
          }
        }
        // keyboard
        const k = keysRef.current!.getContext('2d')!;
        k.setTransform(dpr, 0, 0, dpr, 0, 0);
        k.fillStyle = '#0c0d0f';
        k.fillRect(0, 0, KEYS_W, s.h);
        k.font = `${Math.min(9, rowH - 2)}px Inter, sans-serif`;
        k.textBaseline = 'middle';
        for (let r = first; r < last; r++) {
          const pitch = HIGH - r;
          const y = r * rowH - s.scrollY;
          const black = isBlackKey(pitch);
          const held = heldKey.current === pitch;
          k.fillStyle = held ? t.accent : black ? '#1d1f22' : '#e9ecef';
          k.fillRect(black ? 0 : 0, y, black ? KEYS_W * 0.62 : KEYS_W, rowH - (black ? 0 : 1));
          if (!black) {
            k.fillStyle = '#9aa1a8';
            k.fillRect(0, y + rowH - 1, KEYS_W, 1);
          }
          if (pitch % 12 === 0 && rowH >= 8) {
            k.fillStyle = '#333';
            k.fillText(noteName(pitch), KEYS_W - 22, y + rowH / 2);
          }
          if (project.scale !== 'Chromatic' && mask[pitch % 12]) {
            k.fillStyle = t.accent;
            k.fillRect(KEYS_W - 3, y + 1, 3, rowH - 2);
          }
        }
        // ruler
        const ru = rulerRef.current!.getContext('2d')!;
        ru.setTransform(dpr, 0, 0, dpr, 0, 0);
        ru.fillStyle = t.panel2;
        ru.fillRect(0, 0, s.w, RULER_H);
        ru.font = '10px Inter, sans-serif';
        ru.textBaseline = 'middle';
        for (let b = Math.floor(t0 / bar); b * bar <= t1; b++) {
          const x = Math.round(b * bar * pxt - s.scrollX);
          ru.fillStyle = t.gridBar;
          ru.fillRect(x, 8, 1, RULER_H - 8);
          ru.fillStyle = t.text;
          ru.fillText(String(b + 1), x + 3, 9);
        }
        // velocity lane
        const v = velRef.current!.getContext('2d')!;
        v.setTransform(dpr, 0, 0, dpr, 0, 0);
        v.fillStyle = t.panel;
        v.fillRect(0, 0, s.w, VEL_H);
        v.fillStyle = 'rgba(255,255,255,0.05)';
        v.fillRect(0, VEL_H / 2, s.w, 1);
        for (const n of notes) {
          const x = n.start * pxt - s.scrollX;
          if (x < -4 || x > s.w) continue;
          const hgt = n.velocity * (VEL_H - 8);
          const isSel = sel.has(n.id);
          v.fillStyle = isSel ? shade(color, 0.4) : color;
          v.fillRect(x, VEL_H - hgt, 3, hgt);
          v.beginPath();
          v.arc(x + 1.5, VEL_H - hgt, 3, 0, Math.PI * 2);
          v.fill();
        }
      }
      // overlay: playhead + marquee
      const o = overlayRef.current!.getContext('2d')!;
      o.setTransform(dpr, 0, 0, dpr, 0, 0);
      o.clearRect(0, 0, s.w, s.h);
      const pt = currentPatternTick(pat.id);
      if (pt >= 0) {
        const x = pt * pxt - s.scrollX;
        o.fillStyle = t.accent2;
        o.shadowColor = t.accent2;
        o.shadowBlur = 6;
        o.fillRect(Math.round(x), 0, 1.5, s.h);
        o.shadowBlur = 0;
        if (st.followPlayhead && vpRef.current && (x > s.w * 0.95 || x < 0)) vpRef.current.scrollLeft = Math.max(0, pt * pxt - 40);
      }
      const d = drag.current;
      if (d?.kind === 'marquee') {
        const x = Math.min(d.x0, d.x1) - s.scrollX;
        const y = Math.min(d.y0, d.y1) - s.scrollY;
        o.fillStyle = rgba(t.accent, 0.12);
        o.strokeStyle = t.accent;
        o.fillRect(x, y, Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0));
        o.strokeRect(x + 0.5, y + 0.5, Math.abs(d.x1 - d.x0), Math.abs(d.y1 - d.y0));
      }
    });
  }, [ghosts]);

  if (!pattern || !channel) return <div className="empty-hint">Add a channel to start writing notes.</div>;

  const notesOf = () => getProject().patterns.find((p) => p.id === pattern.id)?.notes[channel.id] ?? [];

  const toContent = (e: { clientX: number; clientY: number }) => {
    const r = gridRef.current!.getBoundingClientRect();
    const x = e.clientX - r.left + size.current.scrollX;
    const y = e.clientY - r.top + size.current.scrollY;
    return { x, y, tick: x / ppt(), pitch: HIGH - Math.floor(y / kh()) };
  };

  const hitNote = (x: number, y: number): { note: Note; edge: boolean } | null => {
    const notes = notesOf();
    const pxt = ppt();
    for (let i = notes.length - 1; i >= 0; i--) {
      const n = notes[i];
      const nx = n.start * pxt;
      const nw = Math.max(3, n.length * pxt);
      const ny = (HIGH - n.pitch) * kh();
      if (x >= nx && x <= nx + nw && y >= ny && y < ny + kh()) return { note: n, edge: x > nx + nw - Math.min(7, nw / 3) };
    }
    return null;
  };

  const preview = (pitch: number, vel = 0.8) => engine.previewNote(channel.id, pitch, vel, 0.25);
  const snapOf = (e: { altKey: boolean }) => (e.altKey ? 1 : ui().pianoSnap);

  const onDown = (e: RPE) => {
    setUI({ focus: 'piano' });
    void engine.resume();
    const { x, y, tick, pitch } = toContent(e);
    const hit = hitNote(x, y);
    if (e.button === 2) {
      if (hit) {
        deleteNotes(pattern.id, channel.id, new Set([hit.note.id]));
        setUI({ selectedNoteIds: ui().selectedNoteIds.filter((i) => i !== hit.note.id) });
      }
      return;
    }
    if (e.button !== 0) return;
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    const s = ui();
    if (hit) {
      const n = hit.note;
      let sel = s.selectedNoteIds.includes(n.id) ? s.selectedNoteIds : [n.id];
      if (e.ctrlKey || e.metaKey) sel = s.selectedNoteIds.includes(n.id) ? s.selectedNoteIds.filter((i) => i !== n.id) : [...s.selectedNoteIds, n.id];
      setUI({ selectedNoteIds: sel });
      beginGesture();
      const notes = notesOf().filter((k) => sel.includes(k.id));
      if (hit.edge) {
        drag.current = { kind: 'resize', startTick: tick, anchor: n.id, orig: new Map(notes.map((k) => [k.id, { start: k.start, length: k.length }])) };
      } else {
        if (e.shiftKey) {
          // shift-drag copies
          const copies = notes.map((k) => ({ ...k, id: uid('n') }));
          update('Copy notes', (d) => { d.patterns.find((p) => p.id === pattern.id)!.notes[channel.id].push(...copies); });
          const anchorCopy = copies[notes.findIndex((k) => k.id === n.id)];
          setUI({ selectedNoteIds: copies.map((c) => c.id) });
          drag.current = { kind: 'move', startTick: tick, startPitch: pitch, anchor: anchorCopy.id, orig: new Map(copies.map((k) => [k.id, { start: k.start, pitch: k.pitch }])), lastPitch: n.pitch };
        } else {
          drag.current = { kind: 'move', startTick: tick, startPitch: pitch, anchor: n.id, orig: new Map(notes.map((k) => [k.id, { start: k.start, pitch: k.pitch }])), lastPitch: n.pitch };
        }
        preview(n.pitch, n.velocity);
      }
      return;
    }
    if (tool === 'select' || e.ctrlKey || e.metaKey) {
      drag.current = { kind: 'marquee', x0: x, y0: y, x1: x, y1: y, add: e.ctrlKey || e.metaKey };
      return;
    }
    if (pitch < LOW || pitch > HIGH) return;
    beginGesture();
    const id = uid('n');
    const note: Note = { id, pitch, start: Math.max(0, snapTicks(tick, snapOf(e), 'floor')), length: ui().lastNoteLength, velocity: 0.8 };
    update('Add note', (d) => {
      const p = d.patterns.find((pp) => pp.id === pattern.id)!;
      (p.notes[channel.id] ??= []).push(note);
    });
    setUI({ selectedNoteIds: [id] });
    preview(pitch);
    drag.current = { kind: 'move', startTick: note.start, startPitch: pitch, anchor: id, orig: new Map([[id, { start: note.start, pitch }]]), lastPitch: pitch };
  };

  const onMove = (e: RPE) => {
    const d = drag.current;
    const { x, y, tick, pitch } = toContent(e);
    if (!d) {
      const hit = hitNote(x, y);
      (e.currentTarget as HTMLElement).style.cursor = hit?.edge ? 'ew-resize' : hit ? 'grab' : tool === 'draw' ? 'crosshair' : 'default';
      return;
    }
    const sn = snapOf(e);
    if (d.kind === 'move') {
      const a = d.orig.get(d.anchor)!;
      const ns = Math.max(0, snapTicks(a.start + (tick - d.startTick), sn));
      let dt = ns - a.start;
      let dp = pitch - d.startPitch;
      const origs = [...d.orig.values()];
      const minStart = Math.min(...origs.map((o) => o.start));
      if (minStart + dt < 0) dt = -minStart;
      dp = clamp(dp, LOW - Math.min(...origs.map((o) => o.pitch)), HIGH - Math.max(...origs.map((o) => o.pitch)));
      updateNotes(pattern.id, channel.id, (n) => {
        const o = d.orig.get(n.id);
        if (o) { n.start = o.start + dt; n.pitch = o.pitch + dp; }
      }, new Set(d.orig.keys()));
      const newPitch = a.pitch + dp;
      if (newPitch !== d.lastPitch) {
        d.lastPitch = newPitch;
        preview(newPitch);
      }
    } else if (d.kind === 'resize') {
      const a = d.orig.get(d.anchor)!;
      const minLen = sn > 1 ? sn : 6;
      const newEnd = Math.max(a.start + minLen, snapTicks(tick, sn));
      const delta = newEnd - (a.start + a.length);
      updateNotes(pattern.id, channel.id, (n) => {
        const o = d.orig.get(n.id);
        if (o) n.length = Math.max(minLen, o.length + delta);
      }, new Set(d.orig.keys()));
      setUI({ lastNoteLength: Math.max(minLen, a.length + delta) });
    } else if (d.kind === 'marquee') {
      d.x1 = x;
      d.y1 = y;
      const pxt = ppt();
      const t0 = Math.min(d.x0, d.x1) / pxt;
      const t1 = Math.max(d.x0, d.x1) / pxt;
      const p0 = HIGH - Math.floor(Math.max(d.y0, d.y1) / kh());
      const p1 = HIGH - Math.floor(Math.min(d.y0, d.y1) / kh());
      const ids = notesOf().filter((n) => n.pitch >= p0 && n.pitch <= p1 && n.start < t1 && n.start + n.length > t0).map((n) => n.id);
      setUI({ selectedNoteIds: d.add ? [...new Set([...ui().selectedNoteIds, ...ids])] : ids });
    }
  };

  const onUp = () => {
    const d = drag.current;
    drag.current = null;
    if (d && (d.kind === 'move' || d.kind === 'resize')) endGesture();
    dirty.current = true;
  };

  const onWheel = (e: React.WheelEvent) => {
    const vp = vpRef.current!;
    if (e.ctrlKey || e.metaKey) {
      const { tick } = toContent(e);
      const r = gridRef.current!.getBoundingClientRect();
      const mx = e.clientX - r.left;
      const z = clamp(ui().pianoZoomX * (e.deltaY < 0 ? 1.15 : 1 / 1.15), 15, 600);
      setUI({ pianoZoomX: z });
      requestAnimationFrame(() => { vp.scrollLeft = tick * (z / PPQ) - mx; });
    } else if (e.altKey) {
      setUI({ pianoZoomY: clamp(ui().pianoZoomY + (e.deltaY < 0 ? 1 : -1), 7, 30) });
    } else if (e.shiftKey) {
      vp.scrollLeft += e.deltaY || e.deltaX;
    } else {
      vp.scrollLeft += e.deltaX;
      vp.scrollTop += e.deltaY;
    }
  };

  // keyboard (left)
  const keyPitch = (e: { clientY: number }) => {
    const r = keysRef.current!.getBoundingClientRect();
    return HIGH - Math.floor((e.clientY - r.top + size.current.scrollY) / kh());
  };
  const keyDown = (e: RPE) => {
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    void engine.resume();
    const p = keyPitch(e);
    heldKey.current = p;
    drag.current = { kind: 'keys', pitch: p };
    engine.previewNote(channel.id, p, 0.85);
    dirty.current = true;
  };
  const keyMove = (e: RPE) => {
    const d = drag.current;
    if (d?.kind !== 'keys') return;
    const p = keyPitch(e);
    if (p !== d.pitch) {
      engine.previewNoteOff(channel.id, d.pitch);
      engine.previewNote(channel.id, p, 0.85);
      d.pitch = p;
      heldKey.current = p;
      dirty.current = true;
    }
  };
  const keyUp = () => {
    const d = drag.current;
    if (d?.kind === 'keys') engine.previewNoteOff(channel.id, d.pitch);
    drag.current = null;
    heldKey.current = null;
    dirty.current = true;
  };

  // velocity lane
  const velApply = (e: { clientX: number; clientY: number }) => {
    const r = velRef.current!.getBoundingClientRect();
    const x = e.clientX - r.left + size.current.scrollX;
    const vel = clamp(1 - (e.clientY - r.top - 4) / (VEL_H - 8), 0.02, 1);
    const pxt = ppt();
    const sel = new Set(ui().selectedNoteIds);
    const targets = notesOf().filter((n) => Math.abs(n.start * pxt - x) < 5 && (sel.size === 0 || sel.has(n.id) || !notesOf().some((m) => sel.has(m.id) && Math.abs(m.start * pxt - x) < 5)));
    if (targets.length) updateNotes(pattern.id, channel.id, (n) => { n.velocity = Math.round(vel * 100) / 100; }, new Set(targets.map((n) => n.id)));
  };

  const selectedCount = useUI.getState().selectedNoteIds.length;
  const isEmpty = !(pattern.notes[channel.id]?.length);
  const elsewhere = isEmpty ? patterns.filter((p) => p.id !== pattern.id && p.notes[channel.id]?.length) : [];

  return (
    <div className="piano" onPointerDown={() => setUI({ focus: 'piano' })}>
      <div className="piano-head">
        <select className="input" value={pattern.id} onChange={(e) => setUI({ selectedPatternId: e.target.value, selectedNoteIds: [] })} title="Pattern">
          {patterns.map((p) => <option key={p.id} value={p.id}>{p.name}</option>)}
        </select>
        <select className="input" value={channel.id} onChange={(e) => setUI({ pianoChannelId: e.target.value, selectedChannelId: e.target.value, selectedNoteIds: [] })} title="Channel" style={{ borderLeft: `4px solid ${channel.color}` }}>
          {channels.map((c) => <option key={c.id} value={c.id}>{c.name}</option>)}
        </select>
        <div className="pl-tools">
          <button className={`btn icon small ${tool === 'draw' ? 'on' : ''}`} title="Draw notes" onClick={() => setTool('draw')}><Icon name="pencil" size={12} /></button>
          <button className={`btn icon small ${tool === 'select' ? 'on' : ''}`} title="Select notes" onClick={() => setTool('select')}><Icon name="pointer" size={12} /></button>
        </div>
        <div className="pl-tools">
          <Icon name="magnet" size={12} style={{ color: 'var(--dim)' }} />
          <select className="input small-select" value={snap} onChange={(e) => setUI({ pianoSnap: Number(e.target.value) })} title="Snap (hold Alt to bypass)">
            {SNAPS.map((s) => <option key={s.label} value={s.value}>{s.label}</option>)}
          </select>
          <button className="btn small" title="Quantize selected (or all) notes to the snap grid" onClick={() => {
            const sel = ui().selectedNoteIds;
            quantizeNotes(pattern.id, channel.id, Math.max(TICKS_PER_STEP / 2, ui().pianoSnap), sel.length ? new Set(sel) : undefined);
          }}>Quantize</button>
        </div>
        <div className="pl-tools">
          <span className="label">Scale</span>
          <select className="input small-select" value={key} onChange={(e) => setKeyScale(Number(e.target.value), scale)}>
            {NOTE_NAMES.map((n, i) => <option key={n} value={i}>{n}</option>)}
          </select>
          <select className="input small-select" value={scale} onChange={(e) => setKeyScale(key, e.target.value)}>
            {SCALE_NAMES.map((s) => <option key={s}>{s}</option>)}
          </select>
        </div>
        <button className={`btn small ${ghosts ? 'on' : ''}`} onClick={() => setGhosts(!ghosts)} title="Show notes of other channels">Ghosts</button>
        <div className="pl-tools" title="Zoom (Ctrl/Alt + wheel)">
          <input type="range" className="zoom-range" min={15} max={400} value={zoomX} onChange={(e) => setUI({ pianoZoomX: Number(e.target.value) })} />
          <input type="range" className="zoom-range short" min={7} max={30} value={zoomY} onChange={(e) => setUI({ pianoZoomY: Number(e.target.value) })} />
        </div>
        <span className="spacer" />
        <span className="label">{selectedCount ? `${selectedCount} selected · ` : ''}right-click deletes · shift-drag copies</span>
      </div>
      <div className="piano-body">
        <div className="piano-ruler-row">
          <div style={{ width: KEYS_W, flexShrink: 0, background: 'var(--panel2)', borderBottom: '1px solid var(--border)' }} />
          <canvas ref={rulerRef} className="piano-ruler" />
        </div>
        <div className="piano-main">
          <canvas ref={keysRef} className="piano-keys" onPointerDown={keyDown} onPointerMove={keyMove} onPointerUp={keyUp} onPointerCancel={keyUp} />
          <div className="piano-grid-wrap">
            <div ref={vpRef} className="pl-viewport" onScroll={() => { size.current.scrollX = vpRef.current!.scrollLeft; size.current.scrollY = vpRef.current!.scrollTop; dirty.current = true; }}>
              <div style={{ width: contentW, height: contentH }} />
            </div>
            <canvas ref={gridRef} className="pl-canvas" />
            {isEmpty && (
              <div className="piano-empty">
                Click in the grid to draw notes for <b style={{ color: channel.color }}>{channel.name}</b>
                {elsewhere.length > 0 && (
                  <span> · notes exist in {elsewhere.map((p) => (
                    <button key={p.id} className="btn small" onClick={() => setUI({ selectedPatternId: p.id, selectedNoteIds: [] })}>{p.name}</button>
                  ))}</span>
                )}
              </div>
            )}
            <canvas
              ref={overlayRef}
              className="pl-canvas pl-overlay"
              onPointerDown={onDown}
              onPointerMove={onMove}
              onPointerUp={onUp}
              onPointerCancel={onUp}
              onContextMenu={(e) => e.preventDefault()}
              onWheel={onWheel}
            />
          </div>
        </div>
        <div className="piano-vel-row">
          <div className="piano-vel-label" style={{ width: KEYS_W }}>VEL</div>
          <canvas
            ref={velRef}
            className="piano-vel"
            onPointerDown={(e) => { (e.currentTarget as Element).setPointerCapture(e.pointerId); beginGesture(); drag.current = { kind: 'velocity' }; velApply(e); }}
            onPointerMove={(e) => { if (drag.current?.kind === 'velocity') velApply(e); }}
            onPointerUp={() => { drag.current = null; endGesture(); }}
          />
        </div>
      </div>
    </div>
  );
}
