// Builds the "Pro Sounds" of Goofy Studio into public/sounds/ and writes
// src/audio/pro/manifest.json. The finished files are committed, so this only
// has to run when sounds change:
//
//   node scripts/sounds/build.mjs --vsco <VSCO-2-CE checkout> --sfz <folder with its .sfz files> [--kits] [--only id,id]
//
// --sfz builds the sampled instruments, --kits the synthesized drums/808s/synths
// and the real percussion one-shots (those also need --vsco).
//
// Needs the ffmpeg CLI (with libopus).
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildVsco } from './vsco.mjs';
import { buildKits } from './kits.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const outDir = path.join(root, 'public/sounds');
const manifestFile = path.join(root, 'src/audio/pro/manifest.json');
const args = process.argv.slice(2);
const arg = (k) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : undefined; };
const only = arg('--only')?.split(',');

const manifest = fs.existsSync(manifestFile) ? JSON.parse(fs.readFileSync(manifestFile, 'utf8')) : { version: 1, sounds: {} };
const put = (id, entry) => { manifest.sounds[id] = entry; };

if (arg('--vsco') && arg('--sfz')) {
  const res = buildVsco({ srcRoot: arg('--vsco'), sfzDir: arg('--sfz'), outDir, only });
  for (const [id, e] of Object.entries(res)) put(id, { source: 'vsco', loop: e.loop, zones: e.zones });
}

if (args.includes('--kits')) {
  const res = buildKits({ outDir, vscoRoot: arg('--vsco'), only });
  for (const [id, e] of Object.entries(res)) put(id, { source: e.source ?? 'synth', loop: !!e.loop, zones: e.zones });
}

// keep the manifest stable and sorted
manifest.sounds = Object.fromEntries(Object.entries(manifest.sounds).sort(([a], [b]) => a.localeCompare(b)));
fs.mkdirSync(path.dirname(manifestFile), { recursive: true });
fs.writeFileSync(manifestFile, JSON.stringify(manifest, null, 0).replace(/\{"f"/g, '\n{"f"') + '\n');
let total = 0;
const walk = (d) => { for (const f of fs.readdirSync(d, { withFileTypes: true })) { const p = path.join(d, f.name); if (f.isDirectory()) walk(p); else total += fs.statSync(p).size; } };
if (fs.existsSync(outDir)) walk(outDir);
console.log(`manifest: ${Object.keys(manifest.sounds).length} sounds · public/sounds ${(total / 1024 / 1024).toFixed(2)} MB`);
