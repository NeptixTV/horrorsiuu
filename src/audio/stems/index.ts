import SeparateWorker from './separate.worker?worker&inline';
import { separate, stereoCorrelation, type StemName, type StemResult } from './dsp';

export type { StemName, StemResult };
export { stereoCorrelation };

/** Runs the separation in a worker (falls back to the main thread). */
export function separateStems(buffer: AudioBuffer, stems: StemName[], vocalFocus: number, onProgress: (p: number) => void): Promise<StemResult> {
  const left = buffer.getChannelData(0).slice();
  const right = (buffer.numberOfChannels > 1 ? buffer.getChannelData(1) : buffer.getChannelData(0)).slice();
  return new Promise((resolve, reject) => {
    let worker: Worker;
    try {
      worker = new SeparateWorker();
    } catch {
      try {
        resolve(separate(left, right, buffer.sampleRate, { stems, vocalFocus, onProgress }));
      } catch (e) {
        reject(e);
      }
      return;
    }
    worker.onmessage = (e) => {
      const m = e.data;
      if (m.type === 'progress') onProgress(m.p);
      else if (m.type === 'done') { worker.terminate(); resolve(m.result); }
      else if (m.type === 'error') { worker.terminate(); reject(new Error(m.message)); }
    };
    worker.onerror = (e) => { worker.terminate(); reject(new Error(e.message)); };
    worker.postMessage({ left, right, sampleRate: buffer.sampleRate, stems, vocalFocus }, [left.buffer, right.buffer]);
  });
}
