"""
Original music for the hideout, synthesized from scratch:
  M_Hideout_Theme     - dark ambient score (loops), D minor
  M_Title_Sting       - title card hit
  M_Departure_Sting   - the van leaves
  M_Radio_Station_A   - "WHLW 1340 late night": brushed jazz trio, heard through an old radio
  M_Radio_Station_B   - a hymn on the Sunday-night organ hour, heard through an old radio
Run from the HorrorHeist project folder:  python Tools/AssetGen/audio/gen_music.py
"""
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import hhsynth as H  # noqa: E402
from hhsynth import (lp, hp, bp, peak, resonator, white, pink, brown, env_exp, env_adsr, fade, place, pan,  # noqa: E402
                     note_hz, detune_ratio, make_ir, reverb, reverb_tail, loudness, dc_block, make_loop, radio, save,
                     write_manifest, seed)

SR = 44100
H.SR = SR  # all helpers default to the music sample rate from here on


def tt(d):
    return np.arange(int(d * SR)) / SR


# ---------------------------------------------------------------------------------------
# Instruments

def felt_piano(freq, dur, vel=0.7, felt=0.6):
    """Soft, intimate piano: slightly inharmonic partials, felt-dampened hammer."""
    x = tt(dur)
    out = np.zeros_like(x)
    B = 0.00035
    for k in range(1, 14):
        fk = freq * k * np.sqrt(1 + B * k * k)
        if fk > SR * 0.45:
            break
        amp = (1 / k ** 1.25) * (1.0 if k == 1 else vel ** 0.6)
        decay = (2.8 + 2.0 * vel) / (1 + 0.45 * (k - 1)) * (220 / max(freq, 60)) ** 0.25
        beat = 1 + 0.0012 * np.sin(2 * np.pi * 0.7 * x + k)
        out += amp * np.exp(-x / decay) * np.sin(2 * np.pi * fk * x * beat + H.RNG.uniform(0, 6.28))
    hammer = lp(white(min(dur, 0.05)), 1500 + 2500 * vel) * env_exp(min(dur, 0.05), 0.006, sr=SR) * 0.15
    out[: len(hammer)] += hammer
    out = lp(out, 900 + 3600 * vel * (1 - felt * 0.5))
    attack = np.clip(x / 0.004, 0, 1)
    release = np.clip((dur - x) / 0.25, 0, 1)
    return out * attack * release * vel


def saw_voice(freq, dur, voices=5, spread=9.0):
    x = tt(dur)
    out = np.zeros_like(x)
    for v in range(voices):
        cents = (v - (voices - 1) / 2) * spread / max(voices - 1, 1) * 2
        f = freq * detune_ratio(cents)
        phase = H.RNG.uniform(0, 1)
        harmonics = int(max(1, min(3000.0 / f, 24)))
        for k in range(1, harmonics + 1):
            out += np.sin(2 * np.pi * f * k * x + phase * k) / k / voices
    return out


def pad(freqs, dur, cutoff=900.0, attack=2.5, release=3.0, lfo_rate=0.07):
    x = tt(dur)
    stereo = np.zeros((2, len(x)))
    for i, f in enumerate(freqs):
        v = saw_voice(f, dur, voices=3, spread=10)
        stereo += pan(v, -0.6 + 1.2 * (i / max(len(freqs) - 1, 1)))
    cut = cutoff * (1 + 0.35 * np.sin(2 * np.pi * lfo_rate * x + 0.7))
    for c in range(2):
        stereo[c] = H.tv_lowpass(stereo[c], cut, sr=SR)
        stereo[c] = H.tv_lowpass(stereo[c], cut * 1.4, sr=SR)
    env = env_adsr(dur, attack, 0.5, 0.85, release, sr=SR)
    return stereo * env / max(len(freqs), 1)


def sub_drone(freq, dur):
    x = tt(dur)
    s = np.sin(2 * np.pi * freq * x) + 0.25 * np.sin(2 * np.pi * freq * 2 * x + 0.3)
    s *= 0.85 + 0.15 * np.sin(2 * np.pi * 0.05 * x)
    return s


