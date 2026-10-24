import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { viteSingleFile } from 'vite-plugin-singlefile';
import pkg from './package.json' with { type: 'json' };
import fs from 'node:fs';
import { thirdPartyNotices } from './scripts/notices.mjs';

// sample credits travel with the notices (the recordings are CC0, credit is encouraged)
const soundCredits = [
  {
    name: 'Pro Sounds – VS Chamber Orchestra: Community Edition samples',
    license: 'CC0 1.0 Universal',
    text: `${fs.readFileSync('public/sounds/CREDITS.txt', 'utf8')}\n\n${fs.readFileSync('public/sounds/LICENSE-VSCO-2-CE.txt', 'utf8')}`,
  },
];

// Content-Security-Policy for the browser version (the desktop app sends a
// stricter one as a header). Only for the normal build: the dev server and the
// single-file build need inline scripts.
const WEB_CSP = [
  "default-src 'self'",
  "script-src 'self' blob:",
  "worker-src 'self' blob:",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data: blob:",
  "font-src 'self' data:",
  "media-src 'self' data: blob:",
  "connect-src 'self' data: blob: https://api.github.com https://raw.githubusercontent.com",
  "object-src 'none'",
  "base-uri 'self'",
  "form-action 'none'",
].join('; ');

const webCsp = {
  name: 'goofy-web-csp',
  apply: 'build' as const,
  transformIndexHtml: {
    order: 'post' as const,
    handler: (html: string) => html.replace(/(<meta charset="UTF-8" \/>)/i, `$1\n    <meta http-equiv="Content-Security-Policy" content="${WEB_CSP}" />`),
  },
};

// `npm run build` produces a normal multi-file build in dist/.
// `npm run build:single` produces one self-contained index.html in dist-single/.
export default defineConfig(({ mode }) => ({
  base: './',
  define: {
    __APP_VERSION__: JSON.stringify(pkg.version),
  },
  plugins: [react(), thirdPartyNotices({ appName: 'Goofy Studio', extra: soundCredits }), ...(mode === 'single' ? [viteSingleFile()] : [webCsp])],
  build: {
    outDir: mode === 'single' ? 'dist-single' : 'dist',
    target: 'es2022',
    chunkSizeWarningLimit: 1500,
  },
  test: {
    environment: 'node',
  },
}));
