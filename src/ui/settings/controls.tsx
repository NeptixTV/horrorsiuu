// Small building blocks of the settings center.
import type { ReactNode } from 'react';

export function Section({ title, children, hint }: { title: string; children: ReactNode; hint?: ReactNode }) {
  return (
    <section className="sc-section">
      <h3>{title}</h3>
      {hint && <p className="sc-hint">{hint}</p>}
      <div className="sc-rows">{children}</div>
    </section>
  );
}

export function Row({ label, desc, children, warn }: { label: string; desc?: ReactNode; children: ReactNode; warn?: boolean }) {
  return (
    <div className={`sc-row ${warn ? 'warn' : ''}`}>
      <div className="sc-row-text">
        <div className="sc-row-label">{label}</div>
        {desc && <div className="sc-row-desc">{desc}</div>}
      </div>
      <div className="sc-row-ctl">{children}</div>
    </div>
  );
}

export function Toggle({ value, onChange, label }: { value: boolean; onChange: (v: boolean) => void; label?: string }) {
  return (
    <button type="button" role="switch" aria-checked={value} aria-label={label} className={`sc-toggle ${value ? 'on' : ''}`} onClick={() => onChange(!value)}>
      <span className="sc-toggle-knob" />
    </button>
  );
}

export function Select<T extends string | number>({ value, options, onChange, width }: {
  value: T; options: { value: T; label: string }[]; onChange: (v: T) => void; width?: number;
}) {
  return (
    <select
      className="input sc-select"
      style={width ? { width } : undefined}
      value={String(value)}
      onChange={(e) => {
        const o = options.find((x) => String(x.value) === e.target.value);
        if (o) onChange(o.value);
      }}
    >
      {options.map((o) => <option key={String(o.value)} value={String(o.value)}>{o.label}</option>)}
    </select>
  );
}

export function Slider({ value, min, max, step, onChange, format, width = 180 }: {
  value: number; min: number; max: number; step: number; onChange: (v: number) => void; format: (v: number) => string; width?: number;
}) {
  return (
    <div className="sc-slider">
      <input type="range" min={min} max={max} step={step} value={value} style={{ width }} onChange={(e) => onChange(Number(e.target.value))} />
      <span className="sc-slider-val">{format(value)}</span>
    </div>
  );
}

export function Stat({ label, value, warn }: { label: string; value: ReactNode; warn?: boolean }) {
  return (
    <div className={`sc-stat ${warn ? 'warn' : ''}`}>
      <span>{label}</span>
      <b>{value}</b>
    </div>
  );
}

export const fmtBytes = (n: number) => (n >= 1e9 ? `${(n / 1e9).toFixed(2)} GB` : n >= 1e6 ? `${(n / 1e6).toFixed(1)} MB` : `${Math.round(n / 1e3)} KB`);