def bowed_metal(freq, dur):
    x = tt(dur)
    exc = white(dur)
    out = np.zeros_like(x)
    for ratio, q, a in ((1.0, 120, 1.0), (2.76, 150, 0.6), (5.40, 170, 0.35), (8.93, 190, 0.2)):
        out += a * resonator(exc, freq * ratio, q=q, sr=SR)
    env = np.sin(np.pi * np.clip(x / dur, 0, 1)) ** 2
    return out * env


def boom(dur=3.0):
    x = tt(dur)
    f = 32 + 60 * np.exp(-x / 0.12)
    body = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-x / 0.9)
    hitn = lp(white(dur), 400) * np.exp(-x / 0.08) * 0.6
    return body + hitn


def string_cluster(freqs, dur, attack=1.5, release=2.5):
    x = tt(dur)
    stereo = np.zeros((2, len(x)))
    for i, f in enumerate(freqs):
        vib = 1 + 0.004 * np.sin(2 * np.pi * (5.2 + 0.3 * i) * x + i)
        v = np.zeros_like(x)
        for k in range(1, 18):
            if f * k > SR * 0.45:
                break
            v += np.sin(2 * np.pi * np.cumsum(f * k * vib) / SR) / k
        bow = bp(white(dur), f, min(f * 6, 9000)) * 0.05
        v = lp(v + bow, 2800)
        stereo += pan(v, -0.7 + 1.4 * (i / max(len(freqs) - 1, 1)))
    return stereo * env_adsr(dur, attack, 0.3, 0.9, release, sr=SR) / max(len(freqs), 1)


def fm_epiano(freq, dur, vel=0.6):
    x = tt(dur)
    index = (1.6 + 2.0 * vel) * np.exp(-x / 0.35)
    mod = np.sin(2 * np.pi * freq * x) * index
    body = np.sin(2 * np.pi * freq * x + mod)
    tine = np.sin(2 * np.pi * freq * 14.0 * x) * np.exp(-x / 0.03) * 0.12 * vel
    env = np.exp(-x / (1.4 + 1.5 * vel)) * np.clip(x / 0.003, 0, 1) * np.clip((dur - x) / 0.08, 0, 1)
    trem = 1 + 0.12 * np.sin(2 * np.pi * 4.6 * x)
    return (body + tine) * env * trem * vel


def upright_bass(freq, dur, vel=0.8):
    x = tt(dur)
    s = np.sin(2 * np.pi * freq * x) + 0.45 * np.sin(2 * np.pi * 2 * freq * x) * np.exp(-x / 0.3) + 0.2 * np.sin(2 * np.pi * 3 * freq * x) * np.exp(-x / 0.15)
    pluck = lp(white(0.03), 1200) * env_exp(0.03, 0.005, sr=SR) * 0.4
    s[: len(pluck)] += pluck
    env = np.exp(-x / 0.9) * np.clip(x / 0.004, 0, 1) * np.clip((dur - x) / 0.06, 0, 1)
    return lp(s * env, 1800) * vel


def ride(dur=1.4, vel=0.5):
    x = tt(dur)
    freqs = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]
    metal = sum(np.sign(np.sin(2 * np.pi * f * 1.9 * x + H.RNG.uniform(0, 6))) for f in freqs)
    metal = bp(metal, 3000, 11000)
    ping = np.sin(2 * np.pi * 4600 * x) * np.exp(-x / 0.25) * 0.15
    return (metal * 0.15 + ping) * np.exp(-x / 0.7) * vel


def brush_hit(dur=0.25, vel=0.5):
    x = tt(dur)
    return bp(white(dur), 1800, 9000) * np.exp(-x / 0.06) * vel


def kick_soft(dur=0.4, vel=0.5):
    x = tt(dur)
    f = 48 + 40 * np.exp(-x / 0.03)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-x / 0.18) * vel


def vibes(freq, dur, vel=0.6):
    x = tt(dur)
    s = np.sin(2 * np.pi * freq * x) + 0.25 * np.sin(2 * np.pi * freq * 4.0 * x) * np.exp(-x / 0.2)
    trem = 1 + 0.35 * np.sin(2 * np.pi * 5.5 * x)
    return s * np.exp(-x / 1.6) * trem * vel * np.clip(x / 0.002, 0, 1) * np.clip((dur - x) / 0.1, 0, 1)


