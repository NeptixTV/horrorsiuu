// One shared requestAnimationFrame loop for all meters, playheads and
// visualisers (instead of dozens of independent loops).
type Cb = (dt: number, now: number) => void;
const cbs = new Set<Cb>();
let running = false;
let last = 0;
let busy = 0;
let frameLoad = 0;

function tick(now: number) {
  const dt = last ? Math.min(0.1, (now - last) / 1000) : 0.016;
  last = now;
  const t0 = performance.now();
  for (const cb of cbs) {
    try { cb(dt, now); } catch (e) { console.error(e); }
  }
  busy = performance.now() - t0;
  frameLoad = frameLoad * 0.9 + (busy / Math.max(1, dt * 1000)) * 0.1;
  if (cbs.size) requestAnimationFrame(tick);
  else running = false;
}

export function onFrame(cb: Cb): () => void {
  cbs.add(cb);
  if (!running) {
    running = true;
    last = 0;
    requestAnimationFrame(tick);
  }
  return () => { cbs.delete(cb); };
}

/** Fraction (0..1) of the frame budget spent in UI frame callbacks. */
export function uiLoad() {
  return frameLoad;
}
