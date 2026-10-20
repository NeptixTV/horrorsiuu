// Main-process services for the admin panel and the self-updater.
//
// Security rules (the renderer is treated as untrusted):
// - every call must come from the app's own page in the main window
// - the GitHub token never leaves this process and is only sent to the API of
//   this app's repository
// - uploads: only .exe release files of a sane size; downloads: only release
//   files of this repository, checked against the SHA-256 from the feed
// - only files that this process downloaded itself can be started
const { app, ipcMain, net, safeStorage, shell } = require('electron');

// Chromium's network stack (respects the system proxy); Node's fetch is only used for streaming uploads.
const cfetch = (url, opts) => net.fetch(url, opts);
const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { Readable, Transform } = require('node:stream');
const { pipeline } = require('node:stream/promises');
const pkg = require('../package.json');

const REPO_API = 'https://api.github.com';
// "owner/repo" of the release channel, taken from package.json → homepage
const REPO = (/github\.com\/([\w.-]+\/[\w.-]+)/.exec(pkg.homepage || '') || [])[1] || 'NeptixTV/horrorsiuu';
const APP_ORIGIN = 'app://goofy';
const MAX_UPLOAD = 1.5 * 1024 * 1024 * 1024;
const MAX_DOWNLOAD = 1.5 * 1024 * 1024 * 1024;
const RELEASE_FILE = /^GoofyStudio-[\w.-]{1,40}\.exe$/;
const tokenFile = () => path.join(app.getPath('userData'), 'admin-token.bin');
const downloaded = new Map(); // path → kind, files this process downloaded

function readToken() {
  try {
    const raw = fs.readFileSync(tokenFile());
    return safeStorage.isEncryptionAvailable() ? safeStorage.decryptString(raw) : raw.toString('utf8');
  } catch {
    return null;
  }
}

function writeToken(token) {
  if (!token) {
    try { fs.unlinkSync(tokenFile()); } catch { /* not there */ }
    return true;
  }
  if (typeof token !== 'string' || token.length > 512 || !/^[\w-]+$/.test(token)) return false;
  const data = safeStorage.isEncryptionAvailable() ? safeStorage.encryptString(token) : Buffer.from(token, 'utf8');
  fs.writeFileSync(tokenFile(), data, { mode: 0o600 });
  return true;
}

function ghHeaders(extra = {}) {
  const token = readToken();
  return {
    Accept: 'application/vnd.github+json',
    'X-GitHub-Api-Version': '2022-11-28',
    'User-Agent': 'GoofyStudio',
    ...(token ? { Authorization: `Bearer ${token}` } : {}),
    ...extra,
  };
}

async function asResult(res) {
  let data = null;
  const text = await res.text();
  try { data = text ? JSON.parse(text) : null; } catch { data = { message: text.slice(0, 300) }; }
  return { ok: res.ok, status: res.status, data };
}

/** Counts bytes flowing through a stream, hashes them and reports progress to the window. */
function progressStream(win, id, total, hash, limit = Infinity) {
  let done = 0;
  let last = 0;
  return new Transform({
    transform(chunk, _enc, cb) {
      done += chunk.length;
      if (done > limit) return cb(new Error('File too large'));
      hash?.update(chunk);
      const now = Date.now();
      if (now - last > 100 || done === total) {
        last = now;
        if (win && !win.isDestroyed()) win.webContents.send('progress', { id, done, total });
      }
      cb(null, chunk);
    },
  });
}

const isHttpsUrl = (u) => {
  try { return new URL(u).protocol === 'https:'; } catch { return false; }
};

