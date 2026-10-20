// "Goofy Guard" – the very last stage before the speakers / headphones.
//
// 1. Scrubs non-finite samples (NaN / Infinity) so a broken node can never
//    send garbage to the device, and reports them so the engine can rebuild
//    the poisoned part of the graph.
// 2. Brickwall limiter with one render quantum (128 samples ≈ 2.7 ms) of
//    look-ahead. It works per block: the peak of the incoming block decides
//    the gain at the end of the block that is played now, and the gain moves
//    there in a straight line – nothing above the ceiling reaches the output,
//    and the cost is one multiplication per sample. After a reduction the
//    gain holds for ~25 ms before it releases, so bass notes are not
//    "pumped" within their own waveform (that sounds rough and crackly).
// 3. Runaway protection: a signal that stays absurdly hot (> +24 dBFS before
//    the limiter, e.g. a feedback loop) is faded to silence and reported.
//    The engine un-mutes after it has rebuilt the graph.
//
// It runs on every block of the whole app, so it must stay cheap and must
// never allocate while processing.

class GoofyGuard extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    const sr = sampleRate;
    this.sr = sr;
    this.enabled = true;
    this.ceiling = Math.pow(10, -0.3 / 20);
    this.protect = true;
    // delayed block (look-ahead) – grows if the browser ever renders bigger blocks
    this.dL = new Float32Array(128);
    this.dR = new Float32Array(128);
    this.dPeak = 0;
    this.gain = 1;
    this.holdLeft = 0;
    this.holdSamples = Math.round(0.025 * sr);
    this.releaseTc = 0.08 * sr; // samples
    // runaway / mute state
    this.hot = 0;
    this.hotLimit = Math.round((0.25 * sr) / 128);
    this.muted = false;
    this.muteGain = 1;
    this.muteStep = 1 / (0.02 * sr);
    // reporting
    this.nan = 0;
    this.minGain = 1;
    this.peak = 0;
    this.reportEvery = Math.round(0.25 * sr);
    this.sinceReport = 0;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (!m) return;
      // the owner removed this node: end the processor so the node can be freed
      if (m.type === 'dispose') {
        this.disposed = true;
        return;
      }
      if (m.type === 'config') {
        if (typeof m.enabled === 'boolean') this.enabled = m.enabled;
        if (typeof m.ceiling === 'number' && Number.isFinite(m.ceiling)) this.ceiling = Math.min(1, Math.max(0.05, m.ceiling));
        if (typeof m.protect === 'boolean') this.protect = m.protect;
      } else if (m.type === 'unmute') {
        this.muted = false;
        this.hot = 0;
      } else if (m.type === 'mute') {
        this.muted = true;
      }
    };
  }

  process(inputs, outputs) {
    if (this.disposed) return false;
    const input = inputs[0];
    const output = outputs[0];
    if (!output || output.length === 0) return true;
    const outL = output[0];
    const outR = output[1] || output[0];
    const n = outL.length;
    const inL = input && input[0] ? input[0] : null;
    const inR = input && input[1] ? input[1] : inL;
    if (this.dL.length !== n) {
      this.dL = new Float32Array(n);
      this.dR = new Float32Array(n);
      this.dPeak = 0;
    }
    const dL = this.dL;
    const dR = this.dR;

    // peak of the incoming block (it is played on the next call)
    let peak = 0;
    let bad = 0;
    if (inL) {
      for (let i = 0; i < n; i++) {
        const l = inL[i];
        const r = inR[i];
        // a single comparison catches NaN and ±Infinity
        if (!(l > -1e9 && l < 1e9) || !(r > -1e9 && r < 1e9)) { bad++; continue; }
        const a = l < 0 ? -l : l;
        const b = r < 0 ? -r : r;
        if (a > peak) peak = a;
        if (b > peak) peak = b;
      }
    }

    // gain at the end of the block that is played now
    const c = this.ceiling;
    const g0 = this.gain;
    let g1 = 1;
    if (this.enabled) {
      const gD = this.dPeak > c ? c / this.dPeak : 1;
      const gC = peak > c ? c / peak : 1;
      const target = gD < gC ? gD : gC;
      if (target < g0) {
        g1 = target;
        this.holdLeft = this.holdSamples;
      } else if (this.holdLeft > 0) {
        g1 = g0;
        this.holdLeft -= n;
      } else {
        g1 = target + (g0 - target) * Math.exp(-n / this.releaseTc);
        if (g1 > 0.99999) g1 = 1;
      }
    }
    const dg = (g1 - g0) / n;

    // mute fade, per block
    const m0 = this.muteGain;
    let m1 = m0;
    if (this.muted) m1 = Math.max(0, m0 - this.muteStep * n);
    else if (m0 < 1) m1 = Math.min(1, m0 + this.muteStep * 0.25 * n);
    const dm = (m1 - m0) / n;

    let g = g0;
    let mg = m0;
    if (this.enabled) {
      for (let i = 0; i < n; i++) {
        g += dg;
        mg += dm;
        const k = g * mg;
        let yl = dL[i] * k;
        let yr = dR[i] * k;
        // numerical safety only – the gain already keeps peaks below the ceiling
        if (yl > c) yl = c; else if (yl < -c) yl = -c;
        if (yr > c) yr = c; else if (yr < -c) yr = -c;
        outL[i] = yl;
        if (outR !== outL) outR[i] = yr;
      }
    } else {
      for (let i = 0; i < n; i++) {
        mg += dm;
        outL[i] = dL[i] * mg;
        if (outR !== outL) outR[i] = dR[i] * mg;
      }
    }
    this.gain = g1;
    this.muteGain = m1;
    if (g1 < this.minGain) this.minGain = g1;

    // keep the incoming block (scrubbed) for the next call
    if (inL) {
      if (bad === 0) {
        dL.set(inL);
        dR.set(inR);
      } else {
        for (let i = 0; i < n; i++) {
          const l = inL[i];
          const r = inR[i];
          dL[i] = l > -1e9 && l < 1e9 ? l : 0;
          dR[i] = r > -1e9 && r < 1e9 ? r : 0;
        }
        this.nan += bad;
      }
    } else {
      dL.fill(0);
      dR.fill(0);
    }
    this.dPeak = peak;

    if (peak > this.peak) this.peak = peak;
    if (this.protect && !this.muted) {
      if (peak > 16) this.hot++;
      else if (this.hot > 0) this.hot--;
      if (this.hot >= this.hotLimit) {
        this.muted = true;
        this.port.postMessage({ type: 'runaway', peak });
      }
    }

    this.sinceReport += n;
    if (this.sinceReport >= this.reportEvery) {
      this.sinceReport = 0;
      this.port.postMessage({ type: 'stats', nan: this.nan, minGain: this.minGain, peak: this.peak, muted: this.muted });
      this.nan = 0;
      this.minGain = 1;
      this.peak = 0;
    }
    return true;
  }
}

registerProcessor('goofy-guard', GoofyGuard);
