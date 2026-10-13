"""
Fourth audio batch: the new haunting events (humming, a child's giggle, crying, scratching in
the walls, the telephone, piano notes, a running tap, the touch, the scream, the growl, the
rocking chair, a hand on glass, falling objects, writing, close footsteps) plus the playground
swing. Synthesized from scratch with hhsynth and the helpers of gen_sfx / gen_world_sfx. No
recordings, no samples.

Run from the HorrorHeist project folder:
    python Tools/AssetGen/audio/gen_haunt_sfx.py            # generate everything
    python Tools/AssetGen/audio/gen_haunt_sfx.py hum giggle # only some jobs
    python Tools/AssetGen/audio/gen_haunt_sfx.py --list

New entries are merged into SourceArt/Audio/manifest.json (see gen_world_sfx.merge_manifest).
"""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hhsynth as H  # noqa: E402
from hhsynth import bp, env_exp, hp, lp, modal, note_hz, peak, place, resonator, reverb, seed, white  # noqa: E402
import gen_world_sfx as W  # noqa: E402
import gen_sfx as S  # noqa: E402

H.SR = 48000
ROOM, SMALL, STAIR = S.ROOM, S.SMALL, S.STAIR


# =======================================================================================
# Voice helpers

def glottal(f0_curve, bright=1.0, jitter=0.004, shimmer=0.05):
    """Band-limited glottal source following a pitch curve (Hz per sample)."""
    n = len(f0_curve)
    f0 = f0_curve * (1 + jitter * (S.smooth_noise(n / H.SR, 40) - 0.5) * 2)
    ph = np.cumsum(f0) / H.SR
    y = np.zeros(n)
    top = int(min(60, (H.SR * 0.45) / max(1.0, float(np.max(f0)))))
    for k in range(1, top + 1):
        y += np.sin(2 * np.pi * k * ph) / k ** (1.0 + 0.6 / bright)
    amp = 1 + shimmer * (S.smooth_noise(n / H.SR, 25) - 0.5) * 2
    return y * amp


def formants(src, freqs, qs=(6, 9, 11), gains=(1.0, 0.5, 0.25)):
    return sum(g * resonator(src, f, q=q) for f, q, g in zip(freqs, qs, gains))


# =======================================================================================
# Jobs

def hum():
    """A woman humming the music-box lullaby somewhere in the house, closed-mouth, a bit off."""
    seed(6010)
    tune = [(0.9, 'E4'), (0.9, 'A4'), (0.9, 'C5'), (1.35, 'B4'), (0.45, 'A4'), (0.9, 'G#4'),
            (0.9, 'A4'), (0.9, 'E4'), (0.9, 'F4'), (2.2, 'E4')]
    d = sum(b for b, _ in tune) + 1.4
    n = W.N(d)
    f0 = np.zeros(n)
    env = np.zeros(n)
    at = 0.25
    for i, (beats, nm) in enumerate(tune):
        i0, i1 = int(at * H.SR), int((at + beats) * H.SR)
        f = note_hz(nm) * H.detune_ratio(H.RNG.uniform(-18, 6))
        prev = f0[i0 - 1] if i0 > 0 and f0[i0 - 1] > 0 else f
        seg = np.full(i1 - i0, f)
        glide = min(len(seg), int(0.07 * H.SR))
        seg[:glide] = np.linspace(prev, f, glide)
        f0[i0:i1] = seg
        e = np.ones(i1 - i0)
        e[:int(0.06 * H.SR)] = np.linspace(0.35, 1, int(0.06 * H.SR))
        if i == len(tune) - 1:
            e *= np.linspace(1, 0, len(e)) ** 1.5
        env[i0:i1] = e
        at += beats
    f0[f0 == 0] = note_hz('E4')
    tt = H.t(d)
    f0 *= 1 + 0.012 * np.sin(2 * np.pi * 5.3 * tt) * np.clip((tt % 1.0) / 0.4, 0, 1)
    src = glottal(f0, bright=0.6)
    # Closed mouth: a strong nasal murmur, the rest swallowed.
    x = formants(src, (260, 900, 2300), qs=(5, 7, 9), gains=(1.0, 0.18, 0.05))
    x = lp(x, 1600) * W.lp(env, 30)
    x += lp(white(d), 700) * W.lp(env, 30) * 0.03  # breath through the nose
    x = reverb(x, ROOM, wet=0.45)
    x = H.muffle_through_floor(x) * 0.4 + lp(x, 2500) * 0.6  # another room
    W.emit('Ambience/A_Haunt_Hum_01.wav', W.one_shot(x, -24, 0.05, 0.6), 'Dialogue', radius=1800,
           caption='[Someone is humming a lullaby]')


