// Main-process services for the admin panel and the self-updater.
const { app, ipcMain, net, safeStorage, shell } = require('electron');

// Chromium's network stack (respects the system proxy); Node's fetch is only used for streaming uploads.
const cfetch = (url, opts) => net.fetch(url, opts);
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');
const { Readable, Transform } = require('node:stream');
const { pipeline } = require('node:stream/promises');

const REPO_API = 'https://api.github.com';
const tokenFile = () => path.join(app.getPath('userData'), 'admin-token.bin');

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

/** Counts bytes flowing through a stream and reports progress to the window. */
function progressStream(win, id, total) {
  let done = 0;
  let last = 0;
  return new Transform({
    transform(chunk, _enc, cb) {
      done += chunk.length;
      const now = Date.now();
      if (now - last > 100 || done === total) {
        last = now;
        win?.webContents.send('progress', { id, done, total });
      }
      cb(null, chunk);
    },
  });
}

function register(getWindow) {
  ipcMain.handle('app:info', () => ({
    version: app.getVersion(),
    portable: !!process.env.PORTABLE_EXECUTABLE_FILE,
    platform: process.platform,
    exePath: process.env.PORTABLE_EXECUTABLE_FILE || process.execPath,
  }));

  ipcMain.handle('app:openExternal', (_e, url) => {
    if (/^https:\/\//.test(url)) return shell.openExternal(url);
  });

  ipcMain.handle('admin:hasToken', () => !!readToken());
  ipcMain.handle('admin:setToken', (_e, token) => writeToken(token));

  ipcMain.handle('net:text', async (_e, url) => {
    if (!/^https:\/\/(api\.github\.com|raw\.githubusercontent\.com)\//.test(url)) return { ok: false, status: 0, text: '' };
    try {
      const res = await cfetch(url, { headers: { 'User-Agent': 'GoofyStudio', Accept: 'application/vnd.github+json', 'Cache-Control': 'no-cache' }, cache: 'no-store' });
      return { ok: res.ok, status: res.status, text: await res.text() };
    } catch {
      return { ok: false, status: 0, text: '' };
    }
  });

  ipcMain.handle('gh:request', async (_e, method, apiPath, body) => {
    if (typeof apiPath !== 'string' || !apiPath.startsWith('/')) return { ok: false, status: 0, data: { message: 'bad path' } };
    try {
      const res = await cfetch(`${REPO_API}${apiPath}`, {
        method,
        headers: ghHeaders(body ? { 'Content-Type': 'application/json' } : {}),
        body: body ? JSON.stringify(body) : undefined,
      });
      return asResult(res);
    } catch (err) {
      return { ok: false, status: 0, data: { message: String(err) } };
    }
  });

  ipcMain.handle('gh:upload', async (_e, uploadUrl, filePath, name) => {
    try {
      const url = `${String(uploadUrl).replace(/\{.*\}$/, '')}?name=${encodeURIComponent(name)}`;
      if (!url.startsWith('https://uploads.github.com/')) throw new Error('bad upload url');
      const size = fs.statSync(filePath).size;
      const body = Readable.toWeb(fs.createReadStream(filePath).pipe(progressStream(getWindow(), 'upload', size)));
      const res = await fetch(url, {
        method: 'POST',
        headers: ghHeaders({ 'Content-Type': 'application/octet-stream', 'Content-Length': String(size) }),
        body,
        duplex: 'half',
      });
      return asResult(res);
    } catch (err) {
      return { ok: false, status: 0, data: { message: String(err) } };
    }
  });

  ipcMain.handle('update:download', async (_e, url, fileName, kind) => {
    try {
      if (!/^https:\/\/github\.com\//.test(url) && !/^https:\/\/objects\.githubusercontent\.com\//.test(url)) throw new Error('Only GitHub downloads are allowed');
      const safeName = path.basename(String(fileName)).replace(/[^\w.\- ]/g, '_');
      const dir = kind === 'portable' && process.env.PORTABLE_EXECUTABLE_FILE
        ? path.dirname(process.env.PORTABLE_EXECUTABLE_FILE)
        : app.getPath(kind === 'portable' ? 'downloads' : 'temp');
      const target = path.join(dir, safeName);
      const res = await cfetch(url, { headers: { 'User-Agent': 'GoofyStudio' }, redirect: 'follow' });
      if (!res.ok || !res.body) throw new Error(`Download failed (${res.status})`);
      const total = Number(res.headers.get('content-length')) || 0;
      const tmp = `${target}.part`;
      await pipeline(Readable.fromWeb(res.body), progressStream(getWindow(), 'update', total), fs.createWriteStream(tmp));
      fs.renameSync(tmp, target);
      return { ok: true, path: target };
    } catch (err) {
      return { ok: false, error: String(err && err.message ? err.message : err) };
    }
  });

  ipcMain.handle('update:install', (_e, filePath, kind) => {
    try {
      if (!fs.existsSync(filePath) || !/\.exe$/i.test(filePath)) return false;
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

module.exports = { register };
