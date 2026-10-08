import { Goniometer, Oscilloscope, Spectrogram, Spectrum, VUMeter } from './Visualizers';
import { Meter } from '../components/Meter';
import { engine } from '../../audio/engine';
import './analyzer.css';

function master() {
  if (!engine.initialized) return null;
  const l = engine.graph.master.levels();
  return { l: l.peakL, r: l.peakR };
}

export function AnalyzerPanel() {
  return (
    <div className="an">
      <div className="an-card an-spectrum"><div className="an-label">Spectrum Analyzer · Master</div><Spectrum className="fill" /></div>
      <div className="an-card an-scope"><div className="an-label">Oscilloscope</div><Oscilloscope className="fill" /></div>
      <div className="an-card an-gonio"><div className="an-label">Stereo Field</div><Goniometer className="fill" /></div>
      <div className="an-card an-vu">
        <div className="an-label">VU · Peak</div>
        <div className="an-vu-row">
          <VUMeter className="an-vu-c" channel={0} />
          <VUMeter className="an-vu-c" channel={1} />
          <Meter source={master} width={22} height={110} />
        </div>
      </div>
      <div className="an-card an-sgram"><div className="an-label">Spectrogram</div><Spectrogram className="fill" /></div>
    </div>
  );
}
