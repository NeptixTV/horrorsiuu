// Standard MIDI File (SMF) reader and writer.
// Writer: format 1, one track per channel, tempo + time signature.
// Reader: formats 0 and 1, running status, meta / sysex skipping, strict
// bounds checks (a damaged or hostile file can never read out of range).

export interface SmfNote {
  pitch: number;
  start: number; // ticks
  length: number; // ticks
  velocity: number; // 0..1
}

export interface SmfTrack {
  name: string;
  channel: number; // 0..15 (9 = drums)
  notes: SmfNote[];
  program?: number;
}

export interface SmfSong {
  ppq: number;
  bpm: number;
  timeSig: [number, number];
  tracks: SmfTrack[];
}

// ---------- writer ----------

function vlq(n: number, out: number[]) {
  let v = Math.max(0, Math.round(n));
  const bytes = [v & 0x7f];
  while ((v >>= 7) > 0) bytes.unshift((v & 0x7f) | 0x80);
  out.push(...bytes);
}

function text(s: string): number[] {
  // track names as ASCII (non-ASCII replaced) – the safest for every DAW
  return Array.from(s.slice(0, 120)).map((c) => (c.charCodeAt(0) < 128 ? c.charCodeAt(0) : 0x3f));
}

function chunk(type: string, data: number[]): number[] {
  const len = data.length;
  return [...text(type), (len >>> 24) & 0xff, (len >>> 16) & 0xff, (len >>> 8) & 0xff, len & 0xff, ...data];
}

export function writeMidi(song: SmfSong): Uint8Array {
  const ppq = Math.max(24, Math.min(960, Math.round(song.ppq)));
  const conductor: number[] = [];
  vlq(0, conductor);
  const mpqn = Math.round(60000000 / Math.max(20, Math.min(400, song.bpm)));
  conductor.push(0xff, 0x51, 0x03, (mpqn >> 16) & 0xff, (mpqn >> 8) & 0xff, mpqn & 0xff);
  vlq(0, conductor);
  const den = Math.round(Math.log2(song.timeSig[1] || 4));
  conductor.push(0xff, 0x58, 0x04, song.timeSig[0] & 0xff, den & 0xff, 24, 8);
  vlq(0, conductor);
  conductor.push(0xff, 0x2f, 0x00);
  const chunks: number[][] = [chunk('MThd', [0, 1, ((song.tracks.length + 1) >> 8) & 0xff, (song.tracks.length + 1) & 0xff, (ppq >> 8) & 0xff, ppq & 0xff]), chunk('MTrk', conductor)];

  for (const tr of song.tracks) {
    const ch = Math.max(0, Math.min(15, tr.channel)) & 0x0f;
    const events: { tick: number; order: number; bytes: number[] }[] = [];
    for (const n of tr.notes) {
      const pitch = Math.max(0, Math.min(127, Math.round(n.pitch)));
      const vel = Math.max(1, Math.min(127, Math.round(n.velocity * 127)));
      const on = Math.max(0, Math.round(n.start));
      const off = Math.max(on + 1, Math.round(n.start + n.length));
      events.push({ tick: on, order: 1, bytes: [0x90 | ch, pitch, vel] });
      events.push({ tick: off, order: 0, bytes: [0x80 | ch, pitch, 0] });
    }
    // note-offs before note-ons on the same tick (repeated notes stay separate)
    events.sort((a, b) => a.tick - b.tick || a.order - b.order);
    const data: number[] = [];
    vlq(0, data);
    const name = text(tr.name);
    data.push(0xff, 0x03);
    vlq(name.length, data);
    data.push(...name);
    if (tr.program !== undefined && ch !== 9) {
      vlq(0, data);
      data.push(0xc0 | ch, tr.program & 0x7f);
    }
    let last = 0;
    for (const e of events) {
      vlq(e.tick - last, data);
      data.push(...e.bytes);
      last = e.tick;
    }
    vlq(0, data);
    data.push(0xff, 0x2f, 0x00);
    chunks.push(chunk('MTrk', data));
  }
  return Uint8Array.from(chunks.flat());
}

// ---------- reader ----------

const MAX_EVENTS = 2_000_000;

