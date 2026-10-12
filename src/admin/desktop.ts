// Bridge to the Electron main process (only present in the desktop app).

export interface DesktopInfo {
  version: string;
  portable: boolean;
  platform: string;
  exePath: string;
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
  ghUpload(uploadUrl: string, filePath: string, name: string): Promise<GhResponse>;
  fetchText(url: string): Promise<{ ok: boolean; status: number; text: string }>;
  pathForFile(file: File): string;
  download(url: string, fileName: string, kind: 'setup' | 'portable'): Promise<{ ok: boolean; path?: string; error?: string }>;
  install(path: string, kind: 'setup' | 'portable'): Promise<boolean>;
  openExternal(url: string): Promise<void>;
  onProgress(cb: (p: { id: string; done: number; total: number }) => void): () => void;
}

export const desktop: GoofyDesktop | null = (globalThis as unknown as { goofyDesktop?: GoofyDesktop }).goofyDesktop ?? null;

export const isDesktop = !!desktop;
