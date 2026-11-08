// Electron main process: wraps the Goofy Studio web build as a native desktop app.
const { app, BrowserWindow, Menu, nativeTheme, net, protocol, screen, session, shell } = require('electron');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const ipc = require('./ipc.cjs');

const DIST = path.join(__dirname, '..', 'dist');
const APP_ORIGIN = 'app://goofy';
const isAppUrl = (url) => typeof url === 'string' && (url === APP_ORIGIN || url.startsWith(`${APP_ORIGIN}/`));

// A privileged custom scheme gives the app a stable, secure origin
// (needed for IndexedDB persistence, AudioWorklet and the microphone).
protocol.registerSchemesAsPrivileged([
  { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true, stream: true, corsEnabled: true } },
]);

// Audio may start without a click inside the desktop app.
app.commandLine.appendSwitch('autoplay-policy', 'no-user-gesture-required');
// A music program must never be slowed down because its window is in the
// background or covered by another program (browser, Discord …): Chromium
// would lower the priority of the page and throttle its timers, and the beat
// started to stutter as soon as another window was in front.
app.commandLine.appendSwitch('disable-renderer-backgrounding');
app.commandLine.appendSwitch('disable-background-timer-throttling');
app.commandLine.appendSwitch('disable-backgrounding-occluded-windows');
app.commandLine.appendSwitch('disable-features', 'CalculateNativeWinOcclusion,IntensiveWakeUpThrottling');

// Windows: the processes that make the sound (the page with the audio engine
// and Chromium's audio service) get "above normal" priority, like real
// audio programs – other programs can then not take their CPU time away so
// easily. Checked again every few seconds (Chromium may restart a process).
function raiseAudioPriority() {
  if (process.platform !== 'win32') return;
  const { priority } = os.constants;
  for (const m of app.getAppMetrics()) {
    const audio = m.type === 'Utility' && /audio/i.test(`${m.serviceName ?? ''} ${m.name ?? ''}`);
    if (m.type !== 'Tab' && !audio) continue;
    try {
      // lower numbers are higher priorities; never lower one that is already higher
      if (os.getPriority(m.pid) > priority.PRIORITY_ABOVE_NORMAL) os.setPriority(m.pid, priority.PRIORITY_ABOVE_NORMAL);
    } catch { /* not allowed – keep the normal priority */ }
  }
}

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

// size and position of the window are remembered like in every desktop program
const stateFile = () => path.join(app.getPath('userData'), 'window-state.json');
function loadWindowState() {
  try {
    const s = JSON.parse(fs.readFileSync(stateFile(), 'utf8'));
    const ok = (n) => Number.isFinite(n);
    if (!ok(s.x) || !ok(s.y) || !ok(s.width) || !ok(s.height)) return null;
    // only if it is still on a connected screen
    const visible = screen.getAllDisplays().some((d) => {
      const a = d.workArea;
      return s.x < a.x + a.width - 80 && s.x + s.width > a.x + 80 && s.y >= a.y - 10 && s.y < a.y + a.height - 80;
    });
    return visible ? { x: s.x, y: s.y, width: Math.max(1024, s.width), height: Math.max(640, s.height), maximized: !!s.maximized } : null;
  } catch {
    return null;
  }
}
function saveWindowState(win) {
  try {
    const b = win.getNormalBounds();
    fs.writeFileSync(stateFile(), JSON.stringify({ ...b, maximized: win.isMaximized() }));
  } catch { /* not important */ }
}

