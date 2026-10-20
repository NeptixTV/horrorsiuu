// MIDI files ⇄ project: export a pattern or the whole song as .mid, import a
// .mid file as new instruments + patterns (drums on drum channels, melodic
// tracks on fitting synth / DreamChord sounds).
import type { Channel, DrumType, Note, Pattern, Project } from '../state/types';
import { MASTER_ID, PPQ, TICKS_PER_STEP } from '../state/types';
import { getProject, update } from '../state/store';
import { setUI, toast } from '../state/ui';
import { defaultDream, defaultDrum, makeChannel, makePattern, makeTrack, SYNTH_PRESETS } from '../state/defaults';
import { applyDreamPreset, findDreamPreset } from '../audio/instruments/dreamPresets';
import { collectNoteEvents, collectPatternEvents, patternLength, songLength, type NoteEvent } from '../audio/sequencer';
import { paletteColor, ticksPerBar, uid } from '../state/utils';
import { downloadBlob } from '../audio/render';
import { readMidi, rescale, writeMidi, type SmfTrack } from './smf';

const GM_DRUM: Record<DrumType, number> = {
  kick: 36, snare: 38, clap: 39, hihat: 42, openhat: 46, tom: 45, perc: 62, rim: 37, cowbell: 56, shaker: 70, crash: 49,
};

/** GM drum note → our drum sound (+ pitch for tuned toms). */
function drumFor(note: number): { type: DrumType; pitch: number } {
  if (note === 35 || note === 36) return { type: 'kick', pitch: 60 };
  if (note === 37) return { type: 'rim', pitch: 60 };
  if (note === 38 || note === 40) return { type: 'snare', pitch: 60 };
  if (note === 39) return { type: 'clap', pitch: 60 };
  if (note === 42 || note === 44) return { type: 'hihat', pitch: 60 };
  if (note === 46 || note === 51 || note === 53 || note === 59) return { type: 'openhat', pitch: 60 };
  const toms: Record<number, number> = { 41: 55, 43: 57, 45: 60, 47: 62, 48: 64, 50: 67 };
  if (toms[note]) return { type: 'tom', pitch: toms[note] };
  if (note === 49 || note === 52 || note === 55 || note === 57) return { type: 'crash', pitch: 60 };
  if (note === 54 || note === 69 || note === 70 || note === 82) return { type: 'shaker', pitch: 60 };
  if (note === 56) return { type: 'cowbell', pitch: 60 };
  return { type: 'perc', pitch: 60 + Math.max(-12, Math.min(12, note - 62)) };
}

function toSmfTracks(project: Project, events: NoteEvent[]): SmfTrack[] {
  const byChannel = new Map<string, NoteEvent[]>();
  for (const e of events) (byChannel.get(e.channelId) ?? byChannel.set(e.channelId, []).get(e.channelId)!).push(e);
  const tracks: SmfTrack[] = [];
  // MIDI channel 10 (index 9) is reserved for drums
  const MELODIC = [0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15];
  let melodic = 0;
  for (const ch of project.channels) {
    const list = byChannel.get(ch.id);
    if (!list?.length) continue;
    const drum = ch.kind === 'drum' && ch.drum;
    tracks.push({
      name: ch.name,
      channel: drum ? 9 : MELODIC[melodic++ % MELODIC.length],
      notes: list.map((e) => ({
        pitch: drum ? GM_DRUM[ch.drum!.type] : e.pitch,
        start: e.tick,
        length: Math.max(1, e.length),
        velocity: e.velocity,
      })),
    });
  }
  return tracks;
}

function save(bytes: Uint8Array, name: string) {
  downloadBlob(new Blob([bytes as BlobPart], { type: 'audio/midi' }), `${name.replace(/[^\w\- ]+/g, '_').slice(0, 80) || 'goofy'}.mid`);
}

export function exportPatternMidi(patternId: string) {
  const p = getProject();
  const pat = p.patterns.find((x) => x.id === patternId);
  if (!pat) return;
  const out: NoteEvent[] = [];
  collectPatternEvents(p, pat, 0, patternLength(pat, p.timeSig), 0, out);
  const tracks = toSmfTracks(p, out);
  if (!tracks.length) {
    toast('This pattern has no notes', 'info');
    return;
  }
  save(writeMidi({ ppq: PPQ, bpm: p.bpm, timeSig: p.timeSig, tracks }), `${p.name} - ${pat.name}`);
  toast(`Exported “${pat.name}” as MIDI`, 'ok');
}

