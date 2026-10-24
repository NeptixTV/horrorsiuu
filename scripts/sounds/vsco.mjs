// Real instruments from "VS Chamber Orchestra: Community Edition" (VSCO-2-CE,
// CC0 1.0 – https://github.com/sgossner/VSCO-2-CE). The key/velocity mapping
// is read from the library's own SFZ files (branch "SFZ").
//
// Each sample is gained like in the SFZ, its silence before the attack is
// removed, long decays are shortened with a fade, sustained instruments get a
// seamless loop, and the result is stored as Opus.
import fs from 'node:fs';
import path from 'node:path';
import { readAudio, writeOpus, SR } from './io.mjs';
import { biquad, db, fade, gain, makeLoop, onset, peak, rmsEnvelope, slice, tailEnd } from './dsp.mjs';

/** Minimal SFZ reader: regions with the opcodes of their group/global inherited. */
export function parseSfz(text) {
  const regions = [];
  let control = {};
  let global = {};
  let group = {};
  let cur = null;
  let section = '';
  const flush = () => {
    if (cur) regions.push({ ...global, ...group, ...cur, default_path: control.default_path ?? '' });
    cur = null;
  };
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.replace(/\/\/.*$/, '').trim();
    if (!line) continue;
    const head = /^<(\w+)>/.exec(line);
    if (head) {
      flush();
      section = head[1];
      if (section === 'group') group = {};
      if (section === 'region') cur = {};
    }
    const rest = head ? line.slice(head[0].length) : line;
    const re = /(\w+)=(.*?)(?=\s+\w+=|$)/g;
    let m;
    while ((m = re.exec(rest))) {
      const [k, v] = [m[1], m[2].trim()];
      if (section === 'control') control[k] = v;
      else if (section === 'global') global[k] = v;
      else if (section === 'group') group[k] = v;
      else if (section === 'region' && cur) cur[k] = v;
    }
  }
  flush();
  return regions;
}

export const VSCO_INSTRUMENTS = [
  { id: 'keys.upright', sfz: 'UprightPiano.sfz', maxLen: (k) => (k < 45 ? 9 : k < 60 ? 7.5 : k < 76 ? 5.5 : 3.5) },
  { id: 'keys.organ', sfz: 'OrganLoud.sfz', loop: true },
  { id: 'mallet.glock', sfz: 'Glockenspiel.sfz', maxLen: () => 4 },
  { id: 'mallet.marimba', sfz: 'Marimba.sfz', maxLen: () => 3 },
  { id: 'strings.harp', sfz: 'Harp.sfz', maxLen: (k) => (k < 48 ? 6 : 4.5) },
  { id: 'strings.violins.pizz', sfz: 'ViolinEnsPizz.sfz', maxLen: () => 1.6 },
  { id: 'strings.violins.spic', sfz: 'ViolinEnsSpic.sfz', maxLen: () => 1.5 },
  { id: 'strings.violins.sus', sfz: 'ViolinEnsSusVib.sfz', loop: true },
  { id: 'strings.cellos.pizz', sfz: 'CelloEnsPizz.sfz', maxLen: () => 2.2 },
  { id: 'strings.cellos.sus', sfz: 'CelloEnsSusVib.sfz', loop: true },
  { id: 'winds.flute.stac', sfz: 'FluteStac.sfz', maxLen: () => 1.2 },
  { id: 'winds.flute.sus', sfz: 'FluteSusVib.sfz', loop: true },
  { id: 'brass.trumpet.stac', sfz: 'TrumpetStac.sfz', maxLen: () => 1.2 },
  { id: 'brass.trumpet.sus', sfz: 'TrumpetSusVib.sfz', loop: true },
  { id: 'brass.horn.sus', sfz: 'FHornSus.sfz', loop: true },
];

const int = (v, d) => (v === undefined ? d : Math.round(Number(v)));

/**
 * Perceived level of a note: RMS of its loudest 400 ms with the deep lows
 * filtered out (bass notes carry lots of energy that the ear barely hears).
 * The loudest window works for a hammer attack as well as for a slow string
 * swell.
 */
export function loudness(ch) {
  const n = ch[0].length;
  const w = Math.min(n, Math.round(SR * 0.4));
  const hop = Math.round(SR * 0.05);
  const sq = new Float64Array(n);
  for (const c of ch) {
    const x = biquad(c.slice(), 'hp', 150, 0.7);
    for (let i = 0; i < n; i++) sq[i] += x[i] * x[i];
  }
  let best = 0;
  for (let a = 0; a + w <= n; a += hop) {
    let s = 0;
    for (let i = a; i < a + w; i++) s += sq[i];
    if (s > best) best = s;
  }
  return Math.sqrt(best / (w * ch.length));
}

/**
 * Target level per velocity layer (dBFS RMS): the loudest layer of every
 * note sounds equally loud across the keyboard, softer layers sit 5 / 10 dB
 * below – the timbre change does the rest, like on a real instrument.
 */
function layerTarget(rank, layers) {
  const fromTop = layers - 1 - rank;
  return -18 - fromTop * (layers > 2 ? 5 : 6);
}

