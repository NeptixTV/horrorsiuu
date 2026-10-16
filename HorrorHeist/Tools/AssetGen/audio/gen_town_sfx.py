"""
Sixth audio batch: the living town - police radio at a crime scene, a playground swing's chain,
children playing in Willow Park, the supermarket's background music, the bank's quiet hall and
the horn of the town's cars. Synthesized from scratch with hhsynth and the helpers of
gen_sfx / gen_world_sfx / gen_music. No recordings, no samples.

Run from the HorrorHeist project folder:
    python Tools/AssetGen/audio/gen_town_sfx.py
    python Tools/AssetGen/audio/gen_town_sfx.py --list
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hhsynth as H  # noqa: E402
from hhsynth import bp, hp, lp, modal, resonator, seed, white  # noqa: E402
import gen_world_sfx as W  # noqa: E402
import gen_sfx as S  # noqa: E402
import gen_music as M  # noqa: E402

H.SR = 48000


def police_radio_loop():
    """Dispatch chatter from a patrol car's radio: squelch, clipped voices, long gaps (loops)."""
    seed(8010)
    L = 18.0
    x = np.zeros(W.N(L))
    at = 0.6
    while at < L - 2.5:
        d = H.RNG.uniform(1.0, 2.6)
        f0 = H.RNG.choice([96.0, 112.0, 128.0, 180.0])
        v = S.voice_like(d, f0)
        burst = np.concatenate([bp(white(0.06), 500, 3000) * 0.6, v, bp(white(0.12), 500, 3000) * np.linspace(0.5, 0, W.N(0.12))])
        burst = H.radio(burst, band=(350, 3000), drive=2.6, hiss=0.02, crackle=True)
        W.cplace(x, burst, at)
        at += d + H.RNG.uniform(1.8, 4.5)
    x += bp(W.cnoise(L, 'white'), 400, 3000) * 0.004
    x = W.creverb(x, S.SMALL, wet=0.15)
    W.emit('Ambience/A_Police_Radio_Loop.wav', W.finish(x, -27, loop=True), 'Ambient', loop=True, radius=1400,
           caption='[Police radio chatter]')


def swing_creak_loop():
    """A playground swing: the chain shackles squeak at both turning points (one period = 2.8 s)."""
    seed(8020)
    L = 2.8
    x = np.zeros(W.N(L))
    for k, at in enumerate((0.15, 0.15 + L / 2)):
        d = H.RNG.uniform(0.35, 0.5)
        tt = H.t(d)
        rate = np.interp(tt, [0, d * 0.5, d], [260, 520, 300]) * (1.0 if k == 0 else 1.12)
        exc = S.stick_slip(d, rate, jitter=0.2)
        y = np.zeros(W.N(d))
        for f, q, a in ((1150, 18, 1.0), (2300, 22, 0.6), (3420, 25, 0.35), (5100, 30, 0.15)):
            y += a * resonator(exc, f * (1.0 if k == 0 else 0.94), q=q)
        y *= np.sin(np.pi * np.clip(tt / d, 0, 1)) ** 0.8
        W.cplace(x, hp(y, 600), at)
    x = W.creverb(x, W.OUTDOOR, wet=0.25)
    W.emit('SFX/A_Swing_Creak_Loop.wav', W.finish(x, -26, loop=True), 'SFX', loop=True, radius=1800,
           caption='[A swing creaks]')


