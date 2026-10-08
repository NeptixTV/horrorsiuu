// In-memory registry of decoded audio (recordings, imports, built-in samples).
// Persisted to IndexedDB by the project storage layer.

export interface Peaks {
  /** min/max pairs, one pair per `samplesPerPeak` samples (mono mix). */
  data: Float32Array;
  samplesPerPeak: number;
}

type Listener = () => void;

class AssetRegistry {
  private buffers = new Map<string, AudioBuffer>();
  private reversed = new Map<string, AudioBuffer>();
  private peaks = new Map<string, Peaks>();
  private names = new Map<string, string>();
  private listeners = new Set<Listener>();
  version = 0;

  has(id: string) {
    return this.buffers.has(id);
  }

  get(id: string): AudioBuffer | undefined {
    return this.buffers.get(id);
  }

  name(id: string) {
    return this.names.get(id) ?? id;
  }

  ids() {
    return [...this.buffers.keys()];
  }

  add(id: string, buffer: AudioBuffer, name?: string) {
    this.buffers.set(id, buffer);
    this.reversed.delete(id);
    this.peaks.delete(id);
    if (name) this.names.set(id, name);
    this.version++;
    this.listeners.forEach((l) => l());
  }

  remove(id: string) {
    this.buffers.delete(id);
    this.reversed.delete(id);
    this.peaks.delete(id);
    this.version++;
    this.listeners.forEach((l) => l());
  }

  getReversed(id: string): AudioBuffer | undefined {
    let r = this.reversed.get(id);
    if (!r) {
      const b = this.buffers.get(id);
      if (!b) return undefined;
      r = new AudioBuffer({ length: b.length, numberOfChannels: b.numberOfChannels, sampleRate: b.sampleRate });
      for (let c = 0; c < b.numberOfChannels; c++) {
        const src = b.getChannelData(c);
        const dst = r.getChannelData(c);
        for (let i = 0, n = src.length; i < n; i++) dst[i] = src[n - 1 - i];
      }
      this.reversed.set(id, r);
    }
    return r;
  }

  getPeaks(id: string): Peaks | undefined {
    let p = this.peaks.get(id);
    if (!p) {
      const b = this.buffers.get(id);
      if (!b) return undefined;
      p = computePeaks(b, 128);
      this.peaks.set(id, p);
    }
    return p;
  }

  subscribe(l: Listener) {
    this.listeners.add(l);
    return () => this.listeners.delete(l);
  }
}

export function computePeaks(b: AudioBuffer, samplesPerPeak: number): Peaks {
  const n = Math.ceil(b.length / samplesPerPeak);
  const data = new Float32Array(n * 2);
  const chans = Array.from({ length: b.numberOfChannels }, (_, c) => b.getChannelData(c));
  for (let i = 0; i < n; i++) {
    let mn = 1;
    let mx = -1;
    const end = Math.min(b.length, (i + 1) * samplesPerPeak);
    for (let s = i * samplesPerPeak; s < end; s++) {
      let v = 0;
      for (const ch of chans) v += ch[s];
      v /= chans.length;
      if (v < mn) mn = v;
      if (v > mx) mx = v;
    }
    data[i * 2] = mn;
    data[i * 2 + 1] = mx;
  }
  return { data, samplesPerPeak };
}

export const assets = new AssetRegistry();