def organ(freqs, dur, drawbars=(0.6, 1.0, 0.5, 0.7, 0.3, 0.45, 0.0, 0.2, 0.1)):
    """Tonewheel organ with a slow leslie."""
    x = tt(dur)
    ratios = (0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0)
    leslie_rate = 0.85
    stereo = np.zeros((2, len(x)))
    for f in freqs:
        v = np.zeros_like(x)
        for r, a in zip(ratios, drawbars):
            if a <= 0 or f * r > SR * 0.45:
                continue
            v += a * np.sin(2 * np.pi * f * r * x + H.RNG.uniform(0, 6))
        stereo += pan(v, 0.0)
    am_l = 1 + 0.25 * np.sin(2 * np.pi * leslie_rate * x)
    am_r = 1 + 0.25 * np.sin(2 * np.pi * leslie_rate * x + np.pi * 0.7)
    stereo[0] *= am_l
    stereo[1] *= am_r
    env = np.clip(x / 0.04, 0, 1) * np.clip((dur - x) / 0.12, 0, 1)
    return stereo * env / max(len(freqs), 1)


def choir_aah(freq, dur):
    x = tt(dur)
    vib = 1 + 0.006 * np.sin(2 * np.pi * 5.0 * x + H.RNG.uniform(0, 6))
    phase = np.cumsum(freq * vib) / SR
    pulses = np.zeros_like(x)
    pulses[np.where(np.diff(np.floor(phase)) > 0)[0]] = 1.0
    src = lp(pulses, 5000) + white(dur) * 0.01
    out = np.zeros_like(x)
    for f, a in zip((700, 1150, 2600), (1.0, 0.6, 0.25)):
        out += a * resonator(src, f, q=6, sr=SR)
    return out * env_adsr(dur, 0.8, 0.3, 0.9, 1.2, sr=SR)


def tape_bed(dur, level=0.004):
    hiss = bp(white(dur), 2000, 12000) * level
    crackle = np.zeros(int(dur * SR))
    for _ in range(int(dur * 3)):
        i = H.RNG.integers(0, len(crackle) - 50)
        crackle[i:i + 40] += H.RNG.uniform(-1, 1) * np.exp(-np.arange(40) / 6) * 0.05
    return np.stack([hiss + hp(crackle, 1500), hiss * 0.9 + hp(np.roll(crackle, 7000), 1500)])


# ---------------------------------------------------------------------------------------
# Hideout theme