def _child_voice(d, f0, vowel_seq, shout=0.0):
    """A child's voice: glottal pulses with vibrato and pitch glides through vowel formants."""
    n = W.N(d)
    tt = np.arange(n) / H.SR
    glide = 1 + 0.18 * np.sin(np.pi * np.clip(tt / d, 0, 1)) * (1 + shout) + 0.03 * np.sin(2 * np.pi * 5.5 * tt)
    pitch = f0 * glide
    ph = np.cumsum(pitch) / H.SR
    src = np.zeros(n)
    src[np.where(np.diff(np.floor(ph)) > 0)[0]] = 1.0
    src = lp(src, 6000) + white(d) * 0.03
    block = max(1, n // len(vowel_seq))
    y = np.zeros(n)
    for i, v in enumerate(vowel_seq):
        seg = src[i * block:(i + 1) * block]
        if not len(seg):
            continue
        out = np.zeros_like(seg)
        for f, a in zip(CHILD_FORMANTS[v], (1.0, 0.7, 0.35)):
            out += a * resonator(seg, f, q=7)
        y[i * block:i * block + len(out)] = out
    env = np.clip(tt / 0.03, 0, 1) * np.clip((d - tt) / 0.08, 0, 1)
    return y * env


CHILD_FORMANTS = {'a': (1030, 1700, 3700), 'e': (690, 2600, 3600), 'i': (420, 3100, 3900), 'o': (680, 1250, 3400),
                  'u': (470, 1150, 3300)}


def playground_loop():
    """Willow Park by day: children calling and laughing across the playground, far and near (loops)."""
    seed(8030)
    L = 24.0
    x = np.zeros(W.N(L))
    at = 0.4
    while at < L - 1.5:
        kind = H.RNG.random()
        if kind < 0.4:
            # Laugh: "ha-ha-ha", falling pitch.
            f0 = H.RNG.uniform(300, 380)
            for k in range(int(H.RNG.integers(3, 7))):
                syl = _child_voice(0.11, f0 * (1 - 0.04 * k), ['a'])
                W.cplace(x, syl, at + k * 0.16, gain=H.RNG.uniform(0.15, 0.45) * (1 - 0.08 * k))
            at += H.RNG.uniform(1.0, 2.2)
        elif kind < 0.75:
            # A call across the park ("mo-om!", "o-kay!").
            d = H.RNG.uniform(0.5, 0.9)
            seq = [H.RNG.choice(list(CHILD_FORMANTS)) for _ in range(2)]
            W.cplace(x, _child_voice(d, H.RNG.uniform(280, 360), seq, shout=0.6), at, gain=H.RNG.uniform(0.1, 0.35))
            at += H.RNG.uniform(1.4, 3.0)
        else:
            # Chatter, a few syllables.
            for k in range(int(H.RNG.integers(3, 6))):
                seq = [H.RNG.choice(list(CHILD_FORMANTS))]
                W.cplace(x, _child_voice(0.14, H.RNG.uniform(260, 330), seq), at + k * 0.18, gain=H.RNG.uniform(0.06, 0.18))
            at += H.RNG.uniform(1.6, 3.4)
    x = W.circ(lp, x, 5000)
    x = W.creverb(x, W.OUTDOOR_FAR, wet=0.55, dry=0.6)
    x += W.circ(bp, W.cnoise(L, 'pink'), 120, 2500) * 0.004
    W.emit('Ambience/A_Playground_Loop.wav', W.finish(x, -32, loop=True), 'Ambient', loop=True, radius=4000,
           caption='[Children playing]')


def market_muzak_loop():
    """Pick & Save's ceiling speakers: a soft electric-piano tune over a bass line (loops; 44.1 kHz)."""
    with W.music_rate():
        seed(8040)
        bpm = 92.0
        beat = 60.0 / bpm
        bars = 8
        L = bars * 4 * beat
        n = int(L * M.SR)
        x = np.zeros(n)
        prog = [('C4', 'E4', 'G4', 'B4'), ('A3', 'C4', 'E4', 'G4'), ('F3', 'A3', 'C4', 'E4'), ('G3', 'B3', 'D4', 'F4')] * 2
        roots = ['C2', 'A1', 'F1', 'G1'] * 2
        melody = ['E5', 'G5', 'A5', 'G5', 'E5', 'D5', 'C5', 'D5', 'E5', 'C5', 'A4', 'C5', 'D5', 'B4', 'G4', 'B4']
        for bar, (chord, root) in enumerate(zip(prog, roots)):
            t0 = bar * 4 * beat
            for k in (0, 2):
                for i, note in enumerate(chord):
                    y = M.fm_epiano(H.note_hz(note), 2 * beat, vel=0.32)
                    W.cplace(x, y, t0 + k * beat + i * 0.012)
            for k, mult in ((0, 1.0), (2, 1.0), (3, 1.5)):
                W.cplace(x, M.upright_bass(H.note_hz(root) * mult, beat * 0.95, vel=0.5), t0 + k * beat)
            for k in range(2):
                note = melody[(bar * 2 + k) % len(melody)]
                W.cplace(x, M.vibes(H.note_hz(note), 1.8 * beat, vel=0.28), t0 + (k * 2 + 1) * beat)
        x = W.circ(bp, x, 180, 5500)                        # small ceiling speakers
        x = W.creverb(x, S.ROOM, wet=0.35)
        x += W.circ(lp, W.cnoise(L, 'pink'), 2000) * 0.0025
        W.emit('Ambience/A_Market_Muzak_Loop.wav', W.finish(x, -30, loop=True), 'Music', loop=True, radius=2600)


def bank_roomtone_loop():
    """The bank hall: ventilation, a wall clock ticking, a murmur at the counters, paper (loops)."""
    seed(8051)
    L = 12.0
    x = W.circ(bp, W.cnoise(L, 'pink'), 90, 2500) * 0.25
    x += W.circ(bp, W.cnoise(L, 'brown'), 60, 160) * 0.25
    for k in range(int(L)):
        W.cplace(x, S.clock_tick_sound(tock=k % 2 == 1) * 0.12, k + 0.5)
    for at, f0 in ((1.3, 110.0), (7.3, 190.0)):
        d = 2.0
        v = S.voice_like(d, f0)
        W.cplace(x, lp(v, 1800) * 0.12, at)
    for at in (4.2, 10.2):
        rustle = bp(white(0.4), 1500, 7000) * np.abs(np.sin(np.linspace(0, 7 * np.pi, W.N(0.4)))) * 0.08
        W.cplace(x, rustle, at)
    x = W.creverb(x, S.STAIR, wet=0.3)
    W.emit('Ambience/A_Bank_Roomtone_Loop.wav', W.finish(x, -35, loop=True), 'Ambient', loop=True, radius=1800)


def car_horn():
    """A town car's horn: two short beeps, higher and thinner than the van's."""
    seed(8060)
    d = 0.75
    x = np.zeros(W.N(d))
    for at, length in ((0.0, 0.16), (0.26, 0.34)):
        tt = H.t(length)
        tone = np.zeros(W.N(length))
        for f0, a in ((440.0, 1.0), (554.0, 0.8)):
            ph = np.cumsum(f0 * (1 + 0.002 * np.sin(2 * np.pi * 7 * tt))) / H.SR
            tone += a * sum(np.sin(2 * np.pi * k * ph) / k for k in range(1, 10))
        env = np.clip(tt / 0.008, 0, 1) * np.clip((length - tt) / 0.03, 0, 1)
        H.place(x, np.tanh(bp(tone, 300, 5000) * 1.8) * env, at)
    x = H.reverb(x, W.OUTDOOR, wet=0.3)
    W.emit('SFX/S_Car_Horn_01.wav', W.one_shot(x, -17, 0.002, 0.1), 'SFX', radius=5000, caption='[A car honks]')


JOBS = [police_radio_loop, swing_creak_loop, playground_loop, market_muzak_loop, bank_roomtone_loop, car_horn]


def main(argv):
    os.chdir(os.path.join(HERE, '..', '..', '..'))
    if '--list' in argv:
        print('\n'.join(j.__name__ for j in JOBS))
        return 0
    only = {a for a in argv if not a.startswith('--')}
    for job in JOBS:
        if only and job.__name__ not in only:
            continue
        print('->', job.__name__, flush=True)
        job()
    merged = W.merge_manifest(W.MANIFEST_PATH, H.MANIFEST)
    print('done', len(H.MANIFEST), 'files;', len(merged), 'manifest entries')
    return 1 if W.analyse([e['file'] for e in H.MANIFEST]) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
