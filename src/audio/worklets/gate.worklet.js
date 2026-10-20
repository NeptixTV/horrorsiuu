// "Goofy Gate" – noise gate / expander.
//
// Opens when the (stereo-linked) level rises above the threshold and closes
// when it falls 4 dB below it (hysteresis, so it does not chatter), after the
// hold time. The gain moves with the attack / release times, so opening and
// closing never clicks. `range` is how far a closed gate turns the signal
// down (−80 dB ≈ silent, −10 dB = gentle expander).

class GoofyGate extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    this.sr = sampleRate;
    this.open = 0; // 0 closed … 1 open (target)
    this.gain = 0;
    this.env = 0;
    this.holdLeft = 0;
    this.sinceReport = 0;
    this.minGain = 1;
    this.peak = 0;
    this.set({ threshold: -45, attack: 1, hold: 40, release: 120, range: -80 });
    this.port.onmessage = (e) => {
      // the owner removed this node: end the processor so the node can be freed
      if (e.data && e.data.type === 'dispose') {
        this.disposed = true;
        return;
      }
      if (e.data && e.data.type === 'params') this.set(e.data);
    };
  }

  set(p) {
    const fin = (v, lo, hi, d) => (typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d);
    const sr = this.sr;
    const thr = fin(p.threshold, -90, 0, -45);
    this.openAt = Math.pow(10, thr / 20);
    this.closeAt = Math.pow(10, (thr - 4) / 20);
    this.atk = 1 - Math.exp(-1 / (Math.max(0.05, fin(p.attack, 0.05, 200, 1)) * 0.001 * sr));
    this.rel = 1 - Math.exp(-1 / (Math.max(1, fin(p.release, 1, 3000, 120)) * 0.001 * sr));
    this.holdSamples = Math.round(fin(p.hold, 0, 2000, 40) * 0.001 * sr);
    this.floor = Math.pow(10, fin(p.range, -80, 0, -80) / 20);
    // envelope follower: instant attack, ~10 ms decay
    this.envRel = 1 - Math.exp(-1 / (0.01 * sr));
  }

  process(inputs, outputs) {
    if (this.disposed) return false;
    const input = inputs[0];
    const output = outputs[0];
    if (!output || !output.length) return true;
    const n = output[0].length;
    const inL = input && input[0];
    const inR = input && (input[1] || input[0]);
    const outL = output[0];
    const outR = output[1] || output[0];
    if (!inL) {
      outL.fill(0);
      if (outR !== outL) outR.fill(0);
      return true;
    }
    for (let i = 0; i < n; i++) {
      let l = inL[i];
      let r = inR[i];
      if (!Number.isFinite(l)) l = 0;
      if (!Number.isFinite(r)) r = 0;
      const a = Math.max(Math.abs(l), Math.abs(r));
      this.env = a > this.env ? a : this.env + (a - this.env) * this.envRel;
      if (this.env >= this.openAt) {
        this.open = 1;
        this.holdLeft = this.holdSamples;
      } else if (this.env < this.closeAt) {
        if (this.holdLeft > 0) this.holdLeft--;
        else this.open = 0;
      }
      const target = this.open ? 1 : this.floor;
      this.gain += (target - this.gain) * (target > this.gain ? this.atk : this.rel);
      outL[i] = l * this.gain;
      if (outR !== outL) outR[i] = r * this.gain;
      if (this.gain < this.minGain) this.minGain = this.gain;
      if (a > this.peak) this.peak = a;
    }
    this.sinceReport += n;
    if (this.sinceReport >= this.sr / 20) {
      this.port.postMessage({ type: 'state', gain: this.gain, minGain: this.minGain, peak: this.peak, open: this.open === 1 });
      this.sinceReport = 0;
      this.minGain = 1;
      this.peak = 0;
    }
    return true;
  }
}

registerProcessor('goofy-gate', GoofyGate);
