import type { NumberParamDef, ParamDef } from '../../audio/effects/descriptors';
import type { ParamValue } from '../../state/types';
import { formatHz } from '../../state/utils';
import { Knob } from '../components/Knob';

export function formatParam(def: NumberParamDef, v: number): string {
  switch (def.unit) {
    case 'Hz': return formatHz(v);
    case 'dB': return `${v > 0 ? '+' : ''}${v.toFixed(1)} dB`;
    case 'ms': return v >= 1000 ? `${(v / 1000).toFixed(2)} s` : `${v < 10 ? v.toFixed(1) : Math.round(v)} ms`;
    case 's': return `${v.toFixed(2)} s`;
    case ':1': return `${v.toFixed(1)}:1`;
    case 'st': return `${v > 0 ? '+' : ''}${v.toFixed(1)} st`;
    case 'Q': return `Q ${v.toFixed(2)}`;
    default: return def.max <= 1 && def.min >= 0 ? `${Math.round(v * 100)}%` : v.toFixed(2);
  }
}

export function ParamControl({ def, value, onChange, size = 40, color }: { def: ParamDef; value: ParamValue | undefined; onChange: (v: ParamValue) => void; size?: number; color?: string }) {
  if (def.kind === 'choice') {
    return (
      <label className="fx-choice">
        <span className="knob-label">{def.label}</span>
        <select className="input" value={String(value ?? def.default)} onChange={(e) => onChange(e.target.value)} onKeyDown={(e) => e.stopPropagation()}>
          {def.options.map((o) => <option key={o.value} value={o.value}>{o.label}</option>)}
        </select>
      </label>
    );
  }
  const v = typeof value === 'number' ? value : def.default;
  return (
    <Knob
      value={v}
      min={def.min}
      max={def.max}
      log={def.log}
      defaultValue={def.default}
      size={size}
      label={def.label}
      color={color}
      bipolar={def.min < 0 && def.max > 0}
      onChange={(nv) => onChange(nv)}
      format={(x) => formatParam(def, x)}
    />
  );
}