def giggle():
    """A small child's giggle, a few rooms away."""
    seed(6020)
    d = 1.6
    x = np.zeros(W.N(d))
    at = 0.08
    count = 6
    for i in range(count):
        dur = H.RNG.uniform(0.07, 0.1)
        n = W.N(dur)
        tt = np.arange(n) / H.SR
        f = (430 - 22 * i) * H.RNG.uniform(0.97, 1.04)
        f0 = f * (1 + 0.08 * np.sin(np.pi * tt / dur)) * np.linspace(1.03, 0.95, n)
        voiced = formants(glottal(f0, bright=1.2), (380, 2600, 3400), qs=(5, 8, 10), gains=(1, 0.55, 0.3))
        aspir = bp(white(dur + 0.04), 1200, 5000)
        syl = np.zeros(W.N(dur + 0.04))
        syl[:W.N(0.04)] += aspir[:W.N(0.04)] * np.linspace(0, 1, W.N(0.04)) * 0.15
        syl[W.N(0.03):W.N(0.03) + n] += W.u(voiced) * np.sin(np.pi * tt / dur) ** 0.7
        place(x, syl * (1 - 0.1 * i), at)
        at += dur + H.RNG.uniform(0.045, 0.07)
    x = reverb(lp(x, 6000), STAIR, wet=0.5)
    W.emit('Ambience/A_Haunt_Giggle_01.wav', W.one_shot(x, -24, 0.004, 0.4), 'Dialogue', radius=2000,
           caption='[A child giggles]')


def crying():
    """Quiet sobbing: sniffles, broken whimpers, breath catching."""
    seed(6030)
    d = 5.5
    x = np.zeros(W.N(d))
    at = 0.2
    while at < d - 0.9:
        if H.RNG.random() < 0.35:
            sn = bp(white(0.18), 2000, 7000) * np.sin(np.linspace(0, np.pi, W.N(0.18))) ** 2
            place(x, sn * 0.25, at)
            at += 0.3
            continue
        dur = H.RNG.uniform(0.35, 0.8)
        n = W.N(dur)
        tt = np.arange(n) / H.SR
        f = H.RNG.uniform(290, 340)
        f0 = f * np.interp(tt, [0, dur * 0.2, dur], [1.08, 1.0, 0.82])
        f0 *= 1 + 0.03 * np.sin(2 * np.pi * 7.5 * tt)  # the shake in the voice
        src = glottal(f0, bright=0.8) * 0.6 + white(dur)[:n] * 0.25
        v = formants(src, (560, 1100, 2500), qs=(6, 8, 10))
        e = np.clip(tt / 0.05, 0, 1) * np.clip((dur - tt) / 0.15, 0, 1)
        e *= 1 - 0.7 * (np.sin(2 * np.pi * 6.0 * tt) > 0.6)  # sob breaks
        place(x, W.u(v) * e * H.RNG.uniform(0.6, 1.0), at)
        at += dur + H.RNG.uniform(0.25, 0.6)
        inh = bp(white(0.25), 600, 3500) * np.sin(np.linspace(0, np.pi, W.N(0.25))) * 0.25
        place(x, inh, at - 0.2)
    x = reverb(lp(x, 5500), ROOM, wet=0.4)
    W.emit('Ambience/A_Haunt_Crying_01.wav', W.one_shot(x, -25, 0.02, 0.5), 'Dialogue', radius=1600,
           caption='[Someone is crying quietly]')


