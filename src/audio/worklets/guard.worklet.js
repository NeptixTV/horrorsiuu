// "Goofy Guard" – the very last stage before the speakers / headphones.
//
// 1. Scrubs non-finite samples (NaN / Infinity) so a broken node can never
//    send garbage to the device, and reports them so the engine can rebuild
//    the poisoned part of the graph.
// 2. Brickwall limiter with 1.5 ms look-ahead (sliding-window peak, fast
//    attack, smooth release, final safety clamp) – nothing above the ceiling
//    reaches the output.
// 3. Runaway protection: a signal that stays absurdly hot (> +24 dBFS before
//    the limiter, e.g. a feedback loop) is faded to silence and reported.
//    The engine un-mutes after it has rebuilt the graph.

class GoofyGuard extends AudioWorkletProcessor {
  constructor() {
    super();
    // eslint-disable-next-line no-undef
    const sr = sampleRate;
    this.sr = sr;
    this.enabled = true;
    this.ceiling = Math.pow(10, -0.3 / 20);
    this.protect = true;
    this.W = Math.max(8, Math.round(0.0015 * sr));
    this.size = this.W + 1;
    this.dlL = new Float32Array(this.size);
    this.dlR = new Float32Array(this.size);
    this.pos = 0;
    // monotonic deque (sliding-window maximum of the peak)
    this.dqIdx = new Float64Array(this.size + 2);
    this.dqVal = new Float32Array(this.size + 2);
    this.dqHead = 0;
    this.dqLen = 0;
    this.n = 0;
    this.gain = 1;
    this.attack = 1 - Math.exp(-4.6 / this.W); // ~99% of the reduction inside the look-ahead
    this.release = 1 - Math.exp(-1 / (0.08 * sr));
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

  pushPeak(v) {
    const cap = this.size + 2;
    // drop smaller values from the back
    while (this.dqLen > 0) {
      const back = (this.dqHead + this.dqLen - 1) % cap;
      if (this.dqVal[back] <= v) this.dqLen--;
      else break;
    }
    const at = (this.dqHead + this.dqLen) % cap;
    this.dqIdx[at] = this.n;
    this.dqVal[at] = v;
    this.dqLen++;
    // drop values that left the window from the front
    while (this.dqLen > 0 && this.dqIdx[this.dqHead] <= this.n - this.size) {
      this.dqHead = (this.dqHead + 1) % cap;
      this.dqLen--;
    }
    this.n++;
    return this.dqVal[this.dqHead];
  }

  process(inputs, outputs) {
    const input = inputs[0];
    const output = outputs[0];
    if (!output || output.length === 0) return true;
    const outL = output[0];
    const outR = output[1] || output[0];
    const frames = outL.length;
    const inL = input && input[0] ? input[0] : null;
    const inR = input && input[1] ? input[1] : inL;
    let blockPeak = 0;

    for (let i = 0; i < frames; i++) {
      let l = inL ? inL[i] : 0;
      let r = inR ? inR[i] : 0;
      if (!Number.isFinite(l)) { l = 0; this.nan++; }
      if (!Number.isFinite(r)) { r = 0; this.nan++; }
      const p = Math.max(l < 0 ? -l : l, r < 0 ? -r : r);
      if (p > blockPeak) blockPeak = p;

      let yl;
      let yr;
      if (this.enabled) {
        const env = this.pushPeak(p);
        const target = env > this.ceiling ? this.ceiling / env : 1;
        this.gain += (target - this.gain) * (target < this.gain ? this.attack : this.release);
        const w = this.pos;
        const rd = (w + 1) % this.size; // oldest sample = look-ahead delay
        this.dlL[w] = l;
        this.dlR[w] = r;
        yl = this.dlL[rd] * this.gain;
        yr = this.dlR[rd] * this.gain;
        this.pos = rd;
        const c = this.ceiling;
        if (yl > c) yl = c; else if (yl < -c) yl = -c;
        if (yr > c) yr = c; else if (yr < -c) yr = -c;
        if (this.gain < this.minGain) this.minGain = this.gain;
      } else {
        yl = l;
        yr = r;
      }

      if (this.muted) {
        this.muteGain = Math.max(0, this.muteGain - this.muteStep);
      } else if (this.muteGain < 1) {
        this.muteGain = Math.min(1, this.muteGain + this.muteStep * 0.25);
      }
      outL[i] = yl * this.muteGain;
      if (outR !== outL) outR[i] = yr * this.muteGain;
    }

    if (blockPeak > this.peak) this.peak = blockPeak;
    if (this.protect && !this.muted) {
      if (blockPeak > 16) this.hot++;
      else if (this.hot > 0) this.hot--;
      if (this.hot >= this.hotLimit) {
        this.muted = true;
        this.port.postMessage({ type: 'runaway', peak: blockPeak });
      }
    }

    this.sinceReport += frames;
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
