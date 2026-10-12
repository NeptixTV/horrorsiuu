// "Song Splitter": import a song, separate vocals / instrumental / beat /
// melody, preview each part and drop it into the playlist or the library.
import { useRef, useState } from 'react';
import { separateStems, stereoCorrelation, type StemName } from '../../audio/stems';
import { assets, computePeaks } from '../../audio/assets';
import { engine } from '../../audio/engine';
import { addTrack, ensureTrackInsert, updateTrack } from '../../state/actions';
import { getProject } from '../../state/store';
import { toast, type FloatingWindow } from '../../state/ui';
import { markAssetPending, placeAudioClip } from '../../project/manager';
import { uid } from '../../state/utils';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { Knob } from '../components/Knob';
import { useCanvas } from '../visualizers/Visualizers';
import { theme } from '../../styles/themes';
import { pickFile } from '../dialogs/filePick';
import { addToLibrary } from '../library/libraryStore';
import './stems.css';

const STEMS: { id: StemName; label: string; icon: string; color: string; info: string }[] = [
  { id: 'vocals', label: 'Vocals', icon: '🎤', color: '#f28c28', info: 'the lead voice (centre of the stereo image)' },
  { id: 'instrumental', label: 'Instrumental', icon: '🎹', color: '#5b8cff', info: 'everything without the vocals (karaoke)' },
  { id: 'drums', label: 'Beat / Drums', icon: '🥁', color: '#ff5c5c', info: 'percussive sounds: kick, snare, hats' },
  { id: 'melody', label: 'Melody', icon: '🎸', color: '#9be15d', info: 'music without drums and vocals' },
];

function Wave({ buffer, color }: { buffer: AudioBuffer; color: string }) {
  const peaks = useRef(computePeaks(buffer, Math.max(64, Math.floor(buffer.length / 2000))));
  const ref = useCanvas((ctx, w, h) => {
    ctx.fillStyle = '#07080a';
    ctx.fillRect(0, 0, w, h);
    const d = peaks.current.data;
    const n = d.length / 2;
    ctx.fillStyle = color;
    for (let x = 0; x < w; x++) {
      const i0 = Math.floor((x / w) * n);
      const i1 = Math.max(i0 + 1, Math.floor(((x + 1) / w) * n));
      let mn = 0;
      let mx = 0;
      for (let i = i0; i < i1; i++) { mn = Math.min(mn, d[i * 2]); mx = Math.max(mx, d[i * 2 + 1]); }
      ctx.fillRect(x, h / 2 - mx * (h / 2 - 2), 1, Math.max(1, (mx - mn) * (h / 2 - 2)));
    }
    void theme;
  }, [buffer]);
  return <canvas ref={ref} className="st-wave" />;
}

interface Part {
  stem: StemName | 'original';
  buffer: AudioBuffer;
}

