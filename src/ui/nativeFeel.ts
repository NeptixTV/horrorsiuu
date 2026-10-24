// Makes the app behave like a desktop program instead of a web page:
// - audio files dropped anywhere (where nothing else takes them) go to My Sounds
// - no browser context menu – text fields get a small Cut / Copy / Paste menu
// - no page zoom with Ctrl + mouse wheel / pinch, no middle-click auto-scroll
// - tooltips in the app's own style instead of the browser's yellow-ish boxes
// - the window title follows the project ("Trap Beat • – Goofy Studio")
import { showMenu, type MenuItem } from './components/ContextMenu';
import { importSounds, isAudioFile } from './library/libraryStore';
import { setUI, toast, ui } from '../state/ui';
import { useProject } from '../state/store';

const isTextField = (el: Element | null): el is HTMLInputElement | HTMLTextAreaElement =>
  !!el && ((el instanceof HTMLInputElement && /^(text|search|number|url|email|password|)$/i.test(el.type)) || el instanceof HTMLTextAreaElement);

function textMenu(el: HTMLInputElement | HTMLTextAreaElement): MenuItem[] {
  const hasSel = (el.selectionStart ?? 0) !== (el.selectionEnd ?? 0);
  const ro = el.readOnly || el.disabled;
  const run = (cmd: string) => { el.focus(); return document.execCommand(cmd); };
  return [
    { label: 'Cut', shortcut: 'Ctrl+X', disabled: !hasSel || ro, onClick: () => run('cut') },
    { label: 'Copy', shortcut: 'Ctrl+C', disabled: !hasSel, onClick: () => run('copy') },
    {
      label: 'Paste', shortcut: 'Ctrl+V', disabled: ro, onClick: () => {
        if (run('paste')) return;
        void navigator.clipboard?.readText?.().then((t) => {
          el.focus();
          el.setRangeText(t, el.selectionStart ?? el.value.length, el.selectionEnd ?? el.value.length, 'end');
          el.dispatchEvent(new Event('input', { bubbles: true }));
        }).catch(() => toast('Press Ctrl+V to paste', 'info'));
      },
    },
    { separator: true },
    { label: 'Select all', shortcut: 'Ctrl+A', onClick: () => { el.focus(); el.select(); } },
  ];
}

// ---------------------------------------------------------------- tooltips

let tip: HTMLDivElement | null = null;
let tipTimer: ReturnType<typeof setTimeout> | null = null;
let tipTarget: Element | null = null;

function hideTip() {
  if (tipTimer) clearTimeout(tipTimer);
  tipTimer = null;
  tipTarget = null;
  if (tip) tip.style.opacity = '0';
}

function showTip(el: Element, text: string, x: number, y: number) {
  if (!tip) {
    tip = document.createElement('div');
    tip.className = 'app-tooltip';
    document.body.appendChild(tip);
  }
  tip.textContent = text;
  tip.style.opacity = '0';
  tip.style.left = '0px';
  tip.style.top = '0px';
  const r = tip.getBoundingClientRect();
  const left = Math.max(4, Math.min(window.innerWidth - r.width - 4, x + 12));
  const below = y + 22 + r.height < window.innerHeight;
  tip.style.left = `${left}px`;
  tip.style.top = `${below ? y + 20 : y - r.height - 10}px`;
  tip.style.opacity = '1';
  void el;
}

/** The browser shows `title` after a delay in its own style – take the text over and show ours. */
function onOver(e: MouseEvent) {
  if (!ui().showTooltips) return;
  const el = (e.target as Element | null)?.closest?.('[title], [data-tip]');
  if (!el || el === tipTarget) return;
  hideTip();
  const title = el.getAttribute('title');
  if (title) {
    el.setAttribute('data-tip', title);
    el.removeAttribute('title');
  }
  const text = el.getAttribute('data-tip');
  if (!text) return;
  tipTarget = el;
  const { clientX: x, clientY: y } = e;
  tipTimer = setTimeout(() => { if (tipTarget === el && el.isConnected) showTip(el, text, x, y); }, 550);
}