export function exportSongMidi() {
  const p = getProject();
  const tracks = toSmfTracks(p, collectNoteEvents(p, 'song', null, 0, songLength(p)));
  if (!tracks.length) {
    toast('The song has no notes in the playlist yet', 'info');
    return;
  }
  save(writeMidi({ ppq: PPQ, bpm: p.bpm, timeSig: p.timeSig, tracks }), p.name);
  toast('Exported the song as MIDI', 'ok');
}

/** Sound for an imported melodic track (by General MIDI program or pitch range). */
function soundFor(track: SmfTrack): Pick<Channel, 'kind' | 'synth' | 'dream'> {
  const dream = (name: string) => {
    const pr = findDreamPreset(name);
    return { kind: 'dream' as const, dream: pr ? applyDreamPreset(defaultDream(), pr) : defaultDream() };
  };
  const synth = (name: string) => ({ kind: 'synth' as const, synth: structuredClone((SYNTH_PRESETS.find((s) => s.name === name) ?? SYNTH_PRESETS[0]).params) });
  const prog = track.program;
  if (prog === undefined) {
    const avg = track.notes.reduce((n, x) => n + x.pitch, 0) / Math.max(1, track.notes.length);
    return avg < 48 ? synth('Fat Bass') : dream('Ocean Dream');
  }
  if (prog < 8) return dream('Dreamy Piano');
  if (prog < 16) return dream('Glass Garden');
  if (prog < 24) return dream('Velvet Keys');
  if (prog < 32) return synth('Glass Pluck');
  if (prog < 40) return synth('Fat Bass');
  if (prog < 56) return dream('Cloud Strings');
  if (prog < 64) return synth('Supersaw Lead');
  if (prog < 88) return synth('Square Lead');
  if (prog < 96) return dream('Ocean Dream');
  if (prog < 104) return dream('Space Ambient');
  return synth('Init Saw');
}

const MAX_BARS_PER_PATTERN = 64;
const MAX_SONG_BARS = 1000;
const MAX_TRACKS = 48;
const MAX_NOTES = 200_000;
const CHUNK_BARS = 8;