function register(getWindow) {
  /** Registers a handler that only answers the app's own page in the main window. */
  const handle = (channel, fn) => ipcMain.handle(channel, (e, ...args) => {
    const win = getWindow();
    const url = e.senderFrame?.url ?? '';
    if (!win || e.sender !== win.webContents || !(url === APP_ORIGIN || url.startsWith(`${APP_ORIGIN}/`))) {
      throw new Error('Blocked: request from an unknown page');
    }
    return fn(...args);
  });

  handle('app:info', () => ({
    version: app.getVersion(),
    portable: !!process.env.PORTABLE_EXECUTABLE_FILE,
    platform: process.platform,
  }));

  handle('app:openExternal', (url) => {
    if (isHttpsUrl(url)) return shell.openExternal(url);
    return false;
  });

  handle('admin:hasToken', () => !!readToken());
  handle('admin:setToken', (token) => writeToken(token));

  handle('net:text', async (url) => {
    const allowed = [`https://api.github.com/repos/${REPO}/`, `https://raw.githubusercontent.com/${REPO}/`];
    if (typeof url !== 'string' || !allowed.some((a) => url.startsWith(a))) return { ok: false, status: 0, text: '' };
    try {
      const res = await cfetch(url, { headers: { 'User-Agent': 'GoofyStudio', Accept: 'application/vnd.github+json', 'Cache-Control': 'no-cache' }, cache: 'no-store' });
      const text = await res.text();
      return { ok: res.ok, status: res.status, text: text.slice(0, 2 * 1024 * 1024) };
    } catch {
      return { ok: false, status: 0, text: '' };
    }
  });

  handle('gh:request', async (method, apiPath, body) => {
    // the admin panel only ever talks to this app's repository
    const repoPath = `/repos/${REPO}`;
    if (typeof apiPath !== 'string' || !(apiPath === repoPath || apiPath.startsWith(`${repoPath}/`) || apiPath.startsWith(`${repoPath}?`)) || apiPath.includes('..')) {
      return { ok: false, status: 0, data: { message: 'bad path' } };
    }
    if (!['GET', 'POST', 'PUT', 'PATCH'].includes(method)) return { ok: false, status: 0, data: { message: 'bad method' } };
    const json = body ? JSON.stringify(body) : undefined;
    if (json && json.length > 5 * 1024 * 1024) return { ok: false, status: 0, data: { message: 'request too large' } };
    try {
      const res = await cfetch(`${REPO_API}${apiPath}`, {
        method,
        headers: ghHeaders(json ? { 'Content-Type': 'application/json' } : {}),
        body: json,
      });
      return asResult(res);
    } catch (err) {
      return { ok: false, status: 0, data: { message: String(err) } };
    }
  });

  handle('gh:upload', async (uploadUrl, filePath, name) => {
    try {
      const base = String(uploadUrl).replace(/\{.*\}$/, '');
      if (!base.startsWith(`https://uploads.github.com/repos/${REPO}/releases/`)) throw new Error('bad upload url');
      if (typeof name !== 'string' || !RELEASE_FILE.test(name)) throw new Error('bad file name');
      // only real .exe files are uploaded – never an arbitrary file of this computer
      if (typeof filePath !== 'string' || !/\.exe$/i.test(filePath)) throw new Error('Only .exe files can be uploaded');
      const st = fs.statSync(filePath);
      if (!st.isFile() || st.size <= 0 || st.size > MAX_UPLOAD) throw new Error('The file is empty or too large');
      const fd = fs.openSync(filePath, 'r');
      const head = Buffer.alloc(2);
      fs.readSync(fd, head, 0, 2, 0);
      fs.closeSync(fd);
      if (head.toString('latin1') !== 'MZ') throw new Error('This is not a Windows program (.exe)');
      const hash = crypto.createHash('sha256');
      const url = `${base}?name=${encodeURIComponent(name)}`;
      const stream = Readable.toWeb(fs.createReadStream(filePath).pipe(progressStream(getWindow(), 'upload', st.size, hash, MAX_UPLOAD)));
      const res = await fetch(url, {
        method: 'POST',
        headers: ghHeaders({ 'Content-Type': 'application/octet-stream', 'Content-Length': String(st.size) }),
        body: stream,
        duplex: 'half',
      });
      const result = await asResult(res);
      return { ...result, sha256: res.ok ? hash.digest('hex') : undefined };
    } catch (err) {
      return { ok: false, status: 0, data: { message: String(err && err.message ? err.message : err) } };
    }
  });

  handle('update:download', async (url, fileName, kind, sha256) => {
    let tmp = null;
    try {
      if (typeof url !== 'string' || !url.startsWith(`https://github.com/${REPO}/releases/download/`)) throw new Error('Only release files of Goofy Studio can be downloaded');
      const safeName = path.basename(String(fileName)).replace(/[^\w.\- ]/g, '_');
      if (!RELEASE_FILE.test(safeName)) throw new Error('Unexpected update file name');
      const expected = typeof sha256 === 'string' && /^[0-9a-f]{64}$/i.test(sha256) ? sha256.toLowerCase() : null;
      // portable: next to the running .exe; installer: a fresh private temp folder
      const dir = kind === 'portable' && process.env.PORTABLE_EXECUTABLE_FILE
        ? path.dirname(process.env.PORTABLE_EXECUTABLE_FILE)
        : kind === 'portable'
          ? app.getPath('downloads')
          : fs.mkdtempSync(path.join(app.getPath('temp'), 'goofy-update-'));
      const target = path.join(dir, safeName);
      const res = await cfetch(url, { headers: { 'User-Agent': 'GoofyStudio' }, redirect: 'follow' });
      if (!res.ok || !res.body) throw new Error(`Download failed (${res.status})`);
      const total = Number(res.headers.get('content-length')) || 0;
      if (total > MAX_DOWNLOAD) throw new Error('Update file too large');
      const hash = crypto.createHash('sha256');
      tmp = `${target}.part`;
      await pipeline(Readable.fromWeb(res.body), progressStream(getWindow(), 'update', total, hash, MAX_DOWNLOAD), fs.createWriteStream(tmp, { mode: 0o700 }));
      const actual = hash.digest('hex');
      if (expected && actual !== expected) throw new Error('The downloaded file is damaged or was changed (checksum mismatch) – it was deleted');
      fs.renameSync(tmp, target);
      tmp = null;
      downloaded.set(target, kind === 'setup' ? 'setup' : 'portable');
      return { ok: true, path: target, verified: !!expected };
    } catch (err) {
      if (tmp) { try { fs.unlinkSync(tmp); } catch { /* ignore */ } }
      return { ok: false, error: String(err && err.message ? err.message : err) };
    }
  });

  handle('update:install', (filePath) => {
    try {
      // only an update this app downloaded itself in this session
      const kind = downloaded.get(filePath);
      if (!kind || !fs.existsSync(filePath)) return false;
      if (process.platform !== 'win32') {
        shell.showItemInFolder(filePath);
        return true;
      }
      const child = spawn(filePath, kind === 'setup' ? [] : ['--after-update'], { detached: true, stdio: 'ignore' });
      child.unref();
      setTimeout(() => app.quit(), 400);
      return true;
    } catch {
      return false;
    }
  });
}

module.exports = { register, REPO };
