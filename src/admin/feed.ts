// News + update feed shared by all installed apps. The feed is a JSON file on
// the "goofy-feed" branch of the public GitHub repository; release files are
// GitHub Release assets. Everyone can read it, only the admin (with a GitHub
// token) can write it.
import { desktop } from './desktop';

export const FEED = { owner: 'NeptixTV', repo: 'horrorsiuu', branch: 'goofy-feed', path: 'feed.json' };

export type MessageKind = 'info' | 'warning' | 'party';

export interface FeedMessage {
  id: string;
  title: string;
  body: string;
  kind: MessageKind;
  date: string;
}

export interface FeedUpdate {
  version: string;
  notes: string;
  date: string;
  /** download links; the *Sha256 fields (1.2+) let the app verify the downloaded file */
  assets: { setup?: string; portable?: string; page?: string; setupSha256?: string; portableSha256?: string };
  mandatory?: boolean;
}

export interface Feed {
  version: 1;
  messages: FeedMessage[];
  update: FeedUpdate | null;
}

export const EMPTY_FEED: Feed = { version: 1, messages: [], update: null };

export const APP_VERSION: string = typeof __APP_VERSION__ === 'string' ? __APP_VERSION__ : '0.0.0';

/** -1 if a < b, 0 if equal, 1 if a > b (semantic versions, "v" prefix allowed). */
export function compareVersions(a: string, b: string): number {
  const pa = a.replace(/^v/i, '').split(/[.-]/).map((x) => parseInt(x, 10) || 0);
  const pb = b.replace(/^v/i, '').split(/[.-]/).map((x) => parseInt(x, 10) || 0);
  for (let i = 0; i < Math.max(pa.length, pb.length, 3); i++) {
    const d = (pa[i] ?? 0) - (pb[i] ?? 0);
    if (d !== 0) return d > 0 ? 1 : -1;
  }
  return 0;
}

const clip = (v: unknown, max: number) => (typeof v === 'string' ? v.slice(0, max) : '');
const httpsUrl = (v: unknown) => {
  if (typeof v !== 'string' || v.length > 2000) return undefined;
  try {
    return new URL(v).protocol === 'https:' ? v : undefined;
  } catch {
    return undefined;
  }
};
const sha = (v: unknown) => (typeof v === 'string' && /^[0-9a-f]{64}$/i.test(v) ? v.toLowerCase() : undefined);

/** Only well-formed values from the feed reach the UI (https links only – no javascript: or file: URLs). */
function cleanUpdate(u: unknown): FeedUpdate | null {
  if (!u || typeof u !== 'object') return null;
  const x = u as Record<string, unknown>;
  const version = clip(x.version, 40);
  if (!/^v?\d+(\.\d+){0,3}([-.][\w.]+)?$/.test(version)) return null;
  const a = (x.assets && typeof x.assets === 'object' ? x.assets : {}) as Record<string, unknown>;
  const assets: FeedUpdate['assets'] = {};
  const setup = httpsUrl(a.setup);
  const portable = httpsUrl(a.portable);
  const page = httpsUrl(a.page);
  if (setup) assets.setup = setup;
  if (portable) assets.portable = portable;
  if (page) assets.page = page;
  const ss = sha(a.setupSha256);
  const ps = sha(a.portableSha256);
  if (ss) assets.setupSha256 = ss;
  if (ps) assets.portableSha256 = ps;
  return { version, notes: clip(x.notes, 20000), date: clip(x.date, 40), assets, mandatory: x.mandatory === true };
}

export function parseFeed(text: string): Feed | null {
  try {
    const j = JSON.parse(text);
    if (!j || typeof j !== 'object') return null;
    return {
      version: 1,
      messages: Array.isArray(j.messages)
        ? (j.messages as unknown[]).slice(0, 100).flatMap((m): FeedMessage[] => {
          if (!m || typeof m !== 'object') return [];
          const x = m as Record<string, unknown>;
          const id = clip(x.id, 80);
          if (!id || x.title === undefined) return [];
          const kind: MessageKind = x.kind === 'warning' || x.kind === 'party' ? x.kind : 'info';
          return [{ id, title: clip(x.title, 200), body: clip(x.body, 20000), kind, date: clip(x.date, 40) }];
        })
        : [],
      update: cleanUpdate(j.update),
    };
  } catch {
    return null;
  }
}

const RAW = () => `https://raw.githubusercontent.com/${FEED.owner}/${FEED.repo}/${FEED.branch}/${FEED.path}`;
const API = () => `https://api.github.com/repos/${FEED.owner}/${FEED.repo}/contents/${FEED.path}?ref=${FEED.branch}`;

function decodeBase64Utf8(b64: string): string {
  const bin = atob(b64.replace(/\s/g, ''));
  const bytes = Uint8Array.from(bin, (c) => c.charCodeAt(0));
  return new TextDecoder().decode(bytes);
}

/** Loads the newest feed (API first – always fresh –, raw file as fallback). */
export async function fetchFeed(): Promise<Feed | null> {
  const tryText = async (url: string, accept?: string): Promise<string | null> => {
    try {
      if (desktop) {
        const r = await desktop.fetchText(url);
        return r.ok ? r.text : null;
      }
      const r = await fetch(url, { cache: 'no-store', headers: accept ? { Accept: accept } : undefined });
      return r.ok ? await r.text() : null;
    } catch {
      return null;
    }
  };
  const api = await tryText(`${API()}&t=${Date.now()}`, 'application/vnd.github+json');
  if (api) {
    try {
      const j = JSON.parse(api);
      if (j.content) {
        const f = parseFeed(decodeBase64Utf8(j.content));
        if (f) return f;
      }
    } catch { /* fall through */ }
  }
  const raw = await tryText(`${RAW()}?t=${Date.now()}`);
  return raw ? parseFeed(raw) : null;
}

// ---------- what this user has already seen ----------
const SEEN_KEY = 'goofy-seen-messages';
const SKIP_KEY = 'goofy-skip-update';

export function seenMessages(): Set<string> {
  try {
    return new Set(JSON.parse(localStorage.getItem(SEEN_KEY) ?? '[]'));
  } catch {
    return new Set();
  }
}

export function markSeen(ids: string[]) {
  const s = seenMessages();
  for (const id of ids) s.add(id);
  try { localStorage.setItem(SEEN_KEY, JSON.stringify([...s].slice(-500))); } catch { /* ignore */ }
}

export function skippedVersion(): string | null {
  try { return localStorage.getItem(SKIP_KEY); } catch { return null; }
}

export function skipVersion(v: string | null) {
  try {
    if (v) localStorage.setItem(SKIP_KEY, v);
    else localStorage.removeItem(SKIP_KEY);
  } catch { /* ignore */ }
}
