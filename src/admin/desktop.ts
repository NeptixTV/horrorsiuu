// Bridge to the Electron main process (only present in the desktop app).

export interface DesktopInfo {
  version: string;
  portable: boolean;
  platform: string;
  /** 'overlay': the app draws the title bar (Windows), 'native': the system does */
  titleBar?: 'overlay' | 'native';
}

export interface GhResponse {
  ok: boolean;
  status: number;
  data: unknown;
}

export interface GoofyDesktop {
  info(): Promise<DesktopInfo>;
  hasToken(): Promise<boolean>;
  setToken(token: string | null): Promise<boolean>;
  gh(method: string, path: string, body?: unknown): Promise<GhResponse>;
  /** `sha256` of the uploaded file (1.2+) – goes into the feed so clients can verify the download */
  ghUpload(uploadUrl: string, filePath: string, name: string): Promise<GhResponse & { sha256?: string }>;
  fetchText(url: string): Promise<{ ok: boolean; status: number; text: string }>;
  pathForFile(file: File): string;
  download(url: string, fileName: string, kind: 'setup' | 'portable', sha256?: string): Promise<{ ok: boolean; path?: string; error?: string; verified?: boolean }>;
  install(path: string, kind: 'setup' | 'portable'): Promise<boolean>;
  openExternal(url: string): Promise<void>;
  onProgress(cb: (p: { id: string; done: number; total: number }) => void): () => void;
  /** UI zoom of the desktop window (Settings → Design → Scale); missing in versions < 1.2 */
  setZoom?(factor: number): Promise<void> | void;
  /** colours of the window-buttons area (1.3+) */
  setTitleBarColors?(color: string, symbol: string): Promise<boolean>;
}

export const desktop: GoofyDesktop | null = (globalThis as unknown as { goofyDesktop?: GoofyDesktop }).goofyDesktop ?? null;

export const isDesktop = !!desktop;
