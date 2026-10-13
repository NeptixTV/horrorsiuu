"""
Third audio batch: the drivable van, the patrol car, the hideout's garage door and the neighbour.
Synthesized from scratch with hhsynth and the helpers of gen_world_sfx (same rooms, levels and
manifest handling). No recordings, no samples.

Run from the HorrorHeist project folder:
    python Tools/AssetGen/audio/gen_vehicle_sfx.py            # generate everything
    python Tools/AssetGen/audio/gen_vehicle_sfx.py van_horn   # only some jobs
    python Tools/AssetGen/audio/gen_vehicle_sfx.py --list

New entries are merged into SourceArt/Audio/manifest.json (see gen_world_sfx.merge_manifest).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hhsynth as H  # noqa: E402
from hhsynth import bp, env_exp, hp, lp, modal, place, reverb, seed, white  # noqa: E402
import gen_world_sfx as W  # noqa: E402

H.SR = 48000


def _loop_crossfade(x, n, fade_len):
    """First n samples of x as a seamless loop: the overshoot after n is crossfaded into the start."""
    y = x[:n].copy()
    k = np.linspace(0.0, 1.0, fade_len)
    y[:fade_len] = x[:fade_len] * np.sqrt(k) + x[n:n + fade_len] * np.sqrt(1.0 - k)
    return y


def van_engine_loop():
    """Steady running engine for the drivable van (pitch rises with speed in the game)."""
    seed(5010)
    L = 2.0
    d = L + 0.4
    rpm = np.full(W.N(d), 1250.0) * (1 + 0.012 * (W.S.smooth_noise(d, 2.5) - 0.5))
    load = np.full(W.N(d), 0.45)
    x = W.engine(d, rpm, load, rough=0.06)
    x += W.u(lp(H.brown(d), 120)) * 0.25
    x = reverb(lp(x, 6500), W.STREET, wet=0.12)
    y = _loop_crossfade(x, W.N(L), W.N(0.35))
    W.emit('SFX/S_Van_Engine_Loop.wav', W.finish(y, -20, loop=True), 'SFX', loop=True, radius=2500)


def van_horn():
    """Two-tone car horn (a little tired, like the van)."""
    seed(5020)
    d = 0.9
    tt = H.t(d)
    env = np.clip(tt / 0.012, 0, 1) * np.clip((d - tt) / 0.06, 0, 1)
    tone = np.zeros(W.N(d))
    for f0, a in ((392.0, 1.0), (494.0, 0.85)):
        ph = np.cumsum(f0 * (1 + 0.003 * np.sin(2 * np.pi * 6.5 * tt))) / H.SR
        tone += a * sum(np.sin(2 * np.pi * k * ph) / k ** 0.9 for k in range(1, 12))
    x = bp(tone, 250, 4500) * env
    x = np.tanh(x * 1.6)
    x = reverb(x, W.OUTDOOR, wet=0.35)
    W.emit('SFX/S_Van_Horn_01.wav', W.one_shot(x, -16, 0.002, 0.12), 'SFX', radius=6000, caption='[A horn blares]')


def police_siren_loop():
    """Close siren (wail): the frequency sweeps once per loop with a whole number of cycles."""
    seed(5030)
    L = 4.0
    tt = H.t(L)
    sweep = 0.5 - 0.5 * np.cos(2 * np.pi * tt / L)
    f = 760.0 + 760.0 * sweep
    f *= 1100.0 * L / (np.sum(f) / H.SR)  # mean frequency x loop length = whole number of cycles
    ph = np.cumsum(f) / H.SR
    tone = sum(a * np.sin(2 * np.pi * k * ph) for k, a in ((1, 1.0), (2, 0.3), (3, 0.45), (4, 0.12), (5, 0.2)))
    x = W.circ(lp, tone, 3200)
    x = W.creverb(x, W.STREET, wet=0.25)
    W.emit('SFX/S_Police_Siren_Loop.wav', W.finish(x, -17, loop=True), 'SFX', loop=True, radius=9000,
           caption='[Police siren]')


def garage_door_motor():
    """Roll-up door: motor whine, chain and slats rattling up into the drum, a clunk at the end."""
    seed(5040)
    d = 3.6
    tt = H.t(d)
    run = np.clip(tt / 0.15, 0, 1) * np.clip((3.2 - tt) / 0.12, 0, 1)
    ph = np.cumsum(118.0 * (1 + 0.02 * np.sin(2 * np.pi * 3.0 * tt))) / H.SR
    motor = sum(np.sin(2 * np.pi * k * ph) / k for k in range(1, 9)) * run
    chain = np.zeros(W.N(d))
    t0 = 0.1
    while t0 < 3.15:
        place(chain, modal([2300, 3900, 6100], [0.006, 0.004, 0.003], [1, 0.6, 0.3], 0.03) * H.RNG.uniform(0.3, 0.7), t0)
        t0 += H.RNG.uniform(0.035, 0.07)
    slats = bp(white(d), 400, 2500) * (0.5 + 0.5 * np.abs(np.sin(2 * np.pi * 7.0 * tt))) * run
    x = W.u(lp(motor, 1800)) * 0.6 + W.u(chain) * 0.35 + W.u(slats) * 0.25
    place(x, modal([95, 180, 420], [0.12, 0.08, 0.04], [1, 0.6, 0.3], 0.4) * 1.2, 3.22)
    x = reverb(x, W.SMALL, wet=0.25)
    W.emit('SFX/S_Garage_Door_Motor_01.wav', W.one_shot(x, -18, 0.01, 0.2), 'SFX', radius=2500,
           caption='[The garage door rattles]')


def car_bump():
    """Van body hitting something: a dull metal thump with a bit of rattle."""
    seed(5050)
    d = 0.7
    x = np.zeros(W.N(d))
    body = modal([72, 118, 190, 305, 470], [0.12, 0.09, 0.06, 0.04, 0.03], [1, 0.8, 0.55, 0.35, 0.25], 0.6)
    place(x, body, 0.0)
    place(x, lp(white(0.08), 1400) * env_exp(0.08, 0.01) * 0.8, 0.0)
    place(x, bp(white(0.3), 1500, 5000) * env_exp(0.3, 0.06) * 0.15, 0.02)
    x = reverb(x, W.STREET, wet=0.2)
    W.emit('SFX/S_Car_Bump_01.wav', W.one_shot(x, -15, 0.0005, 0.12), 'SFX', radius=3000)


def neighbour_shout():
    """A man shouting "Hey!" from a porch: aspirated onset into a formant-filtered voiced vowel."""
    seed(5060)
    d = 0.85
    tt = H.t(d)
    f0 = np.interp(tt, [0, 0.08, 0.35, d], [150, 175, 160, 120])
    ph = np.cumsum(f0) / H.SR
    glottal = sum(np.sin(2 * np.pi * k * ph) / k ** 1.4 for k in range(1, 40))
    voiced_env = np.interp(tt, [0, 0.07, 0.12, 0.45, 0.62, d], [0, 0, 1, 0.85, 0, 0])
    formants = np.interp(tt, [0, 0.15, 0.5], [0, 0, 1])  # "eh" gliding to "ee"
    x = np.zeros(W.N(d))
    for (fa, fb), gain in (((560, 420), 1.0), ((1750, 2150), 0.55), ((2550, 2750), 0.3)):
        fc = fa + (fb - fa) * formants.mean()
        x += bp(glottal, fc * 0.85, fc * 1.15) * gain
    x *= voiced_env
    breath = bp(white(d), 900, 3500) * np.interp(tt, [0, 0.02, 0.1, 0.14], [0, 1, 0.8, 0]) * 0.6
    x = W.u(x) + W.u(breath) * 0.35
    x = reverb(x, W.OUTDOOR, wet=0.35)
    W.emit('SFX/S_Neighbour_Shout_01.wav', W.one_shot(x, -17, 0.002, 0.15), 'SFX', radius=4000,
           caption='[The neighbour: "Hey! Who\'s out there?"]')


JOBS = [van_engine_loop, van_horn, police_siren_loop, garage_door_motor, car_bump, neighbour_shout]


def main(argv):
    os.chdir(os.path.join(HERE, '..', '..', '..'))
    names = [j.__name__ for j in JOBS]
    if '--list' in argv:
        print('\n'.join(names))
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
