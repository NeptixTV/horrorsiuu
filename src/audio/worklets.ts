// Loads the AudioWorklet processors into a context. The processor sources are
// bundled as strings and registered via Blob URLs so they also work in the
// single-file build.
import recorderSrc from './worklets/recorder.worklet.js?raw';
import pitchSrc from './worklets/pitch.worklet.js?raw';

const loaded = new WeakMap<BaseAudioContext, Promise<boolean>>();
let urls: string[] | null = null;

function moduleUrls(): string[] {
  if (!urls) {
    urls = [recorderSrc, pitchSrc].map((src) => URL.createObjectURL(new Blob([src], { type: 'application/javascript' })));
  }
  return urls;
}

/** Resolves to true when worklets are available in this context. */
export function loadWorklets(ctx: BaseAudioContext): Promise<boolean> {
  let p = loaded.get(ctx);
  if (!p) {
    p = (async () => {
      if (!ctx.audioWorklet) return false;
      try {
        for (const url of moduleUrls()) await ctx.audioWorklet.addModule(url);
        return true;
      } catch (err) {
        console.warn('[GoofyStudio] AudioWorklet unavailable', err);
        return false;
      }
    })();
    loaded.set(ctx, p);
  }
  return p;
}

const ready = new WeakSet<BaseAudioContext>();
export function markWorkletsReady(ctx: BaseAudioContext) {
  ready.add(ctx);
}
export function workletsReady(ctx: BaseAudioContext): boolean {
  return ready.has(ctx);
}