def scratch():
    """Nails dragged inside the wall, slow strokes, muffled by plaster."""
    seed(6040)
    d = 4.2
    x = np.zeros(W.N(d))
    at = 0.1
    while at < d - 0.6:
        dur = H.RNG.uniform(0.35, 0.7)
        rate = np.linspace(H.RNG.uniform(180, 260), H.RNG.uniform(90, 140), W.N(dur))
        exc = S.stick_slip(dur, rate, jitter=0.5)
        s = sum(a * resonator(exc, f, q=6) for f, a in ((900, 1.0), (1700, 0.7), (2900, 0.45), (4300, 0.2)))
        s += bp(white(dur), 1500, 6000) * 0.08
        e = np.sin(np.pi * np.linspace(0, 1, W.N(dur))) ** 0.6
        place(x, s * e, at)
        at += dur + H.RNG.uniform(0.15, 0.5)
    x = peak(lp(x, 2600, order=3), 400, 1.0, 3)
    x = reverb(x, SMALL, wet=0.25)
    W.emit('Ambience/A_Haunt_Scratch_01.wav', W.one_shot(x, -24, 0.01, 0.3), 'Ambient', radius=1400,
           caption='[Scratching inside the wall]')


def phone_ring_loop():
    """An old bell telephone: 2 s of ringing, 4 s of silence, seamless."""
    seed(6050)
    L = 6.0
    x = np.zeros(W.N(L))
    bell = lambda f: modal([f, f * 2.76, f * 5.4, f * 8.9], [0.6, 0.35, 0.15, 0.06], [1, 0.5, 0.3, 0.12], 0.9)
    b1, b2 = bell(1180.0), bell(1415.0)
    at = 0.0
    k = 0
    while at < 2.0:
        place(x, (b1 if k % 2 == 0 else b2) * 0.5, at)
        at += 1.0 / 20.0  # the clapper hammers at 20 Hz between the two bells
        k += 1
    x = W.circ(lp, x, 7000)
    x = W.creverb(x, ROOM, wet=0.25)
    W.emit('Ambience/A_Haunt_Phone_Ring_Loop.wav', W.finish(x, -22, loop=True), 'SFX', loop=True, radius=2500,
           caption='[A telephone rings]')


def phone_voice():
    """What answers on the line: a backwards whisper and a slow, deep breath through the receiver."""
    seed(6060)
    w = S.whisper(2.6)[::-1]
    d = 3.6
    x = np.zeros(W.N(d))
    place(x, W.u(w) * 0.8, 0.3)
    tt = H.t(1.4)
    growl = formants(glottal(np.full(len(tt), 48.0) * (1 + 0.1 * np.sin(2 * np.pi * 0.7 * tt)), bright=0.5),
                     (320, 800, 2200), qs=(4, 6, 8))
    place(x, W.u(growl) * np.sin(np.pi * tt / 1.4) * 0.6, 2.1)
    x = H.radio(x, band=(350, 3000), drive=2.8, hiss=0.03)
    W.emit('SFX/S_Haunt_Phone_Voice_01.wav', W.one_shot(x, -20, 0.01, 0.3), 'Dialogue', radius=500,
           caption='[A voice on the line: "...get... out..."]')


def piano_note(f, dur, vel):
    tt = H.t(dur)
    y = np.zeros_like(tt)
    B = 0.0004
    for k in range(1, 12):
        fk = f * k * np.sqrt(1 + B * k * k)
        if fk > H.SR * 0.45:
            break
        dec = 2.2 / (1 + 0.35 * k)
        for det in (-0.6, 0.6):
            y += (1.0 / k ** 1.1) * np.exp(-tt / dec) * np.sin(2 * np.pi * fk * H.detune_ratio(det) * tt)
    hammer = lp(white(0.02), 3000) * env_exp(0.02, 0.003)
    y[:len(hammer)] += hammer * 0.4
    return y * vel


