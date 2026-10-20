import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import '@fontsource/inter/400.css';
import '@fontsource/inter/600.css';
import '@fontsource/inter/700.css';
import '@fontsource/inter/800.css';
import '@fontsource/inter/900.css';
import '@fontsource/jetbrains-mono/400.css';
import '@fontsource/jetbrains-mono/600.css';
import { App } from './App';
import { engine } from './audio/engine';
import * as store from './state/store';
import * as ui from './state/ui';
import * as actions from './state/actions';
import * as commands from './ui/commands';
import * as manager from './project/manager';
import { diagnostics } from './audio/diagnostics';
import { renderProject } from './audio/render';

// Handy for power users and automated tests: window.goofy.engine etc.
// (Everything here is also reachable from the UI; no secrets live in the page.)
(window as unknown as { goofy: unknown }).goofy = {
  engine, useProject: store.useProject, useUI: ui.useUI, store, ui, actions, commands, manager, diagnostics, renderProject,
};

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
