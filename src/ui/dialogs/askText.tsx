// In-app replacement for window.prompt() (which is not available in the
// desktop build): a small modal text input that resolves to the entered text.
import { useEffect, useRef, useState } from 'react';
import { create } from 'zustand';
import { Modal } from '../components/Window';

interface AskState {
  req: { title: string; value: string; resolve: (v: string | null) => void } | null;
}

const useAsk = create<AskState>(() => ({ req: null }));

export function askText(title: string, value = ''): Promise<string | null> {
  return new Promise((resolve) => {
    useAsk.getState().req?.resolve(null);
    useAsk.setState({ req: { title, value, resolve } });
  });
}

export function AskTextHost() {
  const req = useAsk((s) => s.req);
  const [text, setText] = useState('');
  const ref = useRef<HTMLInputElement>(null);
  useEffect(() => {
    if (req) {
      setText(req.value);
      requestAnimationFrame(() => ref.current?.select());
    }
  }, [req]);
  if (!req) return null;
  const close = (v: string | null) => {
    useAsk.setState({ req: null });
    req.resolve(v);
  };
  return (
    <Modal title={req.title} onClose={() => close(null)} width={380}>
      <form
        className="ask-form"
        onSubmit={(e) => { e.preventDefault(); close(text.trim() ? text : null); }}
      >
        <input
          ref={ref}
          className="input"
          value={text}
          autoFocus
          onChange={(e) => setText(e.target.value)}
          onKeyDown={(e) => { e.stopPropagation(); if (e.key === 'Escape') close(null); }}
        />
        <div className="ask-actions">
          <button type="button" className="btn" onClick={() => close(null)}>Cancel</button>
          <button type="submit" className="btn primary">OK</button>
        </div>
      </form>
    </Modal>
  );
}
