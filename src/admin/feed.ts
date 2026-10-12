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
  assets: { setup?: string; portable?: string; page?: string };
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

export function parseFeed(text: string): Feed | null {
  try {
    const j = JSON.parse(text);
    if (!j || typeof j !== 'object') return null;
    return {
      version: 1,
      messages: Array.isArray(j.messages) ? j.messages.filter((m: FeedMessage) => m && m.id && m.title !== undefined) : [],
      update: j.update && j.update.version ? j.update : null,
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
