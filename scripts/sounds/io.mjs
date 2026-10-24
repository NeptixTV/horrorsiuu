// Audio file input/output for the sound build scripts (uses the ffmpeg CLI).
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

export const SR = 48000; // Opus works at 48 kHz – everything is rendered/resampled to it

/** Decodes any audio file to stereo float32 at 48 kHz. */
export function readAudio(file) {
  const r = spawnSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-i', file, '-f', 'f32le', '-ac', '2', '-ar', String(SR), '-'], { maxBuffer: 1 << 30 });
  if (r.status !== 0) throw new Error(`ffmpeg could not read ${file}: ${r.stderr}`);
  const all = new Float32Array(r.stdout.buffer, r.stdout.byteOffset, r.stdout.byteLength / 4);
  const n = all.length / 2;
  const L = new Float32Array(n);
  const R = new Float32Array(n);
  for (let i = 0; i < n; i++) {
    L[i] = all[2 * i];
    R[i] = all[2 * i + 1];
  }
  return [L, R];
}

function isMono(ch) {
  if (ch.length < 2) return true;
  const [a, b] = ch;
  for (let i = 0; i < a.length; i += 7) if (Math.abs(a[i] - b[i]) > 1e-4) return false;
  return true;
}

/**
 * Encodes to Ogg Opus. Opus keeps the exact start of the sound (it stores the
 * encoder delay), which matters for drums. Identical channels are stored mono.
 */
export function writeOpus(file, ch, { bitrate } = {}) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  const mono = isMono(ch);
  const chans = mono ? [ch[0]] : ch;
  const n = chans[0].length;
  const inter = new Float32Array(n * chans.length);
  for (let i = 0; i < n; i++) for (let c = 0; c < chans.length; c++) inter[i * chans.length + c] = chans[c][i];
  const kbps = bitrate ?? (mono ? 64 : 96);
  const r = spawnSync('ffmpeg', [
    '-hide_banner', '-loglevel', 'error', '-y',
    '-f', 'f32le', '-ar', String(SR), '-ac', String(chans.length), '-i', '-',
    '-c:a', 'libopus', '-b:a', `${kbps}k`, '-application', 'audio', '-map_metadata', '-1', '-fflags', '+bitexact', file,
  ], { input: Buffer.from(inter.buffer), maxBuffer: 1 << 30 });
  if (r.status !== 0) throw new Error(`ffmpeg could not write ${file}: ${r.stderr}`);
  return { mono, bytes: fs.statSync(file).size };
}