function createWindow() {
  const st = loadWindowState();
  const overlay = process.platform === 'win32';
  mainWindow = new BrowserWindow({
    width: st?.width ?? 1600,
    height: st?.height ?? 960,
    ...(st ? { x: st.x, y: st.y } : {}),
    minWidth: 1024,
    minHeight: 640,
    backgroundColor: '#121417',
    title: 'Goofy Studio',
    icon: path.join(__dirname, 'icon.png'),
    autoHideMenuBar: true,
    // Windows: no white system title bar – the app draws a dark one and
    // Windows puts its minimise / maximise / close buttons on top of it
    ...(overlay ? { titleBarStyle: 'hidden', titleBarOverlay: { color: '#16191d', symbolColor: '#d3d9df', height: 30 } } : {}),
    show: false,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      preload: path.join(__dirname, 'preload.cjs'),
      backgroundThrottling: false,
      // Chromium's spellchecker would download dictionaries from a Google CDN – no hidden connections
      spellcheck: false,
    },
  });
  if (!st || st.maximized) mainWindow.maximize();
  // shown when the first frame is painted – no white flash on start
  mainWindow.once('ready-to-show', () => mainWindow?.show());
  mainWindow.webContents.on('did-finish-load', raiseAudioPriority);
  mainWindow.loadURL(`${APP_ORIGIN}/index.html`);
  // no pinch-zoom of the whole page (the app has its own zoom)
  mainWindow.webContents.on('did-finish-load', () => { void mainWindow?.webContents.setVisualZoomLevelLimits(1, 1); });
  mainWindow.on('close', () => { if (mainWindow) saveWindowState(mainWindow); });
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
  // GitHub (news, updates, admin) is reached through the main process only
  "connect-src 'self' data: blob:",
  "object-src 'none'",
  "base-uri 'self'",
  "form-action 'none'",
].join('; ');

const ALLOWED = new Set(['media', 'midi', 'midiSysex', 'speaker-selection', 'clipboard-sanitized-write', 'fullscreen']);

// Every web contents (window, worker frames …) stays on the app's own pages:
// no navigation to other sites (they would get the preload API), no
// <webview>, and new windows only as links in the system browser.
app.on('web-contents-created', (_e, contents) => {
  const block = (ev, url) => { if (!isAppUrl(url)) ev.preventDefault(); };
  contents.on('will-navigate', block);
  contents.on('will-redirect', block);
  contents.on('will-attach-webview', (ev) => ev.preventDefault());
  contents.setWindowOpenHandler(({ url }) => {
    try {
      if (new URL(url).protocol === 'https:') void shell.openExternal(url);
    } catch { /* not a URL */ }
    return { action: 'deny' };
  });
});

app.whenReady().then(async () => {
  if (!(await acquireLock())) {
    app.quit();
    return;
  }
  protocol.handle('app', async (request) => {
    let file;
    try {
      const url = new URL(request.url);
      if (url.host !== 'goofy') return new Response('Not found', { status: 404 });
      file = path.normalize(path.join(DIST, decodeURIComponent(url.pathname)));
    } catch {
      return new Response('Bad request', { status: 400 });
    }
    // must stay inside dist/ ("dist-other/…" is not inside "dist")
    if (file !== DIST && !file.startsWith(DIST + path.sep)) return new Response('Forbidden', { status: 403 });
    let res;
    try {
      res = await net.fetch(pathToFileURL(file).toString());
    } catch {
      return new Response('Not found', { status: 404 });
    }
    if (!file.endsWith('.html')) return res;
    const headers = new Headers(res.headers);
    headers.set('Content-Type', 'text/html; charset=utf-8');
    headers.set('Content-Security-Policy', CSP);
    return new Response(res.body, { status: res.status, headers });
  });
  // microphone, MIDI, output device selection … only for the app's own pages
  session.defaultSession.setPermissionRequestHandler((wc, permission, callback, details) =>
    callback(ALLOWED.has(permission) && isAppUrl(details?.requestingUrl || wc.getURL())));
  session.defaultSession.setPermissionCheckHandler((_wc, permission, requestingOrigin) => ALLOWED.has(permission) && isAppUrl(requestingOrigin));
  // no spellchecker: it would download Hunspell dictionaries from a Google CDN in the background
  session.defaultSession.setSpellCheckerEnabled(false);
  session.defaultSession.setSpellCheckerLanguages([]);
  Menu.setApplicationMenu(null);
  // dark scrollbars, file dialogs and system pop-ups everywhere
  nativeTheme.themeSource = 'dark';
  ipc.register(() => mainWindow);
  createWindow();
  if (process.platform === 'win32') setInterval(raiseAudioPriority, 10000).unref();
});

app.on('second-instance', () => {
  if (mainWindow) {
    if (mainWindow.isMinimized()) mainWindow.restore();
    mainWindow.focus();
  }
});

app.on('window-all-closed', () => app.quit());