def hideout_theme():
    seed(2001)
    bpm = 64.0
    beat = 60.0 / bpm
    bar = beat * 4
    cycles = 5
    total = cycles * 8 * bar + 6.0
    n = int(total * SR)
    mix = np.zeros((2, n))
    hall = make_ir(duration=5.0, decay=3.6, predelay=0.03, damping=5000, stereo=True, sr=SR, brightness=0.4)

    chords = [
        ['D2', 'A2', 'D3', 'F3', 'E4'],
        ['A#1', 'F2', 'D3', 'A3', 'C4'],
        ['G2', 'D3', 'A#3', 'A4'],
        ['A2', 'E3', 'G3', 'D4', 'A#3'],
    ]

    # Sub drone for the whole piece.
    drone = sub_drone(note_hz('D1'), total) * 0.28
    mix += np.stack([drone, drone])

    pads = np.zeros((2, n))
    for cycle in range(cycles):
        open_ = [0.55, 0.75, 1.0, 0.8, 0.5][cycle]
        for ci, chord in enumerate(chords):
            start = (cycle * 8 + ci * 2) * bar
            freqs = [note_hz(nm) for nm in chord]
            p = pad(freqs, 2 * bar + 3.0, cutoff=500 + 900 * open_, attack=2.8, release=3.0)
            place(pads, p, start, gain=0.55)
    mix += reverb(pads, hall, wet=0.35, dry=0.8)

    # Piano motif (enters in cycle 2).
    motif = [
        (0, 0, 'A4', 0.55), (0, 2, 'F4', 0.45), (1, 0, 'E4', 0.5), (1, 3, 'D4', 0.4),
        (2, 0, 'A#3', 0.45), (2, 2, 'D4', 0.4), (3, 1, 'C#4', 0.55), (5, 0, 'A4', 0.4),
        (5, 2, 'G4', 0.35), (6, 0, 'F4', 0.45), (7, 0, 'E4', 0.5), (7, 2, 'A3', 0.35),
    ]
    piano = np.zeros((2, n))
    for cycle in range(1, cycles):
        if cycle == 4:
            notes = [(0, 0, 'D5', 0.35), (2, 0, 'A4', 0.3), (4, 0, 'F4', 0.3), (6, 0, 'E4', 0.3)]
        elif cycle == 3:
            notes = [(b, s, nm.replace('4', '5') if '4' in nm else nm, v * 0.8) for b, s, nm, v in motif[::2]]
        else:
            notes = motif
        for b, s, nm, v in notes:
            start = (cycle * 8 + b) * bar + s * beat + H.RNG.uniform(-0.02, 0.03)
            note = felt_piano(note_hz(nm), 5.5, vel=v)
            place(piano, pan(note, H.RNG.uniform(-0.25, 0.25)), start)
    mix += reverb(piano, hall, wet=0.55, dry=0.75) * 0.9

    # Bowed metal swells (cycles 3-4).
    metal = np.zeros((2, n))
    for cycle in (2, 3):
        for k in range(3):
            start = (cycle * 8 + k * 2.5 + H.RNG.uniform(0, 1)) * bar
            f = note_hz(['D4', 'A4', 'A#3', 'E5'][H.RNG.integers(0, 4)])
            m = bowed_metal(f, H.RNG.uniform(6, 9))
            place(metal, pan(m, H.RNG.uniform(-0.8, 0.8)), start, gain=0.05)
    mix += reverb(metal, hall, wet=0.7, dry=0.4)

    # Heartbeat pulse (cycle 3).
    pulse = np.zeros(n)
    for b in range(8):
        for off, g in ((0.0, 1.0), (0.32, 0.6)):
            start = (2 * 8 + b) * bar + off
            place(pulse, lp(kick_soft(0.5, 0.7), 140), start, gain=g * 0.5)
    mix += np.stack([pulse, pulse])

    mix += tape_bed(total, 0.003)
    mix = make_loop(mix, 6.0, sr=SR)
    mix = lp(mix, 11000)
    mix = loudness(dc_block(mix), -21.0)
    save('SourceArt/Audio/Music/M_Hideout_Theme.wav', mix, sr=SR, category='Music', loop=True, spatial=False)


# ---------------------------------------------------------------------------------------
# Stings

def title_sting():
    seed(2002)
    total = 14.0
    n = int(total * SR)
    mix = np.zeros((2, n))
    hall = make_ir(duration=6.0, decay=4.5, predelay=0.03, damping=4500, stereo=True, sr=SR, brightness=0.35)

    swell_d = 2.2
    x = tt(swell_d)
    swell = bp(white(swell_d), 300, 6000) * (x / swell_d) ** 3
    swell = H.tv_lowpass(swell, 200 + 6000 * (x / swell_d) ** 2, sr=SR)
    place(mix, np.stack([swell, swell]) * 0.5, 0.0)
    place(mix, np.stack([boom(4.0), boom(4.0)]) * 0.9, swell_d)
    cluster = string_cluster([note_hz(nm) for nm in ('D3', 'D#3', 'A3', 'A#3', 'D4')], 9.0, attack=1.2, release=4.0)
    place(mix, cluster * 0.8, swell_d)
    place(mix, pan(felt_piano(note_hz('A5'), 7.0, vel=0.5), 0.2), swell_d + 0.25)
    place(mix, pan(felt_piano(note_hz('A#4'), 7.0, vel=0.35), -0.2), swell_d + 1.6)
    mix = reverb(mix, hall, wet=0.45)
    mix = fade(mix, 0.01, 3.0, sr=SR)
    mix = loudness(dc_block(mix), -17.0)
    save('SourceArt/Audio/Music/M_Title_Sting.wav', mix, sr=SR, category='Music', spatial=False)