export function readMidi(buf: ArrayBuffer | Uint8Array): SmfSong {
  const b = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
  let p = 0;
  const need = (n: number) => {
    if (p + n > b.length) throw new Error('The MIDI file is damaged (unexpected end)');
  };
  const u32 = () => { need(4); const v = ((b[p] << 24) >>> 0) + (b[p + 1] << 16) + (b[p + 2] << 8) + b[p + 3]; p += 4; return v; };
  const u16 = () => { need(2); const v = (b[p] << 8) + b[p + 1]; p += 2; return v; };
  const tag = () => { need(4); const s = String.fromCharCode(b[p], b[p + 1], b[p + 2], b[p + 3]); p += 4; return s; };

  if (tag() !== 'MThd') throw new Error('Not a MIDI file');
  const hlen = u32();
  if (hlen < 6) throw new Error('The MIDI file is damaged (header)');
  const format = u16();
  const ntrks = u16();
  const division = u16();
  p += hlen - 6;
  if (format > 2) throw new Error('Unsupported MIDI format');
  if (division & 0x8000) throw new Error('SMPTE-timed MIDI files are not supported');
  const ppq = division || 96;
  let bpm = 120;
  let tempoSet = false;
  let timeSig: [number, number] = [4, 4];
  const tracks: SmfTrack[] = [];
  let events = 0;

  for (let t = 0; t < ntrks && p < b.length; t++) {
    let type: string;
    try {
      type = tag();
    } catch {
      break;
    }
    const len = u32();
    const end = Math.min(b.length, p + len);
    if (type !== 'MTrk') {
      p = end;
      continue;
    }
    let tick = 0;
    let status = 0;
    let name = '';
    const open = new Map<number, { start: number; velocity: number }[]>(); // (ch << 7 | pitch) → stack
    const byChannel = new Map<number, SmfNote[]>();
    const programs = new Map<number, number>();
    const readVlq = () => {
      let v = 0;
      for (let i = 0; i < 4; i++) {
        if (p >= end) throw new Error('The MIDI file is damaged (track)');
        const c = b[p++];
        v = (v << 7) | (c & 0x7f);
        if (!(c & 0x80)) return v;
      }
      throw new Error('The MIDI file is damaged (number)');
    };
    while (p < end) {
      if (++events > MAX_EVENTS) throw new Error('The MIDI file is too large');
      tick += readVlq();
      if (p >= end) break;
      let st = b[p];
      if (st & 0x80) {
        p++;
        if (st < 0xf0) status = st;
      } else {
        if (!status) throw new Error('The MIDI file is damaged (running status)');
        st = status;
      }
      if (st === 0xff) {
        if (p >= end) break;
        const mt = b[p++];
        const ml = readVlq();
        if (p + ml > end) throw new Error('The MIDI file is damaged (meta)');
        if (mt === 0x51 && ml === 3 && !tempoSet) {
          const mpqn = (b[p] << 16) | (b[p + 1] << 8) | b[p + 2];
          if (mpqn > 0) {
            bpm = Math.round((60000000 / mpqn) * 100) / 100;
            tempoSet = true;
          }
        } else if (mt === 0x58 && ml >= 2) {
          timeSig = [Math.max(1, Math.min(16, b[p])), Math.pow(2, Math.min(4, b[p + 1]))];
        } else if (mt === 0x03 && !name) {
          name = Array.from(b.subarray(p, p + Math.min(ml, 80))).map((c) => (c >= 32 && c < 127 ? String.fromCharCode(c) : '')).join('').trim();
        } else if (mt === 0x2f) {
          p += ml;
          break;
        }
        p += ml;
        continue;
      }
      if (st === 0xf0 || st === 0xf7) {
        const sl = readVlq();
        p = Math.min(end, p + sl);
        continue;
      }
      const cmd = st & 0xf0;
      const ch = st & 0x0f;
      const two = cmd !== 0xc0 && cmd !== 0xd0;
      need(two ? 2 : 1);
      const d1 = b[p++] & 0x7f;
      const d2 = two ? b[p++] & 0x7f : 0;
      if (cmd === 0x90 && d2 > 0) {
        const key = (ch << 7) | d1;
        (open.get(key) ?? open.set(key, []).get(key)!).push({ start: tick, velocity: d2 / 127 });
      } else if (cmd === 0x80 || (cmd === 0x90 && d2 === 0)) {
        const key = (ch << 7) | d1;
        const stack = open.get(key);
        const on = stack?.shift();
        if (on) (byChannel.get(ch) ?? byChannel.set(ch, []).get(ch)!).push({ pitch: d1, start: on.start, length: Math.max(1, tick - on.start), velocity: on.velocity });
      } else if (cmd === 0xc0) {
        programs.set(ch, d1);
      }
    }
    p = end;
    // notes that never ended get a 1/8 length
    for (const [key, stack] of open) for (const on of stack) {
      const ch = key >> 7;
      (byChannel.get(ch) ?? byChannel.set(ch, []).get(ch)!).push({ pitch: key & 0x7f, start: on.start, length: ppq / 2, velocity: on.velocity });
    }
    for (const [ch, notes] of byChannel) {
      notes.sort((a, c) => a.start - c.start || a.pitch - c.pitch);
      tracks.push({ name: name || (ch === 9 ? 'Drums' : `Track ${tracks.length + 1}`), channel: ch, notes, program: programs.get(ch) });
    }
  }
  return { ppq, bpm, timeSig, tracks };
}

/** Rescales note times of a song to another resolution. */
export function rescale(song: SmfSong, ppq: number): SmfSong {
  const f = ppq / song.ppq;
  return {
    ...song,
    ppq,
    tracks: song.tracks.map((t) => ({ ...t, notes: t.notes.map((n) => ({ ...n, start: Math.round(n.start * f), length: Math.max(1, Math.round(n.length * f)) })) })),
  };
}