/** Flattens the level between a and b so a loop does not "pump". */
function flatten(ch, a, b) {
  const win = Math.round(SR * 0.35);
  const env = rmsEnvelope(slice(ch, a, b), win);
  const target = env[0] || 1e-6;
  for (let i = a; i < b; i++) {
    const w = (i - a) / win;
    const k = Math.min(env.length - 1, Math.floor(w));
    const k2 = Math.min(env.length - 1, k + 1);
    const e = env[k] + (env[k2] - env[k]) * (w - k);
    const g = Math.min(4, target / Math.max(e, 1e-6));
    for (const c of ch) c[i] *= g;
  }
}

function processRegion(cfg, r, srcRoot) {
  const dir = r.default_path.replace(/\\/g, '/');
  const file = path.join(srcRoot, dir, r.sample.replace(/\\/g, '/'));
  let ch = readAudio(file);
  gain(ch, db(Number(r.volume ?? 0)));
  const key = int(r.pitch_keycenter, 60);
  // start right at the attack (a hair before it)
  const start = Math.max(0, onset(ch, 0.003) - Math.round(SR * 0.0015));
  let end = tailEnd(ch, -60);
  ch = slice(ch, start, end);
  const out = { key, ch, loop: null };
  if (cfg.loop) {
    const n = ch[0].length;
    const ls = Math.round(Math.max(SR * 0.45, n * 0.22));
    // a ~2 s loop is plenty for a steady sustain and keeps the files small
    let le = Math.round(Math.min(n * 0.78, ls + SR * 2.2));
    if (le - ls < SR * 0.8) le = Math.min(n - 1, ls + Math.round(SR * 0.8));
    const xf = Math.round(Math.min(SR * 0.3, (le - ls) / 3));
    flatten(ch, ls - xf, le + Math.round(SR * 0.02));
    const loop = makeLoop(ch, ls, le, xf);
    ch = slice(ch, 0, loop.end + Math.round(SR * 0.01));
    out.ch = ch;
    out.loop = { start: loop.start / SR, end: loop.end / SR, score: loop.score };
  } else {
    const max = Math.round(SR * cfg.maxLen(key));
    if (ch[0].length > max) {
      ch = slice(ch, 0, max);
      fade(ch, 0, Math.min(0.6, cfg.maxLen(key) * 0.25));
    } else {
      fade(ch, 0, 0.05);
    }
    out.ch = ch;
  }
  fade(out.ch, 0.0005, 0);
  return out;
}

export function buildVsco({ srcRoot, sfzDir, outDir, only }) {
  const result = {};
  for (const cfg of VSCO_INSTRUMENTS) {
    if (only && !only.includes(cfg.id)) continue;
    const regions = parseSfz(fs.readFileSync(path.join(sfzDir, cfg.sfz), 'utf8'));
    // round robin: groups with seq_position / lorand
    const processed = regions.map((r) => ({ r, p: processRegion(cfg, r, srcRoot) }));
    // even level across the keyboard: every velocity layer of every note gets
    // its target loudness. A sample whose transient would clip is stored
    // quieter and the difference is restored at playback (zone gain "g").
    const layersOf = new Map();
    for (const r of regions) {
      const k = int(r.pitch_keycenter, 60);
      if (!layersOf.has(k)) layersOf.set(k, new Set());
      layersOf.get(k).add(int(r.hivel, 127));
    }
    for (const { r, p } of processed) {
      const layers = [...layersOf.get(p.key)].sort((a, b) => a - b);
      const rank = layers.indexOf(int(r.hivel, 127));
      gain(p.ch, db(layerTarget(rank, layers.length)) / Math.max(1e-6, loudness(p.ch)));
      // very spiky notes (top of the piano, glockenspiel) would peak far above
      // full scale at that loudness – they give up a little level instead
      const pk = peak(p.ch);
      if (pk > db(2)) gain(p.ch, db(2) / pk);
    }
    const zones = [];
    let bytes = 0;
    const folder = cfg.id.replace(/\./g, '/');
    processed.forEach(({ r, p }, i) => {
      const pk = peak(p.ch);
      const comp = pk > db(-1) ? pk / db(-1) : 1;
      if (comp > 1) gain(p.ch, 1 / comp);
      const rr = r.seq_position ? int(r.seq_position, 1) - 1 : r.lorand !== undefined ? (Number(r.lorand) > 0.25 ? 1 : 0) : 0;
      const name = `${String(p.key).padStart(3, '0')}-v${int(r.hivel, 127)}${rr ? `-rr${rr + 1}` : ''}-${i}.ogg`;
      const rel = `${folder}/${name}`;
      const w = writeOpus(path.join(outDir, rel), p.ch, cfg.loop ? { bitrate: 80 } : {});
      bytes += w.bytes;
      const z = { f: rel, r: p.key, lo: int(r.lokey, p.key), hi: int(r.hikey, p.key), vl: int(r.lovel, 0), vh: int(r.hivel, 127) };
      if (rr) z.rr = rr;
      if (comp > 1) z.g = +comp.toFixed(4);
      if (p.loop) { z.ls = +p.loop.start.toFixed(5); z.le = +p.loop.end.toFixed(5); }
      zones.push(z);
    });
    zones.sort((a, b) => a.r - b.r || a.vl - b.vl || (a.rr ?? 0) - (b.rr ?? 0));
    result[cfg.id] = { zones, loop: !!cfg.loop, bytes };
    console.log(`${cfg.id}: ${zones.length} zones, ${(bytes / 1024 / 1024).toFixed(2)} MB`);
  }
  return result;
}
