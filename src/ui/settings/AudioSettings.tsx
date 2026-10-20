// Settings → Audio, Performance, Recording & MIDI.
import { useEffect, useState, useSyncExternalStore } from 'react';
import { engine } from '../../audio/engine';
import { AudioInput, isBluetoothCallDevice, type DeviceInfo } from '../../audio/input';
import { setUI, toast, useUI, type UIState } from '../../state/ui';
import { getProject } from '../../state/store';
import { midiDevices, onMidiDevices, refreshMidi } from '../shortcuts';
import { uiLoad } from '../components/frameLoop';
import { Icon } from '../components/Icon';
import { Row, Section, Select, Slider, Stat, Toggle } from './controls';

function useDevices() {
  const [devices, setDevices] = useState<{ inputs: DeviceInfo[]; outputs: DeviceInfo[] }>({ inputs: [], outputs: [] });
  const refresh = () => void AudioInput.listDevices().then(setDevices).catch(() => undefined);
  useEffect(() => {
    refresh();
    const md = navigator.mediaDevices;
    md?.addEventListener?.('devicechange', refresh);
    return () => md?.removeEventListener?.('devicechange', refresh);
  }, []);
  return { ...devices, refresh };
}

const pct = (v: number) => `${Math.round(v * 100)} %`;

export function AudioSection() {
  const s = useUI();
  const { inputs, outputs, refresh } = useDevices();
  const [, bump] = useState(0);
  useEffect(() => engine.onChange(() => bump((x) => x + 1)), []);
  const fellBack = !!s.outputDeviceId && engine.currentSink !== s.outputDeviceId;
  const outLabel = outputs.find((o) => o.deviceId === s.outputDeviceId)?.label ?? '';
  const inLabel = inputs.find((o) => o.deviceId === s.inputDeviceId)?.label ?? '';
  const btWarn = isBluetoothCallDevice(outLabel) || isBluetoothCallDevice(inLabel);
  const noLabels = outputs.length > 0 && outputs.every((o) => /^Output \d+$/.test(o.label));

  return (
    <>
      <Section title="Output (speakers / headphones)">
        <Row
          label="Output device"
          desc={engine.supportsOutputSelection
            ? fellBack ? 'Your device is not connected – playing through the system default. It is used again as soon as it is back.' : 'Where Goofy Studio plays its sound.'
            : 'This browser always uses the system output device.'}
          warn={fellBack}
        >
          <div className="sc-inline">
            <Select
              width={230}
              value={s.outputDeviceId}
              options={[{ value: '', label: 'System default' }, ...outputs.filter((o) => o.deviceId && o.deviceId !== 'default').map((o) => ({ value: o.deviceId, label: o.label }))]}
              onChange={async (id) => {
                setUI({ outputDeviceId: id });
                const ok = await engine.setOutputDevice(id);
                if (!ok && id) toast('Could not switch to that device – using the default output', 'error');
              }}
            />
            <button className="btn icon small" title="Refresh devices" onClick={refresh}><Icon name="loop" size={12} /></button>
          </div>
        </Row>
        {noLabels && <Row label="Device names hidden" desc="The browser shows device names only after microphone access was granted once (Record & Tune → Enable input)."><span /></Row>}
        <Row label="Headphone check" desc="Plays a short, quiet beep on the left, right or both sides.">
          <div className="sc-inline">
            <button className="btn small" onClick={() => engine.playTestTone('left')}>Left</button>
            <button className="btn small" onClick={() => engine.playTestTone('both')}>Both</button>
            <button className="btn small" onClick={() => engine.playTestTone('right')}>Right</button>
          </div>
        </Row>
        <Row label="Listening volume" desc="Overall loudness of the app (does not change your mix or exports).">
          <Slider value={s.outputVolume} min={0} max={1} step={0.01} format={pct} onChange={(v) => setUI({ outputVolume: v })} />
        </Row>
        <Row label="Switch back automatically" desc="When your chosen device is unplugged, continue on the default device and return to it when it is plugged in again.">
          <Toggle value={s.autoFallbackOutput} onChange={(v) => setUI({ autoFallbackOutput: v })} />
        </Row>
      </Section>

      <Section title="Ear protection" hint="Protects your hearing and your speakers from sudden extreme peaks.">
        <Row label="Output limiter" desc="Nothing louder than the ceiling reaches the speakers. Only acts on peaks above it.">
          <Toggle value={s.limiter} onChange={(v) => setUI({ limiter: v })} />
        </Row>
        <Row label="Ceiling" desc="Highest level that is let through.">
          <Slider value={s.ceilingDb} min={-12} max={0} step={0.1} format={(v) => `${v.toFixed(1)} dBFS`} onChange={(v) => setUI({ ceilingDb: v })} />
        </Row>
        <Row label="Runaway protection" desc="Mutes within a quarter of a second if a feedback loop makes the signal explode, then repairs the mixer.">
          <Toggle value={s.protect} onChange={(v) => setUI({ protect: v })} />
        </Row>
      </Section>

      <Section title="Input (microphone)">
        <Row label="Input device" desc={btWarn ? 'Bluetooth headset microphone: while it is open, the headphones switch to low-quality call mode. Use a separate microphone for recording.' : 'Used for recording and live voice effects.'} warn={btWarn}>
          <Select
            width={230}
            value={s.inputDeviceId}
            options={[{ value: '', label: 'System default' }, ...inputs.filter((o) => o.deviceId && o.deviceId !== 'default').map((o) => ({ value: o.deviceId, label: o.label }))]}
            onChange={async (id) => {
              setUI({ inputDeviceId: id });
              // switch an open microphone right away
              if (engine.input?.isOpen) {
                const pinned = engine.input.pinned;
                const monitoring = engine.input.monitoring;
                if (await engine.input.open(id)) {
                  engine.input.pinned = pinned;
                  engine.input.setMonitoring(monitoring);
                  engine.sync(getProject());
                }
              }
            }}
          />
        </Row>
        <Row label="Release the microphone after recording" desc="Closes the mic when a take is done (unless you switched the input on yourself). Keeps Bluetooth headphones in high-quality mode.">
          <Toggle value={s.releaseMic} onChange={(v) => setUI({ releaseMic: v })} />
        </Row>
      </Section>
    </>
  );
}

