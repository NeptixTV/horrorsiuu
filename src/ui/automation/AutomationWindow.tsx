// Automation editor: draw the envelope of an automation clip. Click to add a
// point, drag to move, right-click / double-click to delete, or start from a
// shape (ramps, sine, saw, square, random steps).
import { useEffect, useRef, useState, type PointerEvent as RPE } from 'react';
import type { AutomationPoint, AutomationTarget, Clip } from '../../state/types';
import { PPQ, TICKS_PER_STEP } from '../../state/types';
import { beginGesture, endGesture, getProject, update, useProject } from '../../state/store';
import { closeWindow, openWindow, type FloatingWindow } from '../../state/ui';
import { currentNorm, makeAutomationClip, shapePoints, targetInfo, valueAt } from '../../audio/automation';
import { songLength } from '../../audio/sequencer';
import { makeTrack } from '../../state/defaults';
import { ticksPerBar } from '../../state/utils';
import { engine } from '../../audio/engine';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { theme } from '../../styles/themes';
import './automation.css';

const W = 760;
const H = 230;
const PAD = 10;

function setPoints(clipId: string, points: AutomationPoint[], label = 'Edit automation') {
  const sorted = [...points].map((p) => ({ t: Math.round(p.t), v: Math.max(0, Math.min(1, p.v)) })).sort((a, b) => a.t - b.t);
  update(label, (d) => {
    const c = d.clips.find((x) => x.id === clipId);
    if (c?.automation) c.automation.points = sorted;
  });
}

/** Creates an automation clip for a target on a new playlist track and opens the editor. */
export function createAutomation(target: AutomationTarget) {
  const p = getProject();
  const info = targetInfo(p, target);
  if (!info) return;
  const existing = p.clips.find((c) => c.kind === 'automation' && c.automation && JSON.stringify(c.automation.target) === JSON.stringify(target));
  if (existing) {
    openWindow({ kind: 'automation', clipId: existing.id });
    return;
  }
  const bar = ticksPerBar(p.timeSig);
  const start = engine.transport?.mode === 'song' ? Math.floor(engine.transport.getTick() / bar) * bar : 0;
  const end = Math.max(start + bar * 4, songLength(p));
  const track = makeTrack(p.tracks.length + 1);
  track.name = `Auto · ${info.label}`.slice(0, 40);
  track.color = '#8f6bff';
  const clip = makeAutomationClip(p, target, track.id, start, end - start);
  update(`Automate ${info.label}`, (d) => {
    d.tracks.push(track);
    d.clips.push(clip);
  });
  openWindow({ kind: 'automation', clipId: clip.id });
}

