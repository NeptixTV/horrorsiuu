import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
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
