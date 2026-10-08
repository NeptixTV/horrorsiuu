// AudioWorklet that captures the incoming signal sample-accurately.
// The main thread sends {type:'start', at} / {type:'stop'}; captured audio is
// posted back in chunks together with a small peak summary for live waveforms.

class GoofyRecorder extends AudioWorkletProcessor {
  constructor() {
    super();
    this.recording = false;
    this.startAt = 0;
    this.channels = 2;
    this.chunkSize = 4096;
    this.buf = [new Float32Array(this.chunkSize), new Float32Array(this.chunkSize)];
    this.fill = 0;
    this.port.onmessage = (e) => {
      const msg = e.data;
      if (msg.type === 'start') {
        this.recording = true;
        this.startAt = msg.at || 0;
        this.fill = 0;
      } else if (msg.type === 'stop') {
        this.flush();
        this.recording = false;
        this.port.postMessage({ type: 'stopped' });
      }
    };
  }

  flush() {
    if (this.fill === 0) return;
    const l = this.buf[0].slice(0, this.fill);
    const r = this.buf[1].slice(0, this.fill);
    let peak = 0;
    for (let i = 0; i < this.fill; i++) {
      const a = Math.max(Math.abs(l[i]), Math.abs(r[i]));
      if (a > peak) peak = a;
    }
    this.port.postMessage({ type: 'chunk', l, r, peak }, [l.buffer, r.buffer]);
    this.fill = 0;
  }

  process(inputs) {
    const input = inputs[0];
    if (!this.recording || !input || input.length === 0) return true;
    const frames = input[0].length;
    // eslint-disable-next-line no-undef
    const now = currentTime;
    let start = 0;
    if (now < this.startAt) {
      // eslint-disable-next-line no-undef
      const startFrame = Math.round((this.startAt - now) * sampleRate);
      if (startFrame >= frames) return true;
      start = startFrame;
    }
    const left = input[0];
    const right = input[1] || input[0];
    for (let i = start; i < frames; i++) {
      this.buf[0][this.fill] = left[i];
      this.buf[1][this.fill] = right[i];
      this.fill++;
      if (this.fill >= this.chunkSize) this.flush();
    }
    return true;
  }
}

registerProcessor('goofy-recorder', GoofyRecorder);
