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
import { useProject } from './state/store';
import { useUI } from './state/ui';

// Handy for power users and automated tests: window.goofy.engine etc.
(window as unknown as { goofy: unknown }).goofy = { engine, useProject, useUI };

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
