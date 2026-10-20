import { describe, expect, it } from 'vitest';
import { readMidi, rescale, writeMidi } from '../midi/smf';

describe('MIDI files', () => {
  const song = {
    ppq: 96, bpm: 128, timeSig: [3, 4] as [number, number],
    tracks: [
      { name: 'Keys', channel: 0, program: 4, notes: [{ pitch: 60, start: 0, length: 96, velocity: 0.8 }, { pitch: 64, start: 0, length: 96, velocity: 0.6 }, { pitch: 60, start: 96, length: 48, velocity: 1 }] },
      { name: 'Drums', channel: 9, notes: [{ pitch: 36, start: 0, length: 24, velocity: 1 }, { pitch: 38, start: 96, length: 24, velocity: 0.7 }] },
    ],
  };

  it('round-trips notes, tempo, time signature, names and programs', () => {
    const back = readMidi(writeMidi(song));
    expect(back.ppq).toBe(96);
    expect(back.bpm).toBeCloseTo(128, 1);
    expect(back.timeSig).toEqual([3, 4]);
    expect(back.tracks.map((t) => [t.name, t.channel, t.notes.length])).toEqual([['Keys', 0, 3], ['Drums', 9, 2]]);
    expect(back.tracks[0].program).toBe(4);
    const keys = back.tracks[0].notes;
    expect(keys.map((n) => [n.pitch, n.start, n.length])).toEqual([[60, 0, 96], [64, 0, 96], [60, 96, 48]]);
    expect(keys[1].velocity).toBeCloseTo(0.6, 1);
  });

  it('keeps repeated notes on the same pitch apart', () => {
    const s = { ...song, tracks: [{ name: 'R', channel: 0, notes: [{ pitch: 60, start: 0, length: 24, velocity: 1 }, { pitch: 60, start: 24, length: 24, velocity: 1 }] }] };
    const back = readMidi(writeMidi(s));
    expect(back.tracks[0].notes.map((n) => [n.start, n.length])).toEqual([[0, 24], [24, 24]]);
  });

  it('rescales between resolutions', () => {
    const r = rescale(readMidi(writeMidi(song)), 480);
    expect(r.tracks[0].notes[2].start).toBe(480);
    expect(r.tracks[0].notes[2].length).toBe(240);
  });

  it('rejects garbage and truncated files instead of crashing', () => {
    expect(() => readMidi(new Uint8Array([1, 2, 3, 4, 5]))).toThrow();
    const good = writeMidi(song);
    expect(() => readMidi(good.slice(0, 30))).not.toThrow(); // partial tracks are read as far as possible or reported
    const bad = good.slice();
    bad[12] = 0x80; // SMPTE division
    expect(() => readMidi(bad)).toThrow(/SMPTE/);
  });
});

import { crc32, makeZip } from '../project/zip';


describe('export helpers', () => {
  it('crc32 matches the reference value', () => {
    expect(crc32(new TextEncoder().encode('123456789'))).toBe(0xcbf43926);
  });

  it('writes a structurally valid zip (local + central headers + end record)', async () => {
    const blob = makeZip([{ name: 'a.txt', data: new TextEncoder().encode('hello') }, { name: 'b.txt', data: new Uint8Array([1, 2, 3]) }]);
    const b = new Uint8Array(await blob.arrayBuffer());
    const dv = new DataView(b.buffer);
    expect(dv.getUint32(0, true)).toBe(0x04034b50);
    const eocd = b.length - 22;
    expect(dv.getUint32(eocd, true)).toBe(0x06054b50);
    expect(dv.getUint16(eocd + 10, true)).toBe(2);
    const cdOffset = dv.getUint32(eocd + 16, true);
    expect(dv.getUint32(cdOffset, true)).toBe(0x02014b50);
  });
});
