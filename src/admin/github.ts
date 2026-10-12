// Admin side: writes the feed and publishes releases through the GitHub API.
// In the desktop app all requests (and the token) stay in the main process.
import { desktop, type GhResponse } from './desktop';
import { EMPTY_FEED, FEED, parseFeed, type Feed, type FeedMessage, type FeedUpdate } from './feed';
import { uid } from '../state/utils';

const WEB_TOKEN_KEY = 'goofy-admin-token';

export async function hasToken(): Promise<boolean> {
  if (desktop) return desktop.hasToken();
  try { return !!localStorage.getItem(WEB_TOKEN_KEY); } catch { return false; }
}

export async function setToken(token: string | null): Promise<boolean> {
  if (desktop) return desktop.setToken(token);
  try {
    if (token) localStorage.setItem(WEB_TOKEN_KEY, token);
    else localStorage.removeItem(WEB_TOKEN_KEY);
    return true;
  } catch {
    return false;
  }
}

async function gh(method: string, path: string, body?: unknown): Promise<GhResponse> {
  if (desktop) return desktop.gh(method, path, body);
  const token = localStorage.getItem(WEB_TOKEN_KEY);
  const r = await fetch(`https://api.github.com${path}`, {
    method,
    headers: {
      Accept: 'application/vnd.github+json',
      'X-GitHub-Api-Version': '2022-11-28',
      ...(token ? { Authorization: `Bearer ${token}` } : {}),
      ...(body ? { 'Content-Type': 'application/json' } : {}),
    },
    body: body ? JSON.stringify(body) : undefined,
  });
  let data: unknown = null;
  try { data = await r.json(); } catch { /* empty */ }
  return { ok: r.ok, status: r.status, data };
}

function errorText(r: GhResponse, what: string): string {
  const msg = (r.data as { message?: string } | null)?.message;
  if (r.status === 401) return `${what}: the GitHub token is invalid or expired.`;
  if (r.status === 403 || r.status === 404) return `${what}: the token has no write access to ${FEED.owner}/${FEED.repo} (needs “Contents: Read and write”).${msg ? ` (${msg})` : ''}`;
  return `${what} failed (${r.status})${msg ? `: ${msg}` : ''}`;
}

export async function testAccess(): Promise<{ ok: boolean; text: string }> {
  const r = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}`);
  if (!r.ok) return { ok: false, text: errorText(r, 'Connection') };
  const perms = (r.data as { permissions?: { push?: boolean } }).permissions;
  if (!perms?.push) return { ok: false, text: 'The token can read the repository but not write to it. Give it “Contents: Read and write”.' };
  return { ok: true, text: `Connected to ${FEED.owner}/${FEED.repo} with write access.` };
}

function toBase64Utf8(text: string): string {
  const bytes = new TextEncoder().encode(text);
  let s = '';
  for (let i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode(...bytes.subarray(i, i + 0x8000));
  return btoa(s);
}

async function readFeed(): Promise<{ feed: Feed; sha: string | null }> {
  const r = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}/contents/${FEED.path}?ref=${FEED.branch}`);
  if (r.status === 404) return { feed: { ...EMPTY_FEED }, sha: null };
  if (!r.ok) throw new Error(errorText(r, 'Reading the feed'));
  const d = r.data as { content: string; sha: string };
  const text = new TextDecoder().decode(Uint8Array.from(atob(d.content.replace(/\s/g, '')), (c) => c.charCodeAt(0)));
  return { feed: parseFeed(text) ?? { ...EMPTY_FEED }, sha: d.sha };
}

