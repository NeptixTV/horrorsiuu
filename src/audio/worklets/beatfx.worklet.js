// Beat effects that need per-sample processing:
// "goofy-transient" – transient shaper (more / less punch and tail)
// "goofy-lofi"      – bit depth and sample-rate reduction
// Both stop on a 'dispose' message and never allocate while processing.

const fin = (v, lo, hi, d) => (typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d);

class GoofyTransient extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    const sr = sampleRate;
    this.fast = 0;
    this.slow = 0;
    this.g = 1;
    // fast follower: ~1 ms attack, 30 ms release – slow one: 25 ms / 250 ms
    this.fa = 1 - Math.exp(-1 / (0.001 * sr));
    this.fr = 1 - Math.exp(-1 / (0.03 * sr));
    this.sa = 1 - Math.exp(-1 / (0.025 * sr));
    this.sr = 1 - Math.exp(-1 / (0.25 * sr));
    this.gs = 1 - Math.exp(-1 / (0.002 * sr));
    this.attack = 0.5;
    this.sustain = 0;
    this.out = 1;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (!m) return;
      if (m.type === 'dispose') { this.disposed = true; return; }
      if (m.type === 'params') {
        this.attack = fin(m.attack, -1, 1, 0.5);
        this.sustain = fin(m.sustain, -1, 1, 0);
        this.out = Math.pow(10, fin(m.output, -24, 12, 0) / 20);
      }
    };
  }

  process(inputs, outputs) {
    if (this.disposed) return false;
    const input = inputs[0];
    const output = outputs[0];
    if (!output || !output.length) return true;
    const outL = output[0];
    const outR = output[1] || output[0];
    const inL = input && input[0];
    if (!inL) {
      outL.fill(0);
      if (outR !== outL) outR.fill(0);
      return true;
    }
    const inR = input[1] || inL;
    const n = outL.length;
    let fast = this.fast;
    let slow = this.slow;
    let g = this.g;
    const atk = this.attack * 14;
    const sus = this.sustain * 14;
    for (let i = 0; i < n; i++) {
      let l = inL[i];
      let r = inR[i];
      if (!(l > -1e9 && l < 1e9)) l = 0;
      if (!(r > -1e9 && r < 1e9)) r = 0;
      const a = Math.max(l < 0 ? -l : l, r < 0 ? -r : r);
      fast += (a - fast) * (a > fast ? this.fa : this.fr);
      slow += (a - slow) * (a > slow ? this.sa : this.sr);
      // dB difference: positive while a hit starts, negative in its tail
      const d = 8.686 * Math.log((fast + 1e-6) / (slow + 1e-6));
      let gdb = d > 0 ? atk * Math.min(1, d / 6) : sus * Math.min(1, -d / 6);
      if (gdb > 18) gdb = 18; else if (gdb < -18) gdb = -18;
      const target = Math.exp(gdb * 0.11513);
      g += (target - g) * this.gs;
      const k = g * this.out;
      outL[i] = l * k;
      if (outR !== outL) outR[i] = r * k;
    }
    if (fast < 1e-12) fast = 0;
    if (slow < 1e-12) slow = 0;
    this.fast = fast;
    this.slow = slow;
    this.g = g;
    return true;
  }
}

class GoofyLofi extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    this.sr = sampleRate;
    this.levels = 256;
    this.step = 1;
    this.phase = 0;
    this.holdL = 0;
    this.holdR = 0;
    this.port.onmessage = (e) => {
      const m = e.data;
      if (!m) return;
      if (m.type === 'dispose') { this.disposed = true; return; }
      if (m.type === 'params') {
        const bits = fin(m.bits, 2, 16, 12);
        this.levels = Math.pow(2, bits - 1);
        const rate = fin(m.rate, 1000, 48000, 22050);
        this.step = Math.min(1, rate / this.sr);
      }
    };
  }

  process(inputs, outputs) {
    if (this.disposed) return false;
    const input = inputs[0];
    const output = outputs[0];
    if (!output || !output.length) return true;
    const outL = output[0];
    const outR = output[1] || output[0];
    const inL = input && input[0];
    if (!inL) {
      outL.fill(0);
      if (outR !== outL) outR.fill(0);
      return true;
    }
    const inR = input[1] || inL;
    const q = this.levels;
    const step = this.step;
    let ph = this.phase;
    let hl = this.holdL;
    let hr = this.holdR;
    for (let i = 0; i < outL.length; i++) {
      ph += step;
      if (ph >= 1) {
        ph -= 1;
        let l = inL[i];
        let r = inR[i];
        if (!(l > -1e9 && l < 1e9)) l = 0;
        if (!(r > -1e9 && r < 1e9)) r = 0;
        hl = Math.round(l * q) / q;
        hr = Math.round(r * q) / q;
      }
      outL[i] = hl;
      if (outR !== outL) outR[i] = hr;
    }
    this.phase = ph;
    this.holdL = hl;
    this.holdR = hr;
    return true;
  }
}

registerProcessor('goofy-transient', GoofyTransient);
registerProcessor('goofy-lofi', GoofyLofi);