def piano():
    """Three slow notes from an out-of-tune upright, then one held cluster."""
    seed(6070)
    d = 6.5
    x = np.zeros(W.N(d))
    for at, nm, v in ((0.1, 'D#4', 0.6), (1.1, 'A3', 0.5), (2.1, 'E4', 0.55)):
        place(x, piano_note(note_hz(nm) * H.detune_ratio(H.RNG.uniform(-25, 25)), 3.5, v), at)
    for nm in ('C3', 'C#3', 'G3'):
        place(x, piano_note(note_hz(nm) * H.detune_ratio(H.RNG.uniform(-20, 20)), 3.8, 0.45), 3.4)
    x = reverb(lp(x, 7000), ROOM, wet=0.4)
    W.emit('Ambience/A_Haunt_Piano_01.wav', W.one_shot(x, -23, 0.002, 0.8), 'Ambient', radius=2500,
           caption='[Piano keys, in another room]')


def faucet_loop():
    """A tap running into a steel sink: noisy stream plus bubble blips, seamless."""
    seed(6080)
    L = 4.0
    stream = W.cnoise(L, 'pink')
    stream = W.circ(bp, stream, 300, 6000)
    x = W.u(stream) * 0.6
    n = W.N(L)
    for _ in range(int(L * 35)):
        f = H.RNG.uniform(500, 2200)
        dur = 0.025
        tt = H.t(dur)
        blip = np.sin(2 * np.pi * f * (1 + 2.5 * tt) * tt) * np.exp(-tt / 0.006)
        i = H.RNG.integers(0, n)
        for j, v in enumerate(blip):
            x[(i + j) % n] += v * H.RNG.uniform(0.05, 0.15)
    x = W.creverb(x, ROOM, wet=0.3)
    W.emit('Ambience/A_Haunt_Faucet_Loop.wav', W.finish(x, -24, loop=True), 'Ambient', loop=True, radius=1200,
           caption='[A tap is running]')


def touch():
    """Something brushes past you: a cold intake of air, cloth, and a breath at the ear (2D)."""
    seed(6090)
    d = 1.8
    x = np.zeros(W.N(d))
    whoosh = bp(white(0.9), 250, 2500) * np.sin(np.linspace(0, np.pi, W.N(0.9))) ** 2
    place(x, whoosh * 0.6, 0.0)
    cloth = bp(white(0.25), 1500, 6000) * env_exp(0.25, 0.06) * 0.35
    place(x, cloth, 0.25)
    br = bp(white(0.7), 500, 3500) * np.sin(np.linspace(0, np.pi, W.N(0.7))) ** 1.5
    place(x, br * 0.5, 0.9)
    x = lp(x, 8000)
    W.emit('SFX/S_Haunt_Touch_01.wav', W.one_shot(x, -20, 0.01, 0.2), 'SFX', radius=400,
           caption='[Something touches your shoulder]')


def scream():
    """The close scream when it lunges: a hoarse 'haaah' that cracks, with a hiss behind it."""
    seed(6100)
    d = 1.3
    tt = H.t(d)
    f0 = np.interp(tt, [0, 0.08, 0.5, d], [260, 520, 470, 300]) * (1 + 0.04 * np.sin(2 * np.pi * 11 * tt))
    src = glottal(f0, bright=1.6, jitter=0.03, shimmer=0.2)
    v = formants(src, (800, 1250, 2700), qs=(5, 6, 8), gains=(1, 0.8, 0.45))
    rasp = bp(white(d), 700, 5000) * 0.6
    env = np.clip(tt / 0.03, 0, 1) * np.clip((d - tt) / 0.5, 0, 1)
    x = np.tanh((W.u(v) + W.u(rasp) * 0.5) * 2.5) * env
    x = reverb(x, SMALL, wet=0.2)
    W.emit('SFX/S_Haunt_Scream_01.wav', W.one_shot(x, -14, 0.001, 0.2), 'SFX', radius=1500,
           caption='[A scream, right in your face]')