/** Imports a .mid file. Returns the number of notes imported. */
export async function importMidiFile(file: File): Promise<number> {
  if (file.size > 16 * 1024 * 1024) throw new Error('The MIDI file is too large');
  const song = rescale(readMidi(await file.arrayBuffer()), PPQ);
  const base = file.name.replace(/\.(mid|midi|kar)$/i, '').slice(0, 40) || 'MIDI';
  const project = getProject();
  const adoptTempo = project.clips.length === 0;
  const bar = ticksPerBar(adoptTempo ? song.timeSig : project.timeSig);
  // a damaged or hostile file must not freeze the studio: limit length, tracks and notes
  const limit = MAX_SONG_BARS * bar;
  let dropped = 0;
  let tracks = song.tracks
    .map((t) => {
      const notes = t.notes.filter((n) => n.start >= 0 && n.start < limit);
      dropped += t.notes.length - notes.length;
      return { ...t, notes: notes.map((n) => ({ ...n, length: Math.min(n.length, limit - n.start) })) };
    })
    .filter((t) => t.notes.length);
  if (!tracks.length) throw new Error('The MIDI file contains no notes');
  if (tracks.length > MAX_TRACKS) {
    tracks = [...tracks].sort((a, b) => b.notes.length - a.notes.length).slice(0, MAX_TRACKS);
    toast(`Only the ${MAX_TRACKS} busiest MIDI tracks were imported`, 'info', true);
  }
  if (tracks.reduce((n, t) => n + t.notes.length, 0) > MAX_NOTES) throw new Error(`The MIDI file has more than ${MAX_NOTES.toLocaleString()} notes`);
  if (dropped) toast(`${dropped} notes after bar ${MAX_SONG_BARS} were left out`, 'info', true);
  let end = 0;
  for (const t of tracks) for (const n of t.notes) end = Math.max(end, n.start + n.length);
  const totalBars = Math.max(1, Math.ceil(end / bar));
  const chunks = totalBars <= MAX_BARS_PER_PATTERN ? [{ from: 0, bars: totalBars }] : Array.from({ length: Math.ceil(totalBars / CHUNK_BARS) }, (_, i) => ({ from: i * CHUNK_BARS * bar, bars: CHUNK_BARS }));

  // channels: one per melodic track, one per drum sound
  const channels: Channel[] = [];
  const notesByChannel = new Map<string, Note[]>();
  const drumChannels = new Map<DrumType, Channel>();
  const usedMixer = new Set(project.channels.map((c) => c.mixerTrackId));
  const freeMixer = project.mixer.filter((m) => m.id !== MASTER_ID && !usedMixer.has(m.id) && m.inserts.length === 0 && m.sends.length === 0 && /^Insert \d+$/.test(m.name)).map((m) => m.id);
  const nextMixer = () => freeMixer.shift() ?? MASTER_ID;
  let color = project.channels.length;
  for (const t of tracks) {
    if (t.channel === 9) {
      for (const n of t.notes) {
        const { type, pitch } = drumFor(n.pitch);
        let ch = drumChannels.get(type) ?? project.channels.find((c) => c.kind === 'drum' && c.drum?.type === type);
        if (!ch) {
          ch = makeChannel('drum', `${type[0].toUpperCase()}${type.slice(1)}`, nextMixer(), color++, { drum: defaultDrum(type) });
          channels.push(ch);
        }
        drumChannels.set(type, ch);
        (notesByChannel.get(ch.id) ?? notesByChannel.set(ch.id, []).get(ch.id)!).push({ id: uid('n'), pitch, start: n.start, length: Math.max(TICKS_PER_STEP / 2, n.length), velocity: n.velocity });
      }
    } else {
      const ch = makeChannel('synth', t.name.slice(0, 40), nextMixer(), color++, soundFor(t));
      channels.push(ch);
      notesByChannel.set(ch.id, t.notes.map((n) => ({ id: uid('n'), pitch: n.pitch, start: n.start, length: n.length, velocity: n.velocity })));
    }
  }

  const patterns: Pattern[] = chunks.map((c, i) => {
    const pt = makePattern(project.patterns.length + i + 1, (c.bars * bar) / TICKS_PER_STEP);
    pt.name = chunks.length > 1 ? `${base} ${i + 1}` : base;
    pt.color = paletteColor(project.patterns.length * 3 + i);
    for (const [chId, notes] of notesByChannel) {
      const inChunk = notes.filter((n) => n.start >= c.from && n.start < c.from + c.bars * bar).map((n) => ({ ...n, id: uid('n'), start: n.start - c.from }));
      if (inChunk.length) pt.notes[chId] = inChunk;
    }
    return pt;
  });

  const track = chunks.length > 1 ? makeTrack(project.tracks.length + 1) : null;
  if (track) track.name = base;
  const count = [...notesByChannel.values()].reduce((n, l) => n + l.length, 0);
  update(`Import MIDI “${base}”`, (d) => {
    if (adoptTempo) {
      d.bpm = Math.max(20, Math.min(400, song.bpm));
      d.timeSig = song.timeSig;
    }
    d.channels.push(...channels);
    d.patterns.push(...patterns);
    for (const ch of channels) {
      const m = d.mixer.find((x) => x.id === ch.mixerTrackId);
      if (m && m.id !== MASTER_ID) {
        m.name = ch.name;
        m.color = ch.color;
      }
    }
    if (track) {
      d.tracks.push(track);
      patterns.forEach((pt, i) => d.clips.push({ id: uid('c'), trackId: track.id, kind: 'pattern', start: chunks[i].from, length: chunks[i].bars * bar, offset: 0, patternId: pt.id }));
    }
  });
  setUI({ selectedPatternId: patterns[0].id, pianoChannelId: channels.find((c) => c.kind !== 'drum')?.id ?? channels[0]?.id ?? null });
  toast(`Imported ${count} notes from “${file.name}”${track ? ` – ${patterns.length} patterns placed in the playlist` : ' – drag the new pattern into the playlist'}${adoptTempo ? ` (${song.bpm} BPM)` : ''}`, 'ok');
  return count;
}