const LATENCY: { value: string; label: string }[] = [
  { value: 'interactive', label: 'Lowest latency (can crackle)' },
  { value: 'balanced', label: 'Balanced (recommended)' },
  { value: 'playback', label: 'Most stable (playback)' },
  { value: 'custom', label: 'Custom buffer…' },
];

export function PerformanceSection() {
  const s = useUI();
  const [load, setLoad] = useState({ ui: 0, voices: 0 });
  useEffect(() => {
    const id = setInterval(() => setLoad({ ui: uiLoad(), voices: engine.initialized ? engine.graph.voiceCount() : 0 }), 500);
    return () => clearInterval(id);
  }, []);
  const latencyValue = s.latencyMs > 0 ? 'custom' : s.latencyHint;
  const pending = engine.config.latencyHint !== s.latencyHint || engine.config.latencyMs !== s.latencyMs || engine.config.sampleRate !== s.sampleRate;
  const apply = async () => {
    await engine.restart('audio settings changed', { latencyHint: s.latencyHint, latencyMs: s.latencyMs, sampleRate: s.sampleRate });
    toast(`Audio running at ${engine.ctx.sampleRate} Hz`, 'ok');
  };
  return (
    <>
      <Section title="Audio engine" hint="If you hear crackles, choose a more stable mode or a bigger buffer. Lower latency makes playing live feel more direct.">
        <Row label="Latency mode">
          <Select
            width={230}
            value={latencyValue}
            options={LATENCY}
            onChange={(v) => setUI(v === 'custom' ? { latencyMs: s.latencyMs || 40 } : { latencyHint: v as UIState['latencyHint'], latencyMs: 0 })}
          />
        </Row>
        {latencyValue === 'custom' && (
          <Row label="Output buffer" desc="Bigger = safer against dropouts, but higher latency.">
            <Slider value={s.latencyMs} min={5} max={200} step={1} format={(v) => `${v} ms`} onChange={(v) => setUI({ latencyMs: v })} />
          </Row>
        )}
        <Row label="Sample rate" desc="Automatic uses the rate of your audio device (recommended).">
          <Select width={230} value={s.sampleRate} options={[{ value: 0, label: 'Automatic (device)' }, ...[44100, 48000, 88200, 96000].map((r) => ({ value: r, label: `${r / 1000} kHz` }))]} onChange={(v) => setUI({ sampleRate: v })} />
        </Row>
        <Row label="Apply" desc={pending ? 'Changes take effect when the audio engine restarts (your project keeps playing).' : `Running: ${engine.initialized ? engine.ctx.sampleRate : '—'} Hz, ${engine.initialized ? Math.round(engine.ctx.baseLatency * 1000) : '—'} ms base latency.`} warn={pending}>
          <button className={`btn ${pending ? 'primary' : ''}`} onClick={() => void apply()}>Restart audio now</button>
        </Row>
        <Row label="Fix dropouts automatically" desc={engine.canCountDropouts ? 'When the sound card runs out of audio (crackles, gaps), the buffer is made bigger by itself and you get a short note.' : 'Dropouts can only be counted in the desktop app and current Chrome / Edge; late scheduling is still corrected.'}>
          <Toggle value={s.autoStabilize} onChange={(v) => setUI({ autoStabilize: v })} />
        </Row>
        <Row label="Scheduling buffer" desc="How far ahead notes are prepared. Bigger survives a busy computer without stutter; smaller makes changes during playback audible sooner.">
          <Select
            width={230}
            value={s.lookahead}
            options={[{ value: 0.07, label: 'Snappy (70 ms)' }, { value: 0.12, label: 'Balanced (120 ms)' }, { value: 0.2, label: 'Stable (200 ms)' }, { value: 0.35, label: 'Extra stable (350 ms)' }]}
            onChange={(v) => setUI({ lookahead: v })}
          />
        </Row>
      </Section>
      <Section title="Interface performance">
        <Row label="Meter & visualiser frame rate" desc="Lower values save CPU on slow computers and laptops on battery.">
          <Select width={160} value={s.meterFps} options={[{ value: 60, label: '60 fps' }, { value: 30, label: '30 fps' }, { value: 15, label: '15 fps' }]} onChange={(v) => setUI({ meterFps: v })} />
        </Row>
        <Row label="Reduce motion" desc="Turns off animations and transitions.">
          <Toggle value={s.reduceMotion} onChange={(v) => setUI({ reduceMotion: v })} />
        </Row>
        <div className="sc-stats">
          <Stat label="UI frame load" value={`${Math.round(load.ui * 100)} %`} warn={load.ui > 0.6} />
          <Stat label="Sounding voices" value={load.voices} />
        </div>
      </Section>
    </>
  );
}