def growl():
    """Low guttural growl when it stands in front of you."""
    seed(6110)
    d = 2.0
    tt = H.t(d)
    rate = np.interp(tt, [0, 0.4, 1.5, d], [26, 38, 30, 20])
    pulses = S.stick_slip(d, rate, jitter=0.35)
    v = formants(lp(pulses, 1500), (300, 650, 1900), qs=(4, 5, 7), gains=(1, 0.7, 0.3))
    v += formants(glottal(np.full(len(tt), 55.0), bright=0.4), (300, 650, 1900)) * 0.3
    env = np.clip(tt / 0.15, 0, 1) * np.clip((d - tt) / 0.6, 0, 1)
    x = lp(W.u(v) * env, 3000)
    x = reverb(x, SMALL, wet=0.15)
    W.emit('SFX/S_Haunt_Growl_01.wav', W.one_shot(x, -16, 0.01, 0.3), 'SFX', radius=1200,
           caption='[A low growl]')


def rocking_loop():
    """A rocking chair on floorboards, no one in it: two creaks per swing, seamless."""
    seed(6120)
    L = 4.4
    n = W.N(L)
    x = np.zeros(n)
    for at in (0.1, 1.2, 2.3, 3.4):
        c = S.wood_creak(0.45, 30, 18, H.RNG.uniform(0.9, 1.1)) * H.RNG.uniform(0.6, 1.0)
        thump = modal([95, 190], [0.05, 0.03], [1, 0.4], 0.2) * 0.25
        for buf, off in ((c, 0.0), (thump, 0.3)):
            i = int((at + off) * H.SR)
            for j in range(len(buf)):
                x[(i + j) % n] += buf[j]
    x = W.creverb(x, ROOM, wet=0.3)
    W.emit('Ambience/A_Haunt_Rocking_Loop.wav', W.finish(x, -25, loop=True), 'Ambient', loop=True, radius=1200,
           caption='[A rocking chair creaks]')


def glass_squeak():
    """A wet hand dragged down a window pane."""
    seed(6130)
    d = 1.3
    rate = np.interp(H.t(d), [0, 0.5, d], [300, 520, 260])
    exc = S.stick_slip(d, rate, jitter=0.08)
    x = sum(a * resonator(exc, f, q=25) for f, a in ((1250, 1.0), (2480, 0.6), (3700, 0.3)))
    x *= np.sin(np.pi * np.linspace(0, 1, W.N(d))) ** 0.5
    x = reverb(x, ROOM, wet=0.25)
    W.emit('Ambience/A_Haunt_Glass_Squeak_01.wav', W.one_shot(x, -24, 0.01, 0.2), 'Ambient', radius=1200,
           caption='[Something squeaks across the glass]')


def writing():
    """Letters scratched into plaster: short hard strokes."""
    seed(6140)
    d = 2.8
    x = np.zeros(W.N(d))
    at = 0.1
    for _ in range(9):
        dur = H.RNG.uniform(0.08, 0.22)
        exc = S.stick_slip(dur, np.full(W.N(dur), H.RNG.uniform(250, 420)), jitter=0.6)
        s = sum(a * resonator(exc, f, q=5) for f, a in ((1400, 1.0), (2600, 0.8), (4800, 0.4)))
        place(x, s * np.sin(np.pi * np.linspace(0, 1, W.N(dur))), at)
        at += dur + H.RNG.uniform(0.06, 0.25)
    x = reverb(lp(x, 7000), ROOM, wet=0.3)
    W.emit('Ambience/A_Haunt_Writing_01.wav', W.one_shot(x, -24, 0.005, 0.3), 'Ambient', radius=1200,
           caption='[Scratching on the wall]')