function onOut(e: MouseEvent) {
  if (!tipTarget) return;
  const to = e.relatedTarget as Node | null;
  if (to && tipTarget.contains(to)) return;
  hideTip();
}

// ---------------------------------------------------------------- file drop hint

let dragDepth = 0;
let overlay: HTMLDivElement | null = null;
const hasFiles = (e: DragEvent) => !!e.dataTransfer && Array.from(e.dataTransfer.types).includes('Files');

function dropHint(on: boolean) {
  if (on && !overlay) {
    overlay = document.createElement('div');
    overlay.className = 'app-drop-hint';
    overlay.innerHTML = '<b>Drop your sounds</b><span>on a drum pad, a channel or the playlist – or anywhere else to add them to My Sounds</span>';
    document.body.appendChild(overlay);
  }
  overlay?.classList.toggle('on', on);
}

let installed = false;

export function installNativeFeel() {
  if (installed) return;
  installed = true;
  document.documentElement.classList.add('native');

  // ---- drag & drop of files from the desktop / Explorer
  window.addEventListener('dragenter', (e) => { if (hasFiles(e)) { dragDepth++; dropHint(true); } });
  window.addEventListener('dragleave', (e) => { if (hasFiles(e) && --dragDepth <= 0) { dragDepth = 0; dropHint(false); } });
  window.addEventListener('dragover', (e) => {
    if (!hasFiles(e)) return;
    // without this the browser / Electron would open the file instead of the app
    e.preventDefault();
    if (e.dataTransfer && !e.defaultPrevented) e.dataTransfer.dropEffect = 'copy';
  });
  window.addEventListener('drop', (e) => {
    dragDepth = 0;
    dropHint(false);
    if (!hasFiles(e)) return;
    const handled = e.defaultPrevented;
    e.preventDefault();
    if (handled) return;
    const files = Array.from(e.dataTransfer?.files ?? []).filter(isAudioFile);
    if (files.length) void importSounds(files);
    else toast('Only audio files can be dropped here (WAV, MP3, OGG, FLAC, M4A, AIFF)', 'info');
  });

  // ---- context menu: ours only
  window.addEventListener('contextmenu', (e) => {
    if (e.defaultPrevented) return;
    e.preventDefault();
    const el = e.target as Element | null;
    const field = el?.closest?.('input, textarea') ?? null;
    if (isTextField(field)) showMenu(e.clientX, e.clientY, textMenu(field));
  });

  // ---- no page zoom (Ctrl + wheel, pinch) and no auto-scroll on the middle button
  window.addEventListener('wheel', (e) => { if (e.ctrlKey) e.preventDefault(); }, { passive: false });
  window.addEventListener('mousedown', (e) => { if (e.button === 1) e.preventDefault(); });
  // Ctrl + / Ctrl − / Ctrl 0 change the interface size of the program (not a page zoom)
  window.addEventListener('keydown', (e) => {
    if (!(e.ctrlKey || e.metaKey) || e.altKey) return;
    const k = e.key;
    if (k !== '+' && k !== '-' && k !== '=' && k !== '0') return;
    e.preventDefault();
    const cur = ui().uiScale;
    const next = k === '0' ? 1 : Math.round(Math.min(1.6, Math.max(0.7, cur + (k === '-' ? -0.1 : 0.1))) * 100) / 100;
    if (next !== cur) {
      setUI({ uiScale: next });
      toast(`Interface size ${Math.round(next * 100)} %`, 'info');
    }
  });
  document.addEventListener('gesturestart', (e) => e.preventDefault());

  // ---- tooltips
  document.addEventListener('mouseover', onOver);
  document.addEventListener('mouseout', onOut);
  document.addEventListener('pointerdown', hideTip, true);
  document.addEventListener('wheel', hideTip, { passive: true, capture: true });
  window.addEventListener('blur', hideTip);

  // ---- window title
  const title = () => {
    const s = useProject.getState();
    document.title = `${s.project.name}${s.dirty ? ' •' : ''} – Goofy Studio`;
  };
  title();
  useProject.subscribe((s, prev) => { if (s.project.name !== prev.project.name || s.dirty !== prev.dirty) title(); });
}