export function AutomationWindow({ w, clipId }: { w: FloatingWindow; clipId: string }) {
  const clip = useProject((s) => s.project.clips.find((c) => c.id === clipId));
  const project = useProject((s) => s.project);
  const ref = useRef<HTMLCanvasElement>(null);
  const drag = useRef<{ index: number } | null>(null);
  const [hover, setHover] = useState<{ t: number; v: number } | null>(null);
  const [snap, setSnap] = useState(true);
  const [cycle, setCycle] = useState(PPQ * 4);
  const [selected, setSelected] = useState<number | null>(null);

  useEffect(() => {
    if (!clip) closeWindow(w.id);
  }, [clip, w.id]);

  const a = clip?.automation;
  const info = a ? targetInfo(project, a.target) : null;
  const len = clip?.length ?? 1;
  const off = clip?.offset ?? 0;

  // drawing
  useEffect(() => {
    const cv = ref.current;
    if (!cv || !a || !clip) return;
    const dpr = window.devicePixelRatio || 1;
    cv.width = W * dpr;
    cv.height = H * dpr;
    const g = cv.getContext('2d')!;
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    const th = theme();
    g.fillStyle = th.grid;
    g.fillRect(0, 0, W, H);
    const xOf = (t: number) => PAD + ((t - off) / len) * (W - PAD * 2);
    const yOf = (v: number) => PAD + (1 - v) * (H - PAD * 2);
    const bar = ticksPerBar(project.timeSig);
    const beat = PPQ * (4 / project.timeSig[1]);
    for (let t = Math.ceil(off / beat) * beat; t <= off + len; t += beat) {
      g.fillStyle = t % bar === 0 ? th.gridBar : th.gridLine;
      g.fillRect(Math.round(xOf(t)), 0, 1, H);
    }
    for (const v of [0.25, 0.5, 0.75]) {
      g.fillStyle = th.gridLine;
      g.fillRect(0, Math.round(yOf(v)), W, 1);
    }
    // envelope
    const color = '#8f6bff';
    g.beginPath();
    g.moveTo(xOf(off), yOf(valueAt(a.points, off)));
    for (const p of a.points) if (p.t > off && p.t < off + len) g.lineTo(xOf(p.t), yOf(p.v));
    g.lineTo(xOf(off + len), yOf(valueAt(a.points, off + len)));
    g.lineTo(xOf(off + len), H - PAD);
    g.lineTo(xOf(off), H - PAD);
    g.closePath();
    g.fillStyle = 'rgba(143,107,255,0.22)';
    g.fill();
    g.beginPath();
    g.moveTo(xOf(off), yOf(valueAt(a.points, off)));
    for (const p of a.points) if (p.t > off && p.t < off + len) g.lineTo(xOf(p.t), yOf(p.v));
    g.lineTo(xOf(off + len), yOf(valueAt(a.points, off + len)));
    g.strokeStyle = color;
    g.lineWidth = 2;
    g.stroke();
    a.points.forEach((p, i) => {
      if (p.t < off - 1 || p.t > off + len + 1) return;
      g.beginPath();
      g.arc(xOf(p.t), yOf(p.v), i === selected ? 6 : 4.5, 0, Math.PI * 2);
      g.fillStyle = i === selected ? '#fff' : color;
      g.fill();
      g.strokeStyle = '#0b0d10';
      g.lineWidth = 1.5;
      g.stroke();
    });
  }, [a, clip, len, off, project.timeSig, selected]);

  if (!clip || !a) return null;

  const fromEvent = (e: { clientX: number; clientY: number }) => {
    const r = ref.current!.getBoundingClientRect();
    const x = ((e.clientX - r.left) / r.width) * W;
    const y = ((e.clientY - r.top) / r.height) * H;
    let t = off + ((x - PAD) / (W - PAD * 2)) * len;
    if (snap) t = Math.round(t / TICKS_PER_STEP) * TICKS_PER_STEP;
    t = Math.max(off, Math.min(off + len, t));
    const v = Math.max(0, Math.min(1, 1 - (y - PAD) / (H - PAD * 2)));
    return { t, v, x, y };
  };
  const hit = (x: number, y: number) => {
    let best = -1;
    let bd = 9;
    a.points.forEach((p, i) => {
      const px = PAD + ((p.t - off) / len) * (W - PAD * 2);
      const py = PAD + (1 - p.v) * (H - PAD * 2);
      const d = Math.hypot(px - x, py - y);
      if (d < bd) { bd = d; best = i; }
    });
    return best;
  };

  const onDown = (e: RPE<HTMLCanvasElement>) => {
    const { t, v, x, y } = fromEvent(e);
    const i = hit(x, y);
    if (e.button === 2) {
      e.preventDefault();
      if (i >= 0 && a.points.length > 1) {
        setPoints(clip.id, a.points.filter((_, k) => k !== i), 'Delete automation point');
        setSelected(null);
      }
      return;
    }
    (e.currentTarget as Element).setPointerCapture(e.pointerId);
    beginGesture();
    if (i >= 0) {
      drag.current = { index: i };
      setSelected(i);
      return;
    }
    const pts = [...a.points, { t, v }].sort((p, q) => p.t - q.t);
    const idx = pts.findIndex((p) => p.t === t && p.v === v);
    setPoints(clip.id, pts, 'Add automation point');
    drag.current = { index: idx };
    setSelected(idx);
  };
  const onMove = (e: RPE<HTMLCanvasElement>) => {
    const pos = fromEvent(e);
    setHover({ t: pos.t, v: pos.v });
    const d = drag.current;
    if (!d) return;
    const cur = getProject().clips.find((c) => c.id === clipId)?.automation?.points;
    if (!cur || !cur[d.index]) return;
    // a point can be dragged between its neighbours
    const lo = d.index > 0 ? cur[d.index - 1].t : off;
    const hi = d.index < cur.length - 1 ? cur[d.index + 1].t : off + len;
    const next = cur.map((p, k) => (k === d.index ? { t: Math.max(lo, Math.min(hi, pos.t)), v: pos.v } : p));
    update('Move automation point', (dr) => {
      const c = dr.clips.find((x) => x.id === clipId);
      if (c?.automation) c.automation.points = next;
    });
  };
  const onUp = () => {
    drag.current = null;
    endGesture();
  };

  const applyShape = (shape: Parameters<typeof shapePoints>[0]) => {
    const pts = shapePoints(shape, len, cycle, currentNorm(getProject(), a.target)).map((p) => ({ ...p, t: p.t + off }));
    setPoints(clip.id, pts, 'Automation shape');
    setSelected(null);
  };
  const hoverText = hover && info ? `${info.format(info.toValue(hover.v))} at bar ${(hover.t / ticksPerBar(project.timeSig) + 1 + clip.start / ticksPerBar(project.timeSig) - off / ticksPerBar(project.timeSig)).toFixed(2)}` : '';

  return (
    <Window w={w} width={W + 28} color="#8f6bff" title={<><Icon name="automation" size={12} /> <b>Automation</b> <span className="crumb">· {info?.label ?? 'missing target'}</span></>}>
      <div className="auto">
        {!info && <div className="crash inline">The automated parameter does not exist any more (deleted effect or channel). This clip does nothing.</div>}
        <div className="auto-tools">
          <span className="label">Shape</span>
          <button className="btn small" onClick={() => applyShape('flat')}>Flat</button>
          <button className="btn small" onClick={() => applyShape('rampUp')}>Ramp up</button>
          <button className="btn small" onClick={() => applyShape('rampDown')}>Ramp down</button>
          <button className="btn small" onClick={() => applyShape('sine')}>Sine</button>
          <button className="btn small" onClick={() => applyShape('saw')}>Saw</button>
          <button className="btn small" onClick={() => applyShape('square')}>Square</button>
          <button className="btn small" onClick={() => applyShape('random')}>Random</button>
          <select className="input small-select" value={cycle} onChange={(e) => setCycle(Number(e.target.value))} title="Length of one cycle (sine / saw / square / random)">
            <option value={PPQ}>1 beat</option>
            <option value={PPQ * 2}>2 beats</option>
            <option value={PPQ * 4}>1 bar</option>
            <option value={PPQ * 8}>2 bars</option>
            <option value={PPQ * 16}>4 bars</option>
          </select>
          <span className="spacer" />
          <button className={`btn small ${snap ? 'on' : ''}`} onClick={() => setSnap(!snap)} title="Snap points to 1/16">Snap</button>
          <button className="btn small" onClick={() => setPoints(clip.id, a.points.map((p) => ({ ...p, v: 1 - p.v })), 'Flip automation')}>Flip</button>
        </div>
        <canvas
          ref={ref}
          className="auto-canvas"
          style={{ width: W, height: H }}
          onPointerDown={onDown}
          onPointerMove={onMove}
          onPointerUp={onUp}
          onPointerCancel={onUp}
          onPointerLeave={() => setHover(null)}
          onDoubleClick={(e) => {
            const { x, y } = fromEvent(e);
            const i = hit(x, y);
            if (i >= 0 && a.points.length > 1) setPoints(clip.id, a.points.filter((_, k) => k !== i), 'Delete automation point');
          }}
          onContextMenu={(e) => e.preventDefault()}
        />
        <div className="auto-foot">
          <span>{hoverText || 'Click to add a point · drag to move · right-click to delete'}</span>
          <span className="spacer" />
          <span>{a.points.length} points · {(len / ticksPerBar(project.timeSig)).toFixed(1)} bars</span>
        </div>
      </div>
    </Window>
  );
}

/** Context menu helper: is this clip an automation clip with a valid target? */
export function automationLabel(clip: Clip) {
  return clip.automation ? targetInfo(getProject(), clip.automation.target)?.label ?? 'Automation' : '';
}