async function ensureFeedBranch() {
  const r = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}/branches/${FEED.branch}`);
  if (r.ok) return;
  // create the branch from the default branch head
  const repo = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}`);
  const def = (repo.data as { default_branch?: string }).default_branch ?? 'main';
  const ref = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}/git/ref/heads/${encodeURIComponent(def)}`);
  const sha = (ref.data as { object?: { sha?: string } }).object?.sha;
  if (!sha) throw new Error('Could not create the feed branch.');
  const c = await gh('POST', `/repos/${FEED.owner}/${FEED.repo}/git/refs`, { ref: `refs/heads/${FEED.branch}`, sha });
  if (!c.ok) throw new Error(errorText(c, 'Creating the feed branch'));
}

async function writeFeed(mutate: (f: Feed) => Feed, message: string): Promise<Feed> {
  await ensureFeedBranch();
  for (let attempt = 0; attempt < 3; attempt++) {
    const { feed, sha } = await readFeed();
    const next = mutate(feed);
    const r = await gh('PUT', `/repos/${FEED.owner}/${FEED.repo}/contents/${FEED.path}`, {
      message,
      content: toBase64Utf8(`${JSON.stringify(next, null, 2)}\n`),
      branch: FEED.branch,
      ...(sha ? { sha } : {}),
    });
    if (r.ok) return next;
    if (r.status !== 409) throw new Error(errorText(r, 'Saving the feed'));
  }
  throw new Error('The feed was changed at the same time – please try again.');
}

export async function loadFeedAsAdmin(): Promise<Feed> {
  return (await readFeed()).feed;
}

export async function publishMessage(m: Omit<FeedMessage, 'id' | 'date'>): Promise<FeedMessage> {
  const msg: FeedMessage = { ...m, id: uid('msg'), date: new Date().toISOString() };
  await writeFeed((f) => ({ ...f, messages: [msg, ...f.messages].slice(0, 50) }), `News: ${m.title}`);
  return msg;
}

export async function deleteMessage(id: string): Promise<void> {
  await writeFeed((f) => ({ ...f, messages: f.messages.filter((x) => x.id !== id) }), 'Remove news');
}

export async function clearUpdate(): Promise<void> {
  await writeFeed((f) => ({ ...f, update: null }), 'Withdraw update');
}

export interface PublishUpdateInput {
  version: string;
  notes: string;
  mandatory: boolean;
  setupFile?: File | null;
  portableFile?: File | null;
  link?: string;
  onStatus: (text: string) => void;
}

/** Creates the GitHub release, uploads the .exe files and announces the update in the feed. */
export async function publishUpdate(input: PublishUpdateInput): Promise<FeedUpdate> {
  const tag = `v${input.version.replace(/^v/i, '')}`;
  const assets: FeedUpdate['assets'] = {};
  if (input.link) assets.page = input.link;
  if (input.setupFile || input.portableFile) {
    if (!desktop) throw new Error('Uploading files only works in the desktop app. Paste a download link instead.');
    input.onStatus('Creating the GitHub release…');
    let rel = await gh('GET', `/repos/${FEED.owner}/${FEED.repo}/releases/tags/${tag}`);
    if (!rel.ok) {
      rel = await gh('POST', `/repos/${FEED.owner}/${FEED.repo}/releases`, { tag_name: tag, name: `Goofy Studio ${tag}`, body: input.notes, draft: false, prerelease: false });
      if (!rel.ok) throw new Error(errorText(rel, 'Creating the release'));
    }
    const release = rel.data as { upload_url: string; html_url: string; assets?: { name: string; browser_download_url: string }[] };
    assets.page = release.html_url;
    const up = async (file: File, kind: 'setup' | 'portable') => {
      const name = kind === 'setup' ? `GoofyStudio-${tag.slice(1)}-Setup.exe` : `GoofyStudio-${tag.slice(1)}-portable.exe`;
      const existing = release.assets?.find((a) => a.name === name);
      if (existing) return existing.browser_download_url;
      input.onStatus(`Uploading ${name}…`);
      const r = await desktop!.ghUpload(release.upload_url, desktop!.pathForFile(file), name);
      if (!r.ok) throw new Error(errorText(r, `Uploading ${name}`));
      return (r.data as { browser_download_url: string }).browser_download_url;
    };
    if (input.setupFile) assets.setup = await up(input.setupFile, 'setup');
    if (input.portableFile) assets.portable = await up(input.portableFile, 'portable');
  }
  const upd: FeedUpdate = { version: tag.slice(1), notes: input.notes, date: new Date().toISOString(), assets, mandatory: input.mandatory };
  input.onStatus('Announcing the update…');
  await writeFeed((f) => ({ ...f, update: upd }), `Update ${tag}`);
  return upd;
}