export function StemSplitterWindow({ w }: { w: FloatingWindow }) {
  const [song, setSong] = useState<{ name: string; buffer: AudioBuffer } | null>(null);
  const [want, setWant] = useState<Set<StemName>>(new Set(['vocals', 'instrumental', 'drums']));
  const [focus, setFocus] = useState(0.6);
  const [progress, setProgress] = useState<number | null>(null);
  const [parts, setParts] = useState<Part[]>([]);
  const [playing, setPlaying] = useState<string | null>(null);
  const [drop, setDrop] = useState(false);
  const src = useRef<AudioBufferSourceNode | null>(null);

  const load = async (file: File) => {
    try {
      const buffer = await engine.ctx.decodeAudioData(await file.arrayBuffer());
      setSong({ name: file.name.replace(/\.[^.]+$/, ''), buffer });
      setParts([]);
    } catch {
      toast(`Could not read “${file.name}”`, 'error');
    }
  };

  const stopPreview = () => {
    try { src.current?.stop(); } catch { /* ignore */ }
    src.current = null;
    setPlaying(null);
  };
  const preview = (key: string, b: AudioBuffer) => {
    stopPreview();
    void engine.resume();
    const s = engine.ctx.createBufferSource();
    s.buffer = b;
    s.connect(engine.masterOut);
    s.start();
    s.onended = () => { if (src.current === s) { src.current = null; setPlaying(null); } };
    src.current = s;
    setPlaying(key);
  };

  const split = async () => {
    if (!song) return;
    stopPreview();
    setProgress(0);
    try {
      const res = await separateStems(song.buffer, [...want], focus, setProgress);
      const out: Part[] = [];
      for (const s of STEMS) {
        const r = res[s.id];
        if (!r) continue;
        const b = new AudioBuffer({ length: r[0].length, numberOfChannels: 2, sampleRate: song.buffer.sampleRate });
        b.copyToChannel(new Float32Array(r[0]), 0);
        b.copyToChannel(new Float32Array(r[1]), 1);
        out.push({ stem: s.id, buffer: b });
      }
      setParts(out);
      toast('Song split – listen to the parts below', 'ok');
    } catch (err) {
      toast(`Splitting failed: ${err instanceof Error ? err.message : err}`, 'error');
    } finally {
      setProgress(null);
    }
  };

  const labelOf = (p: Part) => (p.stem === 'original' ? 'Original' : STEMS.find((s) => s.id === p.stem)!.label);

  const toPlaylist = (list: Part[]) => {
    if (!song) return;
    const start = 0;
    for (const p of list) {
      const id = uid('stem');
      const name = `${song.name} – ${labelOf(p)}`;
      assets.add(id, p.buffer, name);
      markAssetPending(id);
      const trackId = addTrack();
      const color = p.stem === 'original' ? '#3fa7a0' : STEMS.find((s) => s.id === p.stem)!.color;
      updateTrack(trackId, { name: `${labelOf(p)} (${song.name})`.slice(0, 40), color }, 'Song part track');
      ensureTrackInsert(trackId);
      placeAudioClip(id, trackId, start);
    }
    const bpmHint = getProject().bpm;
    toast(`${list.length} part${list.length > 1 ? 's' : ''} added at bar 1 (project tempo ${bpmHint} BPM) – cut them with the scissors tool`, 'ok');
  };

  const toLibrary = async (p: Part) => {
    if (!song) return;
    await addToLibrary(p.buffer, `${song.name} – ${labelOf(p)}`);
    toast('Saved to My Library', 'ok');
  };

  const mono = song ? song.buffer.numberOfChannels < 2 || stereoCorrelation(song.buffer.getChannelData(0), song.buffer.getChannelData(1)) > 0.995 : false;

  return (
    <Window w={w} width={640} color="#d46bff" title={<><b>Song Splitter</b> <span className="crumb">· remove vocals or the beat</span></>}>
      <div className="st-body" onDragOver={(e) => { e.preventDefault(); setDrop(true); }} onDragLeave={() => setDrop(false)} onDrop={(e) => { e.preventDefault(); setDrop(false); const f = e.dataTransfer.files?.[0]; if (f) void load(f); }}>
        {!song ? (
          <div className={`st-drop ${drop ? 'drop-target' : ''}`} onClick={() => pickFile('audio/*', (f) => void load(f))}>
            <div className="st-drop-icon">🎵</div>
            <b>Drop a song here</b>
            <span>or click to choose an MP3 / WAV / OGG / M4A file</span>
          </div>
        ) : (
          <>
            <div className="st-song">
              <div className="st-song-head">
                <b>{song.name}</b>
                <span className="label">{song.buffer.duration.toFixed(1)} s · {song.buffer.numberOfChannels === 1 ? 'mono' : 'stereo'}</span>
                <span className="spacer" />
                <button className="btn small" onClick={() => (playing === 'orig' ? stopPreview() : preview('orig', song.buffer))}><Icon name={playing === 'orig' ? 'stop' : 'play'} size={10} /> Original</button>
                <button className="btn small" onClick={() => toPlaylist([{ stem: 'original', buffer: song.buffer }])}>Original → playlist</button>
                <button className="btn small ghost" onClick={() => { stopPreview(); setSong(null); setParts([]); }}>Other song</button>
              </div>
              <Wave buffer={song.buffer} color="#3fa7a0" />
              {mono && <div className="st-warn">This song is mono – vocals can't be separated by position, the result will be rough.</div>}
            </div>
            <div className="st-options">
              {STEMS.map((s) => (
                <label key={s.id} className={`st-opt ${want.has(s.id) ? 'on' : ''}`} style={{ ['--sc' as string]: s.color }}>
                  <input type="checkbox" checked={want.has(s.id)} onChange={(e) => { const n = new Set(want); if (e.target.checked) n.add(s.id); else n.delete(s.id); setWant(n); }} />
                  <span className="st-opt-icon">{s.icon}</span>
                  <span><b>{s.label}</b><br /><small>{s.info}</small></span>
                </label>
              ))}
              <Knob value={focus} min={0} max={1} defaultValue={0.6} size={34} label="Vocal focus" format={(v) => `${Math.round(v * 100)}%`} onChange={setFocus} title="Higher = only clearly centred sound counts as vocals" />
            </div>
            <div className="st-actions">
              {progress !== null ? (
                <div className="st-progress"><div style={{ transform: `scaleX(${Math.max(0.02, progress)})` }} /><span>Splitting… {Math.round(progress * 100)}%</span></div>
              ) : (
                <button className="btn primary" disabled={!want.size} onClick={() => void split()}>✂️ Split song</button>
              )}
            </div>
            {parts.length > 0 && (
              <div className="st-parts">
                {parts.map((p) => {
                  const s = STEMS.find((x) => x.id === p.stem)!;
                  return (
                    <div key={p.stem} className="st-part" style={{ ['--sc' as string]: s.color }}>
                      <div className="st-part-head">
                        <span>{s.icon} <b>{s.label}</b></span>
                        <span className="spacer" />
                        <button className="btn small" onClick={() => (playing === p.stem ? stopPreview() : preview(p.stem, p.buffer))}><Icon name={playing === p.stem ? 'stop' : 'play'} size={10} /> {playing === p.stem ? 'Stop' : 'Listen'}</button>
                        <button className="btn small" onClick={() => toPlaylist([p])}><Icon name="playlist" size={10} /> To playlist</button>
                        <button className="btn small" onClick={() => void toLibrary(p)}><Icon name="save" size={10} /> To library</button>
                      </div>
                      <Wave buffer={p.buffer} color={s.color} />
                    </div>
                  );
                })}
                <button className="btn primary" onClick={() => toPlaylist(parts)}>Add all parts to the playlist</button>
              </div>
            )}
          </>
        )}
        <p className="set-hint">The splitter runs locally with signal processing (centre extraction + harmonic/percussive separation) – no upload, no AI model. It works best on stereo studio songs; expect some bleed between the parts.</p>
      </div>
    </Window>
  );
}