def thuds():
    """Thrown things landing: a book, a mug, a picture frame."""
    seed(6150)
    d = 1.4
    book = np.zeros(W.N(d))
    place(book, modal([110, 230, 480], [0.04, 0.03, 0.015], [1, 0.6, 0.3], 0.3), 0.0)
    place(book, bp(white(0.25), 1500, 7000) * env_exp(0.25, 0.05) * 0.25, 0.01)  # pages flutter
    place(book, modal([140, 300], [0.03, 0.02], [0.5, 0.2], 0.2) * 0.4, 0.22)
    mug = np.zeros(W.N(d))
    place(mug, modal([1650, 3900, 6200], [0.15, 0.08, 0.04], [1, 0.5, 0.25], 0.6) * 0.7, 0.0)
    place(mug, modal([180, 420], [0.03, 0.02], [1, 0.4], 0.2) * 0.6, 0.0)
    for k in range(6):  # rolls and wobbles
        place(mug, modal([1650, 3900], [0.03, 0.02], [1, 0.4], 0.1) * 0.15 * (1 - k / 6), 0.25 + k * 0.11)
    frame = np.zeros(W.N(d))
    place(frame, modal([210, 520, 980], [0.05, 0.03, 0.02], [1, 0.6, 0.35], 0.3), 0.0)
    place(frame, modal([2400, 5100, 7300], [0.05, 0.03, 0.02], [0.5, 0.3, 0.2], 0.3) * 0.5, 0.005)
    place(frame, modal([230, 540], [0.03, 0.02], [0.6, 0.3], 0.2) * 0.5, 0.18)
    for k, (x, name) in enumerate(((book, 'Book'), (mug, 'Mug'), (frame, 'Frame'))):
        x = reverb(x, ROOM, wet=0.25)
        W.emit(f'SFX/S_Haunt_Throw_{name}_01.wav', W.one_shot(x, -18, 0.001, 0.2), 'SFX', radius=2200,
               caption='[Something crashes to the floor]')


def close_steps():
    """Four single footsteps on old floorboards, for steps that walk up behind you."""
    for k in range(4):
        seed(6160 + k)
        d = 0.6
        x = np.zeros(W.N(d))
        s = H.RNG.uniform(0.92, 1.08)
        step = modal(np.array([92, 160, 280, 430]) * s, [0.06, 0.04, 0.03, 0.02], [1.0, 0.6, 0.35, 0.15], 0.4)
        step += lp(white(0.4), 1800) * env_exp(0.4, 0.008) * 0.5
        place(x, step, 0.01)
        if k % 2 == 0:
            place(x, S.wood_creak(0.3, 30, 45, s) * 0.25, 0.05)
        x = reverb(x, ROOM, wet=0.22)
        W.emit(f'Ambience/A_Haunt_Step_Close_{k + 1:02d}.wav', W.one_shot(x, -21, 0.001, 0.15), 'Ambient', radius=1500)


def swing_loop():
    """An empty swing chain squeaking on its hook (the playground at night), seamless."""
    seed(6170)
    L = 3.6
    n = W.N(L)
    x = np.zeros(n)
    for at in (0.2, 2.0):
        dur = 0.6
        rate = np.interp(H.t(dur), [0, 0.3, dur], [180, 330, 160])
        exc = S.stick_slip(dur, rate, jitter=0.1)
        sq = sum(a * resonator(exc, f, q=18) for f, a in ((1050, 1.0), (2150, 0.5), (3350, 0.25)))
        sq *= np.sin(np.pi * np.linspace(0, 1, len(sq)))
        i = int(at * H.SR)
        for j in range(len(sq)):
            x[(i + j) % n] += sq[j] * (1.0 if at < 1 else 0.8)
    x = W.creverb(x, W.OUTDOOR, wet=0.35)
    W.emit('Ambience/A_Swing_Creak_Loop.wav', W.finish(x, -26, loop=True), 'Ambient', loop=True, radius=2000,
           caption='[A swing creaks]')


JOBS = [hum, giggle, crying, scratch, phone_ring_loop, phone_voice, piano, faucet_loop, touch, scream, growl,
        rocking_loop, glass_squeak, writing, thuds, close_steps, swing_loop]


def main(argv):
    os.chdir(os.path.join(HERE, '..', '..', '..'))
    names = [j.__name__ for j in JOBS]
    if '--list' in argv:
        print('\n'.join(names))
        return 0
    only = {a for a in argv if not a.startswith('--')}
    unknown = only - set(names)
    if unknown:
        print('unknown job(s):', ', '.join(sorted(unknown)))
        return 2
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
