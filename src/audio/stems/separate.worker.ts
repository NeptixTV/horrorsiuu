/// <reference lib="webworker" />
import { separate, type StemName } from './dsp';

self.onmessage = (e: MessageEvent<{ left: Float32Array; right: Float32Array; sampleRate: number; stems: StemName[]; vocalFocus: number }>) => {
  const { left, right, sampleRate, stems, vocalFocus } = e.data;
  try {
    const res = separate(left, right, sampleRate, {
      stems,
      vocalFocus,
      onProgress: (p) => (self as unknown as Worker).postMessage({ type: 'progress', p }),
    });
    const transfer: ArrayBuffer[] = [];
    for (const v of Object.values(res)) if (v) transfer.push(v[0].buffer as ArrayBuffer, v[1].buffer as ArrayBuffer);
    (self as unknown as Worker).postMessage({ type: 'done', result: res }, transfer);
  } catch (err) {
    (self as unknown as Worker).postMessage({ type: 'error', message: String(err) });
  }
};
