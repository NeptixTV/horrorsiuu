import { useEffect, useState } from 'react';
import logo from '../../assets/logo.png';
import './splash.css';

/**
 * Start-up splash: the logo floats up and settles with soft glows, then the
 * loading progress of the engine is shown and the splash fades out.
 */
export function Splash({ progress, label, done, onGone }: { progress: number; label: string; done: boolean; onGone: () => void }) {
  const [phase, setPhase] = useState<'intro' | 'loading' | 'out'>('intro');
  useEffect(() => {
    const t = setTimeout(() => setPhase('loading'), 1100);
    return () => clearTimeout(t);
  }, []);
  useEffect(() => {
    if (done && phase === 'loading') {
      const t = setTimeout(() => setPhase('out'), 450);
      return () => clearTimeout(t);
    }
  }, [done, phase]);
  useEffect(() => {
    if (phase === 'out') {
      const t = setTimeout(onGone, 700);
      return () => clearTimeout(t);
    }
  }, [phase, onGone]);

  return (
    <div className={`splash ${phase}`}>
      <div className="splash-glow g1" />
      <div className="splash-glow g2" />
      <div className="splash-rays" />
      <div className="splash-center">
        <div className="splash-logo-wrap">
          <img src={logo} className="splash-logo" alt="Goofy Studio" draggable={false} />
          <div className="splash-shine" style={{ WebkitMaskImage: `url(${logo})`, maskImage: `url(${logo})` }} />
        </div>
        <div className="splash-title">
          GOOFY<span>STUDIO</span>
        </div>
        <div className="splash-sub">Digital Audio Workstation</div>
        <div className="splash-progress">
          <div className="splash-bar" style={{ transform: `scaleX(${Math.max(0.02, progress)})` }} />
        </div>
        <div className="splash-label">{phase === 'intro' ? ' ' : label}</div>
      </div>
      <div className="splash-footer">v1.0 · Web Audio Engine</div>
    </div>
  );
}
