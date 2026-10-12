// Classic-DSP source separation for songs (no AI model, runs offline in a worker):
//  • HPSS (median filtering of the spectrogram, Fitzgerald 2010) splits
//    harmonic (sustained) and percussive (drum) energy.
//  • Centre extraction (inter-channel coherence) finds what is panned to the
//    middle – usually the lead vocal.
//  • vocals = centre ∧ harmonic ∧ vocal band; the other stems get the rest.
// The four soft masks are complementary, so vocals + instrumental = original.

export type StemName = 'vocals' | 'instrumental' | 'drums' | 'melody';

export interface SeparateOptions {
  stems: StemName[];
  /** 0..1 how strictly only centred material counts as vocals */
  vocalFocus?: number;
  onProgress?: (p: number) => void;
}

export type StemResult = Partial<Record<StemName, [Float32Array, Float32Array]>>;

const N = 2048;
const HOP = 1024;
const BINS = N / 2 + 1;
const KT = 8; // ± frames for the horizontal (time) median
const KF = 8; // ± bins for the vertical (frequency) median

class FFT {
  readonly rev: Uint32Array;
  readonly cos: Float64Array;
  readonly sin: Float64Array;
  constructor(readonly n: number) {
    this.rev = new Uint32Array(n);
    const bits = Math.log2(n);
    for (let i = 0; i < n; i++) {
      let r = 0;
      for (let b = 0; b < bits; b++) r |= ((i >> b) & 1) << (bits - 1 - b);
      this.rev[i] = r;
    }
    this.cos = new Float64Array(n / 2);
    this.sin = new Float64Array(n / 2);
    for (let i = 0; i < n / 2; i++) {
      this.cos[i] = Math.cos((2 * Math.PI * i) / n);
      this.sin[i] = Math.sin((2 * Math.PI * i) / n);
    }
  }

  /** In-place complex FFT (inverse = true → unscaled inverse). */
  run(re: Float64Array, im: Float64Array, inverse = false) {
    const n = this.n;
    const rev = this.rev;
    for (let i = 0; i < n; i++) {
      const j = rev[i];
      if (j > i) {
        let t = re[i]; re[i] = re[j]; re[j] = t;
        t = im[i]; im[i] = im[j]; im[j] = t;
      }
    }
    const sgn = inverse ? 1 : -1;
    for (let size = 2; size <= n; size <<= 1) {
      const half = size >> 1;
      const step = n / size;
      for (let start = 0; start < n; start += size) {
        for (let k = 0; k < half; k++) {
          const wr = this.cos[k * step];
          const wi = sgn * this.sin[k * step];
          const a = start + k;
          const b = a + half;
          const xr = re[b] * wr - im[b] * wi;
          const xi = re[b] * wi + im[b] * wr;
          re[b] = re[a] - xr;
          im[b] = im[a] - xi;
          re[a] += xr;
          im[a] += xi;
        }
      }
    }
  }
}

function median(buf: Float64Array, n: number): number {
  // insertion sort on a tiny array
  for (let i = 1; i < n; i++) {
    const v = buf[i];
    let j = i - 1;
    while (j >= 0 && buf[j] > v) { buf[j + 1] = buf[j]; j--; }
    buf[j + 1] = v;
  }
  return buf[n >> 1];
}

interface Frame {
  // packed spectrum of L + iR (one complex FFT for both channels)
  re: Float64Array;
  im: Float64Array;
  mag: Float32Array; // mid magnitude per bin
}

