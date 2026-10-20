// Exposes a small, explicit API to the renderer (contextIsolation stays on).
const { contextBridge, ipcRenderer, webFrame, webUtils } = require('electron');

contextBridge.exposeInMainWorld('goofyDesktop', {
  info: () => ipcRenderer.invoke('app:info'),
  hasToken: () => ipcRenderer.invoke('admin:hasToken'),
  setToken: (token) => ipcRenderer.invoke('admin:setToken', token),
  gh: (method, path, body) => ipcRenderer.invoke('gh:request', method, path, body),
  ghUpload: (uploadUrl, filePath, name) => ipcRenderer.invoke('gh:upload', uploadUrl, filePath, name),
  fetchText: (url) => ipcRenderer.invoke('net:text', url),
  pathForFile: (file) => webUtils.getPathForFile(file),
  download: (url, fileName, kind) => ipcRenderer.invoke('update:download', url, fileName, kind),
  install: (path, kind) => ipcRenderer.invoke('update:install', path, kind),
  openExternal: (url) => ipcRenderer.invoke('app:openExternal', url),
  // interface size (Settings → Design); limited to a sane range
  setZoom: (factor) => {
    const f = Number(factor);
    if (Number.isFinite(f)) webFrame.setZoomFactor(Math.min(1.6, Math.max(0.7, f)));
  },
  onProgress: (cb) => {
    const fn = (_e, p) => cb(p);
    ipcRenderer.on('progress', fn);
    return () => ipcRenderer.removeListener('progress', fn);
  },
});
