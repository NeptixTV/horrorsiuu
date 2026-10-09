// electron-builder hook: writes the Goofy Studio icon and version info into
// the Windows executable with resedit (pure JS, so no Wine is needed when
// building on Linux or macOS).
const fs = require('node:fs');
const path = require('node:path');

exports.default = async function afterPack(context) {
  if (context.electronPlatformName !== 'win32') return;
  const { NtExecutable, NtExecutableResource, Data, Resource } = await import('resedit');
  const exeName = `${context.packager.appInfo.productFilename}.exe`;
  const exePath = path.join(context.appOutDir, exeName);
  const exe = NtExecutable.from(fs.readFileSync(exePath));
  const res = NtExecutableResource.from(exe);
  const iconFile = Data.IconFile.from(fs.readFileSync(path.join(__dirname, '..', 'build', 'icon.ico')));
  Resource.IconGroupEntry.replaceIconsForResource(res.entries, 1, 1033, iconFile.icons.map((i) => i.data));
  const version = context.packager.appInfo.version;
  const [major, minor, patch] = version.split('.').map((n) => parseInt(n, 10) || 0);
  const vi = Resource.VersionInfo.fromEntries(res.entries)[0] ?? Resource.VersionInfo.createEmpty();
  vi.setFileVersion(major, minor, patch, 0, 1033);
  vi.setProductVersion(major, minor, patch, 0, 1033);
  vi.setStringValues({ lang: 1033, codepage: 1200 }, {
    FileDescription: 'Goofy Studio',
    ProductName: 'Goofy Studio',
    CompanyName: 'Goofy Studio',
    OriginalFilename: exeName,
    InternalName: 'Goofy Studio',
    LegalCopyright: `© ${new Date().getFullYear()} Goofy Studio`,
  });
  vi.outputToResourceEntries(res.entries);
  res.outputResource(exe);
  fs.writeFileSync(exePath, Buffer.from(exe.generate()));
  console.log(`  • patched icon + version info → ${exeName}`);
};
