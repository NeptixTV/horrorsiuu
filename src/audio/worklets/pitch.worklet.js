// "Goofy Tune" – real-time pitch correction AudioWorklet.
//
// 1. Pitch detection: YIN on a 2x decimated mono signal (70 Hz – 1 kHz).
// 2. Target: nearest note of the selected key/scale (via a 12 note mask),
//    blended by the "correction" amount.
// 3. Smoothing: the pitch shift glides towards the target with the retune
//    speed (0 ms = hard robotic tuning). Humanize slows the retune on
//    sustained notes so natural vibrato survives.
// 4. Pitch shifting: dual read-head delay-line shifter with sin² crossfades.
//    The dry signal is delayed by half a window so wet/dry stays phase aligned.

const LN2_12 = Math.LN2 / 12;

class GoofyPitch extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    const sr = sampleRate;
    this.sr = sr;
    this.dsr = sr / 2;
    this.W = 512;
    this.tauMin = Math.floor(this.dsr / 1000);
    this.tauMax = Math.floor(this.dsr / 70);
    this.aSize = this.W + this.tauMax + 2;
    this.ring = new Float32Array(4096);
    this.ringMask = 4095;
    this.rpos = 0;
    this.frame = new Float32Array(this.aSize);
    this.diff = new Float32Array(this.tauMax + 2);
    this.decimAcc = 0;
    this.decimPhase = 0;
    this.hop = 512;
    this.hopCount = 0;

    this.dlSize = 16384;
    this.dlMask = this.dlSize - 1;
    this.dl = [new Float32Array(this.dlSize), new Float32Array(this.dlSize)];
    this.wpos = 0;
    this.L = Math.round(0.03 * sr);
    this.phi = 0;

    this.shift = 0;
    this.targetShift = 0;
    this.alpha = 1;
    this.detected = -1;
    this.snapped = -1;
    this.held = 0;
    this.level = 0;
    this.reportCount = 0;

    this.mask = [true, false, true, false, true, true, false, true, false, true, false, true];
    this.correction = 1;
    this.retune = 20;
    this.humanize = 0.2;
    this.mix = 1;
    this.bypass = false;

    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'params') {
        if (m.mask) this.mask = m.mask;
        if (typeof m.correction === 'number') this.correction = m.correction;
        if (typeof m.retune === 'number') this.retune = m.retune;
        if (typeof m.humanize === 'number') this.humanize = m.humanize;
        if (typeof m.mix === 'number') this.mix = m.mix;
        if (typeof m.bypass === 'boolean') this.bypass = m.bypass;
      }
    };
  }

  nearestInScale(midi) {
    const base = Math.round(midi);
    for (let d = 0; d <= 6; d++) {
      let best = -1;
      let bestDist = 1e9;
      const cands = d === 0 ? [base] : [base - d, base + d];
      for (let k = 0; k < cands.length; k++) {
        const c = cands[k];
        if (this.mask[((c % 12) + 12) % 12]) {
          const dist = Math.abs(c - midi);
          if (dist < bestDist) {
            bestDist = dist;
            best = c;
          }
        }
      }
      if (best >= 0) return best;
    }
    return base;
  }

  analyze() {
    const { frame, ring, ringMask, aSize, W, tauMin, tauMax, diff } = this;
    const start = this.rpos - aSize;
    let energy = 0;
    for (let j = 0; j < aSize; j++) {
      const v = ring[(start + j) & ringMask];
      frame[j] = v;
      if (j < W) energy += v * v;
    }
    const rms = Math.sqrt(energy / W);
    this.level = rms;
    let freq = -1;
    if (rms > 0.008) {
      for (let tau = 1; tau <= tauMax; tau++) {
        let sum = 0;
        for (let j = 0; j < W; j++) {
          const d = frame[j] - frame[j + tau];
          sum += d * d;
        }
        diff[tau] = sum;
      }
      diff[0] = 1;
      let running = 0;
      for (let tau = 1; tau <= tauMax; tau++) {
        running += diff[tau];
        diff[tau] = running > 0 ? (diff[tau] * tau) / running : 1;
      }
      let tau = -1;
      for (let t = tauMin; t <= tauMax; t++) {
        if (diff[t] < 0.15) {
          while (t + 1 <= tauMax && diff[t + 1] < diff[t]) t++;
          tau = t;
          break;
        }
      }
      if (tau > 0) {
        let refined = tau;
        if (tau > 1 && tau < tauMax) {
          const a = diff[tau - 1];
          const b = diff[tau];
          const c = diff[tau + 1];
          const den = a + c - 2 * b;
          if (Math.abs(den) > 1e-9) refined = tau + (a - c) / (2 * den);
        }
        freq = this.dsr / refined;
      }
    }

    if (freq > 0) {
      const midi = 69 + 12 * Math.log2(freq / 440);
      const snapped = this.nearestInScale(midi);
      if (snapped === this.snapped) this.held += this.hop;
      else this.held = 0;
      this.snapped = snapped;
      this.detected = midi;
      this.targetShift = (snapped - midi) * this.correction;
      const sustained = this.held > 0.12 * this.sr;
      const ms = this.retune + (sustained ? this.humanize * 300 : 0);
      this.alpha = ms <= 0.5 ? 1 : 1 - Math.exp(-1 / (ms * 0.001 * this.sr));
    } else {
      this.detected = -1;
      this.snapped = -1;
      this.held = 0;
      this.targetShift = 0;
      this.alpha = 1 - Math.exp(-1 / (0.08 * this.sr));
    }
  }

  process(inputs, outputs) {
    const input = inputs[0];
    const output = outputs[0];
    if (!output || output.length === 0) return true;
    const frames = output[0].length;
    const inL = input && input[0] ? input[0] : null;
    const inR = input && input[1] ? input[1] : inL;

    if (!inL) {
      for (let c = 0; c < output.length; c++) output[c].fill(0);
      return true;
    }

    if (this.bypass) {
      output[0].set(inL);
      if (output[1]) output[1].set(inR);
      return true;
    }

    const L = this.L;
    const half = L >> 1;
    const dlMask = this.dlMask;
    const dlL = this.dl[0];
    const dlR = this.dl[1];
    const outL = output[0];
    const outR = output[1] || output[0];
    const mix = this.mix;

    for (let i = 0; i < frames; i++) {
      const xl = inL[i];
      const xr = inR[i];

      // analysis path (decimated mono)
      this.decimAcc += (xl + xr) * 0.5;
      if (++this.decimPhase === 2) {
        this.ring[this.rpos & this.ringMask] = this.decimAcc * 0.5;
        this.rpos++;
        this.decimAcc = 0;
        this.decimPhase = 0;
      }
      if (++this.hopCount >= this.hop) {
        this.hopCount = 0;
        this.analyze();
      }

      this.shift += (this.targetShift - this.shift) * this.alpha;
      const ratio = Math.exp(this.shift * LN2_12);
      this.phi += (1 - ratio) / L;
      if (this.phi >= 1) this.phi -= 1;
      else if (this.phi < 0) this.phi += 1;

      const w = this.wpos;
      dlL[w] = xl;
      dlR[w] = xr;

      const d1 = this.phi * L + 1;
      let p2 = this.phi + 0.5;
      if (p2 >= 1) p2 -= 1;
      const d2 = p2 * L + 1;
      const s = Math.sin(Math.PI * this.phi);
      const w1 = s * s;
      const w2 = 1 - w1;

      const r1 = w - d1;
      const i1 = Math.floor(r1);
      const f1 = r1 - i1;
      const r2 = w - d2;
      const i2 = Math.floor(r2);
      const f2 = r2 - i2;
      const a1 = i1 & dlMask;
      const b1 = (i1 + 1) & dlMask;
      const a2 = i2 & dlMask;
      const b2 = (i2 + 1) & dlMask;
      const dryIdx = (w - half) & dlMask;

      const wetL = (dlL[a1] + (dlL[b1] - dlL[a1]) * f1) * w1 + (dlL[a2] + (dlL[b2] - dlL[a2]) * f2) * w2;
      const wetR = (dlR[a1] + (dlR[b1] - dlR[a1]) * f1) * w1 + (dlR[a2] + (dlR[b2] - dlR[a2]) * f2) * w2;
      outL[i] = dlL[dryIdx] * (1 - mix) + wetL * mix;
      if (outR !== outL) outR[i] = dlR[dryIdx] * (1 - mix) + wetR * mix;

      this.wpos = (w + 1) & dlMask;
    }

    this.reportCount += frames;
    if (this.reportCount >= this.sr / 30) {
      this.reportCount = 0;
      this.port.postMessage({
        type: 'pitch',
        detected: this.detected,
        target: this.snapped,
        shift: this.shift,
        level: this.level,
      });
    }
    return true;
  }
}

registerProcessor('goofy-pitch', GoofyPitch);