export function separate(left: Float32Array, right: Float32Array, sampleRate: number, opts: SeparateOptions): StemResult {
  const len = left.length;
  const fft = new FFT(N);
  const win = new Float64Array(N);
  for (let i = 0; i < N; i++) win[i] = Math.sqrt(0.5 - 0.5 * Math.cos((2 * Math.PI * i) / N)); // sqrt-Hann (WOLA)
  const frames = Math.ceil((len + N) / HOP);
  const want = new Set(opts.stems);
  const out: StemResult = {};
  for (const s of want) out[s] = [new Float32Array(len), new Float32Array(len)];
  const focus = Math.min(1, Math.max(0, opts.vocalFocus ?? 0.6));
  const cPow = 1 + focus * 5;

  // band weight for vocals (soft 120 Hz – 9 kHz)
  const band = new Float32Array(BINS);
  for (let b = 0; b < BINS; b++) {
    const f = (b * sampleRate) / N;
    const lo = f < 80 ? 0 : f < 200 ? (f - 80) / 120 : 1;
    const hi = f > 12000 ? 0 : f > 8000 ? 1 - (f - 8000) / 4000 : 1;
    band[b] = lo * hi;
  }

  const ring: Frame[] = [];
  const RING = 2 * KT + 1;
  for (let i = 0; i < RING; i++) ring.push({ re: new Float64Array(N), im: new Float64Array(N), mag: new Float32Array(BINS) });
  const frameAt = (f: number) => ring[((f % RING) + RING) % RING];

  const analyse = (f: number) => {
    const fr = frameAt(f);
    const off = f * HOP - N; // first frame starts before the signal (centred padding)
    for (let i = 0; i < N; i++) {
      const s = off + i;
      const inside = s >= 0 && s < len;
      fr.re[i] = inside ? left[s] * win[i] : 0;
      fr.im[i] = inside ? right[s] * win[i] : 0;
    }
    fft.run(fr.re, fr.im);
    for (let b = 0; b < BINS; b++) {
      // unpack L and R from the packed spectrum
      const nb = (N - b) % N;
      const lr = (fr.re[b] + fr.re[nb]) / 2;
      const li = (fr.im[b] - fr.im[nb]) / 2;
      const rr = (fr.im[b] + fr.im[nb]) / 2;
      const ri = (fr.re[nb] - fr.re[b]) / 2;
      fr.mag[b] = Math.hypot(lr + rr, li + ri) / 2;
    }
  };

  const tmp = new Float64Array(Math.max(2 * KT + 1, 2 * KF + 1));
  const masks: Record<StemName, Float32Array> = {
    vocals: new Float32Array(BINS), instrumental: new Float32Array(BINS), drums: new Float32Array(BINS), melody: new Float32Array(BINS),
  };
  const oRe = new Float64Array(N);
  const oIm = new Float64Array(N);

  const process = (t: number) => {
    const fr = frameAt(t);
    for (let b = 0; b < BINS; b++) {
      // horizontal median (sustained energy)
      let n = 0;
      for (let k = -KT; k <= KT; k++) {
        const f = t + k;
        if (f < 0 || f >= frames) continue;
        tmp[n++] = frameAt(f).mag[b];
      }
      const H = median(tmp, n);
      n = 0;
      for (let k = -KF; k <= KF; k++) {
        const bb = b + k;
        if (bb < 0 || bb >= BINS) continue;
        tmp[n++] = fr.mag[bb];
      }
      const P = median(tmp, n);
      const h2 = H * H;
      const p2 = P * P;
      const mH = h2 / (h2 + p2 + 1e-12);
      const mP = 1 - mH;
      // centre coherence
      const nb = (N - b) % N;
      const lr = (fr.re[b] + fr.re[nb]) / 2;
      const li = (fr.im[b] - fr.im[nb]) / 2;
      const rr = (fr.im[b] + fr.im[nb]) / 2;
      const ri = (fr.re[nb] - fr.re[b]) / 2;
      const cross = lr * rr + li * ri;
      const pw = lr * lr + li * li + rr * rr + ri * ri + 1e-12;
      const c = Math.max(0, (2 * cross) / pw);
      const v = Math.pow(c, cPow) * mH * band[b];
      masks.vocals[b] = v;
      masks.instrumental[b] = 1 - v;
      masks.drums[b] = mP * (1 - v * 0.5);
      masks.melody[b] = Math.max(0, mH - v);
    }
    // synthesis: mask the packed L+iR spectrum (masks are real & symmetric, so L and R stay separable)
    const off = t * HOP - N;
    for (const s of want) {
      const m = masks[s];
      for (let b = 0; b < N; b++) {
        const g = m[b < BINS ? b : N - b];
        oRe[b] = fr.re[b] * g;
        oIm[b] = fr.im[b] * g;
      }
      fft.run(oRe, oIm, true);
      const [L, R] = out[s]!;
      for (let i = 0; i < N; i++) {
        const sIdx = off + i;
        if (sIdx < 0 || sIdx >= len) continue;
        const w = win[i] / N;
        L[sIdx] += oRe[i] * w;
        R[sIdx] += oIm[i] * w;
      }
    }
  };

  for (let f = 0; f < frames + KT; f++) {
    if (f < frames) analyse(f);
    const t = f - KT;
    if (t >= 0) process(t);
    if (opts.onProgress && f % 64 === 0) opts.onProgress(f / (frames + KT));
  }
  opts.onProgress?.(1);
  return out;
}

/** 1 = identical channels (mono): centre extraction cannot separate vocals then. */
export function stereoCorrelation(left: Float32Array, right: Float32Array): number {
  let lr = 0;
  let ll = 0;
  let rr = 0;
  for (let i = 0; i < left.length; i += 7) {
    lr += left[i] * right[i];
    ll += left[i] * left[i];
    rr += right[i] * right[i];
  }
  return lr / Math.sqrt(ll * rr + 1e-12);
}