export function RecordingSection() {
  const s = useUI();
  const devices = useSyncExternalStore(onMidiDevices, () => JSON.stringify(midiDevices()));
  const list = JSON.parse(devices) as { id: string; name: string }[];
  return (
    <>
      <Section title="Recording">
        <Row label="Count-in" desc="Metronome clicks before a recording starts (audio and DreamChord / MIDI recording).">
          <Select width={160} value={s.countInBars} options={[{ value: 0, label: 'Off' }, { value: 1, label: '1 bar' }, { value: 2, label: '2 bars' }]} onChange={(v) => setUI({ countInBars: v })} />
        </Row>
        <Row label="Metronome volume">
          <Slider value={s.metronomeVolume} min={0} max={1} step={0.01} format={pct} onChange={(v) => setUI({ metronomeVolume: v })} />
        </Row>
        <Row label="Quantise recorded notes" desc="Snaps notes played on DreamChord, MIDI or the computer keyboard to 1/16.">
          <Toggle value={s.recQuantize} onChange={(v) => setUI({ recQuantize: v })} />
        </Row>
        <Row label="Show lyrics while recording" desc="Opens the song notes as a teleprompter when you start an audio recording (only if the project has notes).">
          <Toggle value={s.lyricsOnRecord} onChange={(v) => setUI({ lyricsOnRecord: v })} />
        </Row>
      </Section>
      <Section title="MIDI keyboard" hint="Plug in a MIDI keyboard to play the selected channel. Sustain pedal, pitch bend and mod wheel (DreamChord) are supported.">
        <Row label="MIDI input" desc={list.length ? `${list.length} device${list.length === 1 ? '' : 's'} found.` : 'No MIDI device found.'}>
          <div className="sc-inline">
            <Select width={230} value={s.midiInput} options={[{ value: '', label: 'All devices' }, ...list.map((d) => ({ value: d.id, label: d.name })), { value: 'off', label: 'Off (do not use MIDI)' }]} onChange={(v) => { setUI({ midiInput: v }); refreshMidi(); }} />
            <button className="btn icon small" title="Search again" onClick={refreshMidi}><Icon name="loop" size={12} /></button>
          </div>
        </Row>
        <Row label="Computer keyboard as piano" desc="Z–M and Q–U rows play notes on the selected channel.">
          <Toggle value={s.typingKeyboard} onChange={(v) => setUI({ typingKeyboard: v })} />
        </Row>
      </Section>
    </>
  );
}
