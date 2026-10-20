// Song notes: lyrics, ideas and to-dos saved inside the project. The
// teleprompter mode shows the text big while you record; it scrolls with the
// song, and section headings like "[Chorus]" jump into view when the playhead
// passes a playlist marker with the same name.
import { useEffect, useMemo, useRef } from 'react';
import { create } from 'zustand';
import type { Project } from '../../state/types';
import { getProject, pushHistory, update, useProject } from '../../state/store';
import { openWindow, type FloatingWindow } from '../../state/ui';
import { engine } from '../../audio/engine';
import { onFrame } from '../components/frameLoop';
import { Window } from '../components/Window';
import { Icon } from '../components/Icon';
import { HEADING, currentMarker, matchSection, sections } from './prompter';
import './notes.css';

const MAX_NOTES = 200000;
const NO_MARKERS: NonNullable<Project['markers']> = [];

export const useNotesView = create<{ prompter: boolean; speed: number; size: number }>(() => ({ prompter: false, speed: 1, size: 30 }));

/** Opens the notes window (optionally straight in teleprompter mode). */
export function openNotes(prompter = false) {
  if (prompter) useNotesView.setState({ prompter: true });
  openWindow({ kind: 'notes' });
}

function Prompter({ text }: { text: string }) {
  const { speed, size } = useNotesView();
  const box = useRef<HTMLDivElement>(null);
  const secs = useMemo(() => sections(text), [text]);
  const lines = useMemo(() => text.split('\n'), [text]);
  useEffect(() => {
    let last = '';
    return onFrame((dt) => {
      const el = box.current;
      const t = engine.transport;
      if (!el || !t?.playing) return;
      if (t.mode === 'song') {
        const mk = currentMarker(getProject(), t.getTick());
        const key = mk ? `${mk.name}@${mk.tick}` : '';
        if (key && key !== last) {
          last = key;
          const i = matchSection(secs, mk!.name);
          const row = i >= 0 ? el.querySelector<HTMLElement>(`[data-line="${secs[i].line}"]`) : null;
          if (row) {
            el.scrollTo({ top: row.offsetTop - 12, behavior: 'smooth' });
            el.querySelectorAll('.np-line.now').forEach((x) => x.classList.remove('now'));
            row.classList.add('now');
            return;
          }
        }
      }
      // no matching marker: scroll slowly, about one line every two beats
      el.scrollTop += dt * speed * size * 0.7 * ((getProject().bpm || 120) / 120);
    });
  }, [secs, speed, size]);
  return (
    <div className="np-prompter" ref={box} style={{ fontSize: size }}>
      {lines.map((l, i) => {
        const h = HEADING.exec(l);
        return (
          <div key={i} data-line={i} className={`np-line ${h ? 'head' : ''}`}>
            {h ? (h[1] ?? h[2]) : l || ' '}
          </div>
        );
      })}
      <div className="np-end">— end —</div>
    </div>
  );
}

export function NotesWindow({ w }: { w: FloatingWindow }) {
  const text = useProject((s) => s.project.notes ?? '');
  // select the array itself – `?? []` in the selector would be a new array on every render
  const markerList = useProject((s) => s.project.markers);
  const markers = markerList ?? NO_MARKERS;
  const view = useNotesView();
  const base = useRef<Project | null>(null);
  const secs = useMemo(() => sections(text), [text]);
  const linked = secs.filter((s) => markers.some((m) => matchSection([s], m.name) === 0)).length;

  // typing is one undo step per editing session (focus → blur), not per key
  const commit = () => {
    if (base.current) pushHistory(base.current, 'Edit song notes');
    base.current = null;
  };
  useEffect(() => commit, []);

  return (
    <Window
      w={w}
      width={view.prompter ? 620 : 460}
      color="#9be15d"
      title={<><Icon name="notes" size={12} /> <b>{view.prompter ? 'Teleprompter' : 'Song notes'}</b> <span className="crumb">· saved in the project</span></>}
      actions={
        <button className={`btn small ${view.prompter ? 'on' : ''}`} onClick={() => { commit(); useNotesView.setState({ prompter: !view.prompter }); }} title="Big text that scrolls with the song – for singing or rapping while you record">
          {view.prompter ? 'Edit' : 'Teleprompter'}
        </button>
      }
    >
      {view.prompter ? (
        <div className="np">
          {text.trim() ? <Prompter text={text} /> : <div className="np-empty">Write your lyrics first (Edit), then switch to the teleprompter.</div>}
          <div className="np-tools">
            <span className="label">Text size</span>
            <input type="range" min={16} max={56} value={view.size} onChange={(e) => useNotesView.setState({ size: Number(e.target.value) })} />
            <span className="label">Scroll speed</span>
            <input type="range" min={0} max={3} step={0.1} value={view.speed} onChange={(e) => useNotesView.setState({ speed: Number(e.target.value) })} />
            <span className="spacer" />
            <span className="crumb">{linked ? `${linked} section${linked === 1 ? '' : 's'} follow your markers` : 'Scrolls while playing'}</span>
          </div>
        </div>
      ) : (
        <div className="np">
          <textarea
            className="np-text"
            value={text}
            maxLength={MAX_NOTES}
            spellCheck
            placeholder={'Lyrics, ideas, to-dos …\n\n[Verse 1]\nFirst line of the verse\n\n[Chorus]\nThe hook goes here\n\nTip: name playlist markers like the [sections] – the teleprompter jumps to them while the song plays.'}
            onFocus={() => { base.current = getProject(); }}
            onBlur={commit}
            onKeyDown={(e) => e.stopPropagation()}
            onChange={(e) => {
              const v = e.target.value.slice(0, MAX_NOTES);
              if (!base.current) base.current = getProject();
              update('Edit song notes', (d) => { d.notes = v; }, { history: false });
            }}
          />
          <div className="np-tools">
            <span className="crumb">{text.length ? `${text.split(/\s+/).filter(Boolean).length} words · ${secs.length} sections` : 'Empty'}</span>
            <span className="spacer" />
            {secs.length > 0 && markers.length > 0 && <span className="crumb">{linked}/{secs.length} sections match a marker</span>}
          </div>
        </div>
      )}
    </Window>
  );
}
