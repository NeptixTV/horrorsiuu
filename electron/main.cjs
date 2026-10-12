// Electron main process: wraps the Goofy Studio web build as a native desktop app.
const { app, BrowserWindow, Menu, net, protocol, session, shell } = require('electron');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const ipc = require('./ipc.cjs');

const DIST = path.join(__dirname, '..', 'dist');

// A privileged custom scheme gives the app a stable, secure origin
// (needed for IndexedDB persistence, AudioWorklet and the microphone).
protocol.registerSchemesAsPrivileged([
  { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true, corsEnabled: true } },
]);

// Audio may start without a click inside the desktop app.
app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');

// One window only. A freshly downloaded update ("--after-update") waits until
// the old version has closed before it takes over.
async function acquireLock() {
  if (app.requestSingleInstanceLock()) return true;
  if (!process.argv.includes('--after-update')) return false;
  for (let i = 0; i < 40; i++) {
    await new Promise((r) => setTimeout(r, 250));
    if (app.requestSingleInstanceLock()) return true;
  }
  return false;
}

let mainWindow = null;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1600,
    height: 960,
    minWidth: 1024,
    minHeight: 640,
    backgroundColor: '#030304',
    title: 'Goofy Studio',
    icon: path.join(__dirname, 'icon.png'),
    autoHideMenuBar: true,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, 'preload.cjs'),
      backgroundThrottling: false,
    },
  });
  mainWindow.maximize();
  mainWindow.loadURL('app://goofy/index.html');
  // links open in the system browser
  mainWindow.webContents.setWindowOpenHandler(({ url }) => {
    if (/^https?:/.test(url)) void shell.openExternal(url);
    return { action: 'deny' };
  });
  mainWindow.on('closed', () => { mainWindow = null; });
}

// Worklets and the scheduler worker are loaded from blob: URLs.
const CSP = [
  "default-src 'self'",
  "script-src 'self' blob:",
  "worker-src 'self' blob:",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data: blob:",
  "font-src 'self' data:",
  "media-src 'self' data: blob:",
  "connect-src 'self' data: blob: https://api.github.com https://raw.githubusercontent.com",
].join('; ');

const ALLOWED = new Set(['media', 'midi', 'midiSysex', 'speaker-selection', 'clipboard-sanitized-write', 'fullscreen']);

app.whenReady().then(async () => {
  if (!(await acquireLock())) {
    app.quit();
    return;
  }
  protocol.handle('app', async (request) => {
    const { pathname } = new URL(request.url);
    const file = path.normalize(path.join(DIST, decodeURIComponent(pathname)));
    if (!file.startsWith(DIST)) return new Response('Forbidden', { status: 403 });
    const res = await net.fetch(pathToFileURL(file).toString());
    if (!file.endsWith('.html')) return res;
    const headers = new Headers(res.headers);
    headers.set('Content-Type', 'text/html; charset=utf-8');
    headers.set('Content-Security-Policy', CSP);
    return new Response(res.body, { status: res.status, headers });
  });
  session.defaultSession.setPermissionRequestHandler((_wc, permission, callback) => callback(ALLOWED.has(permission)));
  session.defaultSession.setPermissionCheckHandler((_wc, permission) => ALLOWED.has(permission));
  Menu.setApplicationMenu(null);
  ipc.register(() => mainWindow);
  createWindow();
});

app.on('second-instance', () => {
  if (mainWindow) {
    if (mainWindow.isMinimized()) mainWindow.restore();
    mainWindow.focus();
  }
});

app.on('window-all-closed', () => app.quit());
