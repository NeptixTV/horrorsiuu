"""
Fifth audio batch: the crew's gear in use - instant camera flash, walkie mark chirp, salt being
poured, the noise meter's tick. Synthesized from scratch with hhsynth and the helpers of
gen_sfx / gen_world_sfx. No recordings, no samples.

Run from the HorrorHeist project folder:
    python Tools/AssetGen/audio/gen_gear_sfx.py
    python Tools/AssetGen/audio/gen_gear_sfx.py --list
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hhsynth as H  # noqa: E402
from hhsynth import bp, env_exp, hp, lp, modal, place, reverb, seed, white  # noqa: E402
import gen_world_sfx as W  # noqa: E402
import gen_sfx as S  # noqa: E402

H.SR = 48000


def camera_flash():
    """Instant camera: the flash capacitor whines up, shutter clack, flash pop, motor pushes the print out."""
    seed(7010)
    d = 1.9
    x = np.zeros(W.N(d))
    tt = H.t(0.35)
    whine = np.sin(2 * np.pi * np.cumsum(np.interp(tt, [0, 0.35], [2500, 7200])) / H.SR) * np.clip(tt / 0.05, 0, 1) * 0.08
    place(x, whine, 0.0)
    place(x, modal([2100, 3600, 5200], [0.012, 0.008, 0.005], [1, 0.6, 0.3], 0.06) * 0.9, 0.36)   # shutter
    place(x, hp(white(0.05), 3000) * env_exp(0.05, 0.006) * 0.6, 0.365)                           # flash pop
    motor_t = H.t(0.9)
    motor = (np.sign(np.sin(2 * np.pi * 95 * motor_t)) * 0.3 + bp(white(0.9), 300, 2500) * 0.5)
    motor *= np.clip(motor_t / 0.05, 0, 1) * np.clip((0.9 - motor_t) / 0.08, 0, 1)
    place(x, lp(motor, 3500) * 0.25, 0.55)
    x = reverb(x, S.SMALL, wet=0.15)
    W.emit('SFX/S_Camera_Flash_01.wav', W.one_shot(x, -18, 0.002, 0.15), 'SFX', radius=1500,
           caption='[Camera flash]')


def walkie_chirp():
    """Squelch burst, two-tone roger beep, squelch tail (played for the whole crew)."""
    seed(7020)
    d = 0.75
    x = np.zeros(W.N(d))
    place(x, bp(white(0.12), 400, 3500) * np.clip(np.linspace(1, 0, W.N(0.12)) * 1.2, 0, 1) * 0.5, 0.0)
    for at, f in ((0.12, 1450.0), (0.22, 1050.0)):
        tt = H.t(0.09)
        place(x, np.sin(2 * np.pi * f * tt) * np.clip(tt / 0.004, 0, 1) * np.clip((0.09 - tt) / 0.01, 0, 1) * 0.5, at)
    place(x, bp(white(0.25), 400, 3500) * np.linspace(0.6, 0, W.N(0.25)) * 0.35, 0.32)
    x = H.radio(x, band=(400, 3200), drive=1.6, hiss=0.01, crackle=False)
    W.emit('UI/S_Walkie_Chirp_01.wav', W.one_shot(x, -20, 0.001, 0.08), 'UI', spatial=False,
           caption='[Walkie click]')


def salt_pour():
    """Coarse salt poured in a line on floorboards."""
    seed(7030)
    d = 1.6
    tt = H.t(d)
    grains = np.zeros(W.N(d))
    rate = np.interp(tt, [0, 0.15, 1.2, d], [0, 900, 700, 0])
    for i in range(W.N(d)):
        if H.RNG.random() < rate[i] / H.SR:
            grains[i] = H.RNG.uniform(0.3, 1.0) * (1 if H.RNG.random() < 0.5 else -1)
    g = bp(grains, 2500, 11000) + bp(white(d), 3000, 9000) * 0.05 * (rate / 900)
    x = reverb(g, S.ROOM, wet=0.15)
    W.emit('SFX/S_Salt_Pour_01.wav', W.one_shot(x, -24, 0.01, 0.2), 'SFX', radius=900,
           caption='[Salt trickles onto the floor]')


def meter_tick():
    """The noise meter's dry relay tick (2D, the faster the closer)."""
    seed(7040)
    d = 0.08
    x = modal([3100, 5200], [0.006, 0.004], [1, 0.5], d) + hp(white(d), 4000) * env_exp(d, 0.002) * 0.4
    W.emit('UI/S_NoiseMeter_Tick_01.wav', W.one_shot(x, -24, 0.0005, 0.02), 'UI', spatial=False)


JOBS = [camera_flash, walkie_chirp, salt_pour, meter_tick]


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
