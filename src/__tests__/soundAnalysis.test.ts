import { describe, expect, it } from 'vitest';
import { analyzeSound, bpmFromName, categoryFromName, detectPitch, noteFromName, trimRange } from '../audio/soundAnalysis';

const SR = 44100;
const sine = (f: number, sec: number, amp = 0.8) => Float32Array.from({ length: Math.round(SR * sec) }, (_, i) => Math.sin((2 * Math.PI * f * i) / SR) * amp * Math.exp(-i / (SR * sec)));

describe('sound analysis', () => {
  it('sorts sample pack names', () => {
    expect(categoryFromName('Trap Kick 03.wav')).toBe('Kicks');
    expect(categoryFromName('808_kick_hard')).toBe('Kicks');
    expect(categoryFromName('808 Mafia Bass C1')).toBe('808s');
    expect(categoryFromName('HH_closed_12')).toBe('Hi-Hats');
    expect(categoryFromName('Open Hat 2')).toBe('Open Hats');
    expect(categoryFromName('Snare - Crispy')).toBe('Snares');
    expect(categoryFromName('clap_wide')).toBe('Claps');
    expect(categoryFromName('Dark Piano Loop 140 BPM')).toBe('Loops');
    expect(categoryFromName('vox chop hey')).toBe('Vocals');
    expect(categoryFromName('Riser 01')).toBe('FX');
    expect(categoryFromName('Bell C5')).toBe('Melodic');
    expect(categoryFromName('xyz 123')).toBeNull();
  });

  it('reads tempo and note names', () => {
    expect(bpmFromName('Dark Loop 140 BPM')).toBe(140);
    expect(bpmFromName('loop_92bpm_Am')).toBe(92);
    expect(bpmFromName('trap_150_Cmin')).toBe(150);
    expect(bpmFromName('Kick 03')).toBeUndefined();
    expect(noteFromName('Piano C3')).toBe(48);
    expect(noteFromName('808_F#1')).toBe(30);
    expect(noteFromName('bell_Gb4')).toBe(66);
    expect(noteFromName('Am7 chord')).toBeUndefined();
  });

  it('finds the pitch of a tone', () => {
    expect(detectPitch(sine(220, 1), SR)).toBe(57);
    expect(detectPitch(sine(55, 1.5), SR)).toBe(33);
    const noise = Float32Array.from({ length: SR }, () => Math.random() * 2 - 1);
    expect(detectPitch(noise, SR)).toBeNull();
  });

  it('trims silence around a sound', () => {
    const x = new Float32Array(SR);
    for (let i = 10000; i < 20000; i++) x[i] = Math.sin(i * 0.1) * 0.5;
    const r = trimRange([x], SR);
    expect(r.start).toBeGreaterThan(9800);
    expect(r.start).toBeLessThanOrEqual(10000);
    expect(r.end).toBeGreaterThan(19990);
    expect(r.end).toBeLessThan(21000);
  });

  it('guesses unnamed drums from the audio', () => {
    expect(analyzeSound('sample1', [sine(50, 0.5)], SR).category).toBe('Kicks');
    const hat = Float32Array.from({ length: SR * 0.08 }, (_, i) => { let v = Math.random() * 2 - 1; v -= 0; return v * Math.exp(-i / 600); });
    // high-passed noise
    for (let i = hat.length - 1; i > 0; i--) hat[i] = hat[i] - hat[i - 1];
    expect(analyzeSound('sample2', [hat], SR).category).toBe('Hi-Hats');
    const tone = analyzeSound('lead 1', [sine(440, 1.2)], SR);
    expect(tone.category).toBe('Melodic');
    expect(tone.root).toBe(69);
  });
});