def departure_sting():
    seed(2003)
    total = 10.0
    n = int(total * SR)
    mix = np.zeros((2, n))
    hall = make_ir(duration=5.0, decay=3.5, predelay=0.03, damping=4500, stereo=True, sr=SR)

    x = tt(6.0)
    engine_f = 38 + 14 * (x / 6.0) + 2 * np.sin(2 * np.pi * 7 * x)
    engine = np.sign(np.sin(2 * np.pi * np.cumsum(engine_f) / SR))
    engine = lp(engine, 180) * np.clip(x / 2.0, 0, 1) * 0.5
    place(mix, np.stack([engine, engine]), 0.0)
    rise = string_cluster([note_hz(nm) for nm in ('A2', 'A#2', 'E3', 'F3')], 6.0, attack=5.0, release=0.4)
    place(mix, rise * 0.9, 0.0)
    place(mix, np.stack([boom(4.0), boom(4.0)]), 6.0)
    mix = reverb(mix, hall, wet=0.4)
    mix = fade(mix, 0.3, 2.5, sr=SR)
    mix = loudness(dc_block(mix), -17.0)
    save('SourceArt/Audio/Music/M_Departure_Sting.wav', mix, sr=SR, category='Music', spatial=False)


# ---------------------------------------------------------------------------------------
# Radio station A: late-night jazz trio

def jazz_radio():
    seed(2004)
    bpm = 84.0
    beat = 60.0 / bpm
    swing = 0.62  # long-short eighths
    bars = 32
    total = bars * 4 * beat + 3.0
    n = int(total * SR)
    mono = np.zeros(n)

    progression = [
        ('F2', ['G#3', 'C4', 'D#4', 'G4']),     # Fm9
        ('F2', ['G#3', 'C4', 'D#4', 'G4']),
        ('A#1', ['C#4', 'F4', 'G#4', 'C5']),    # Bbm9
        ('D#2', ['G3', 'C#4', 'F4', 'C5']),     # Eb13
        ('G#1', ['G3', 'C4', 'D#4', 'G4']),     # Abmaj7
        ('C#2', ['F3', 'G#3', 'C4', 'F4']),     # Dbmaj7
        ('G2', ['F3', 'A#3', 'C#4', 'F4']),     # Gm7b5
        ('C2', ['E3', 'A#3', 'C#4', 'G4']),     # C7b9
    ]

    def eighth(beat_index, second_half):
        return beat_index * beat + (swing * beat if second_half else 0.0)

    for b in range(bars):
        root, voicing = progression[b % len(progression)]
        bar_start = b * 4 * beat
        # Rhodes comping: "2&" and "4" with occasional anticipation.
        hits = [(1, True, 0.5), (3, False, 0.42)] if b % 2 == 0 else [(0, False, 0.45), (2, True, 0.4)]
        for bi, half, vel in hits:
            start = bar_start + eighth(bi, half) + H.RNG.uniform(-0.01, 0.015)
            for nm in voicing:
                place(mono, fm_epiano(note_hz(nm), 1.6, vel * H.RNG.uniform(0.85, 1.0)), start, gain=0.28)
        # Walking bass.
        r = note_hz(root)
        fifth = r * 1.4983
        third = r * (1.1892 if b % 8 in (0, 1, 2, 6) else 1.2599)
        next_root = note_hz(progression[(b + 1) % len(progression)][0])
        walk = [r, third, fifth, next_root * 0.9439]
        for bi, f in enumerate(walk):
            place(mono, upright_bass(f, beat * 0.95, 0.75), bar_start + bi * beat, gain=0.9)
        # Drums: ride swing pattern, hi-hat chick on 2/4, brush swirl.
        for bi in range(4):
            place(mono, ride(1.0, 0.33 if bi % 2 == 0 else 0.4), bar_start + bi * beat)
            if bi % 2 == 1:
                place(mono, ride(0.6, 0.22), bar_start + eighth(bi, True))
                place(mono, hp(brush_hit(0.12, 0.25), 3000), bar_start + bi * beat)
            if bi == 0 and b % 4 == 0:
                place(mono, kick_soft(0.4, 0.35), bar_start)
    swirl = bp(white(total), 2500, 8500) * (0.6 + 0.4 * np.abs(np.sin(2 * np.pi * (bpm / 60 / 2) * tt(total)))) * 0.035
    mono += swirl

    # Vibraphone melody in the 2nd and 4th chorus.
    melody = ['C5', 'D#5', 'F5', 'G5', 'G#5', 'G5', 'F5', 'D#5', 'C5', 'A#4', 'G#4', 'G4', 'F4', 'G4', 'G#4', 'C5']
    for chorus in (1, 3):
        for i, nm in enumerate(melody):
            start = (chorus * 8) * 4 * beat + i * 2 * beat + (swing * beat if i % 3 == 1 else 0)
            place(mono, vibes(note_hz(nm), 2.2, 0.5), start, gain=0.35)

    room = make_ir(duration=1.8, decay=1.0, predelay=0.01, damping=6000, stereo=False, sr=SR)
    mono = reverb(mono, room, wet=0.25)
    mono = radio(mono, sr=SR, band=(260, 3900), drive=1.8, hiss=0.012)
    mono = make_loop(mono, 2.5, sr=SR)
    mono = loudness(dc_block(mono), -20.0)
    save('SourceArt/Audio/Music/M_Radio_Station_A.wav', mono, sr=SR, category='Music', loop=True, radius=1300)


