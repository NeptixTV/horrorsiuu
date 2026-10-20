// "My presets" part of the preset menus (synth, DreamChord, every effect):
// save the current sound under a name, load or delete own presets. Loaded
// values always pass through the project normaliser, so an imported preset
// file can never put broken values into a project.
import type { MenuItem } from './components/ContextMenu';
import { deleteUserPreset, presetsOf, saveUserPreset, type PresetKind } from '../state/userPresets';
import { askText } from './dialogs/askText';
import { toast } from '../state/ui';

export function userPresetItems(kind: PresetKind, current: () => object, apply: (params: Record<string, unknown>, name: string) => void, suggested = 'My preset'): MenuItem[] {
  const mine = presetsOf(kind);
  const items: MenuItem[] = [{ separator: true }];
  if (mine.length) {
    items.push({ label: 'My presets', submenu: mine.map((u) => ({ label: u.name, onClick: () => apply(u.params, u.name) })) });
    items.push({ label: 'Delete my preset', submenu: mine.map((u) => ({ label: u.name, danger: true, onClick: () => { deleteUserPreset(u.id); toast(`Deleted “${u.name}”`, 'ok'); } })) });
  }
  items.push({
    label: 'Save current settings as preset…',
    onClick: async () => {
      const name = await askText('Preset name', suggested);
      if (!name?.trim()) return;
      const exists = mine.some((u) => u.name.toLowerCase() === name.trim().toLowerCase());
      saveUserPreset(kind, name, current());
      toast(exists ? `Updated preset “${name.trim()}”` : `Saved preset “${name.trim()}” – find it under Presets → My presets`, 'ok');
    },
  });
  return items;
}
