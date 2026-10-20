// Vite plugin: writes THIRD_PARTY_NOTICES.txt into the build output. It lists
// exactly the npm packages whose code or fonts end up in the bundle, with
// their full license texts (MIT and the SIL Open Font License require the
// license to travel with every copy).
import fs from 'node:fs';
import path from 'node:path';

const LICENSE_FILES = ['LICENSE', 'LICENSE.md', 'LICENSE.txt', 'license', 'license.md', 'LICENCE', 'OFL.txt', 'COPYING'];

/** Package directory of a module id inside node_modules (handles @scope/name). */
function packageDir(id) {
  const clean = id.replace(/^\0/, '').split('?')[0];
  const i = clean.lastIndexOf(`node_modules${path.sep}`) >= 0 ? clean.lastIndexOf(`node_modules${path.sep}`) : clean.lastIndexOf('node_modules/');
  if (i < 0) return null;
  const rest = clean.slice(i + 'node_modules/'.length).split(/[\\/]/);
  const parts = rest[0].startsWith('@') ? rest.slice(0, 2) : rest.slice(0, 1);
  return clean.slice(0, i + 'node_modules/'.length) + parts.join('/');
}

function licenseText(dir) {
  for (const f of LICENSE_FILES) {
    const p = path.join(dir, f);
    if (fs.existsSync(p)) return fs.readFileSync(p, 'utf8').trim();
  }
  return null;
}

export function thirdPartyNotices({ appName, extra = [] }) {
  const dirs = new Set();
  return {
    name: 'goofy-third-party-notices',
    apply: 'build',
    moduleParsed(info) {
      const d = packageDir(info.id);
      if (d) dirs.add(d);
    },
    generateBundle(_opts, bundle) {
      // fonts and other assets imported from packages (e.g. @fontsource) are not parsed modules
      for (const file of Object.values(bundle)) {
        for (const id of file.originalFileNames ?? []) {
          const d = packageDir(path.resolve(id));
          if (d) dirs.add(d);
        }
        if (file.type === 'chunk') for (const id of Object.keys(file.modules)) {
          const d = packageDir(id);
          if (d) dirs.add(d);
        }
      }
      const entries = [];
      for (const dir of [...dirs].sort()) {
        let pkg;
        try { pkg = JSON.parse(fs.readFileSync(path.join(dir, 'package.json'), 'utf8')); } catch { continue; }
        entries.push({ name: pkg.name, version: pkg.version, license: typeof pkg.license === 'string' ? pkg.license : pkg.license?.type ?? 'see text', homepage: pkg.homepage ?? '', text: licenseText(dir) });
      }
      const seen = new Set();
      const unique = entries.filter((e) => !seen.has(e.name) && seen.add(e.name));
      const line = '='.repeat(78);
      const out = [
        `${appName} – third-party software notices`,
        '',
        `${appName} contains the following open-source components. Their licenses`,
        'are reproduced below. Electron and Chromium (desktop app only) ship their own',
        'notices next to the program: LICENSE.electron.txt and LICENSES.chromium.html.',
        '',
        ...unique.map((e) => `  - ${e.name} ${e.version} (${e.license})`),
        ...extra.map((e) => `  - ${e.name} (${e.license})`),
        '',
      ];
      for (const e of unique) {
        out.push(line, `${e.name} ${e.version} – ${e.license}${e.homepage ? `\n${e.homepage}` : ''}`, line, '', e.text ?? `License: ${e.license} (no license file in the package)`, '', '');
      }
      for (const e of extra) out.push(line, `${e.name} – ${e.license}`, line, '', e.text, '', '');
      this.emitFile({ type: 'asset', fileName: 'THIRD_PARTY_NOTICES.txt', source: out.join('\n') });
    },
  };
}