# ---------------------------------------------------------------------------------------
# Radio station B: the Sunday-night organ hour

def hymn_radio():
    seed(2005)
    bpm = 62.0
    beat = 60.0 / bpm
    measure = 3 * beat
    hymn = [  # (chord tones, melody, measures)
        (['C3', 'G3', 'C4', 'E4'], 'G4', 1), (['F3', 'A3', 'C4', 'F4'], 'A4', 1),
        (['C3', 'G3', 'C4', 'E4'], 'G4', 1), (['G2', 'G3', 'B3', 'D4'], 'D4', 1),
        (['A2', 'E3', 'A3', 'C4'], 'E4', 1), (['F2', 'F3', 'A3', 'C4'], 'F4', 1),
        (['G2', 'F3', 'B3', 'D4'], 'D4', 1), (['C3', 'G3', 'C4', 'E4'], 'C4', 2),
        (['E3', 'G#3', 'B3', 'E4'], 'B4', 1), (['A2', 'E3', 'A3', 'C4'], 'C5', 1),
        (['F2', 'F3', 'A3', 'D4'], 'A4', 1), (['C3', 'G3', 'C4', 'E4'], 'G4', 1),
        (['D3', 'F3', 'A3', 'D4'], 'F4', 1), (['G2', 'F3', 'B3', 'D4'], 'D4', 1),
        (['C3', 'E3', 'G3', 'C4'], 'C4', 2),
    ]
    verses = 2
    total = sum(m for _, _, m in hymn) * measure * verses + 4.0
    n = int(total * SR)
    stereo = np.zeros((2, n))
    pos = 0.0
    for verse in range(verses):
        for chord, mel, measures in hymn:
            d = measures * measure
            stereo_ch = organ([note_hz(nm) for nm in chord], d + 0.15)
            place(stereo, stereo_ch, pos, gain=0.8)
            place(stereo, organ([note_hz(mel)], d + 0.15, drawbars=(0.0, 1.0, 0.0, 0.8, 0.4, 0.5, 0.2, 0.3, 0.15)), pos, gain=0.65)
            if verse == 1:
                for nm in chord[1:]:
                    place(stereo, pan(choir_aah(note_hz(nm), d + 0.6), H.RNG.uniform(-0.5, 0.5)), pos, gain=0.06)
            pos += d
    church = make_ir(duration=4.5, decay=3.2, predelay=0.04, damping=4000, stereo=True, sr=SR)
    stereo = reverb(stereo, church, wet=0.5)
    mono = stereo.mean(axis=0)
    mono = radio(mono, sr=SR, band=(300, 3300), drive=2.4, wow=0.004, hiss=0.02)
    mono = make_loop(mono, 3.0, sr=SR)
    mono = loudness(dc_block(mono), -21.0)
    save('SourceArt/Audio/Music/M_Radio_Station_B.wav', mono, sr=SR, category='Music', loop=True, radius=1300)


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    jobs = [hideout_theme, title_sting, departure_sting, jazz_radio, hymn_radio]
    only = set(sys.argv[1:])
    for job in jobs:
        if only and job.__name__ not in only:
            continue
        print('->', job.__name__, flush=True)
        job()
    write_manifest('SourceArt/Audio/manifest.json')
    print('done')
