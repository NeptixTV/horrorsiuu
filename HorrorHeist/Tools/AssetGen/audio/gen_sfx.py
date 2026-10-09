"""
Generates ambience, sound effects, footsteps and UI sounds into SourceArt/Audio.
Run from the HorrorHeist project folder:  python Tools/AssetGen/audio/gen_sfx.py
"""
import os
import sys
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
from hhsynth import *  # noqa: E402,F401,F403
import hhsynth as H  # noqa: E402

OUT = 'SourceArt/Audio'
ROOM = make_ir(duration=1.6, decay=0.75, predelay=0.006, damping=7000, stereo=False)
SMALL = make_ir(duration=0.8, decay=0.35, predelay=0.003, damping=8000, stereo=False)
STAIR = make_ir(duration=2.4, decay=1.4, predelay=0.015, damping=5000, stereo=False)
HALL = make_ir(duration=3.5, decay=2.2, predelay=0.02, damping=4500, stereo=True)


def out(*parts):
    return os.path.join(OUT, *parts)


def smooth_noise(duration, rate, sr=SR):
    """Slowly varying random curve in [0,1] (for gusts, intensities)."""
    n = int(duration * sr)
    points = max(int(duration * rate) + 3, 4)
    knots = H.RNG.uniform(0, 1, points)
    xs = np.linspace(0, n, points)
    curve = np.interp(np.arange(n), xs, knots)
    return lp(curve, rate * 2, order=1)


def stick_slip(duration, rate_curve, sr=SR, jitter=0.25):
    """Pulse train with jittered spacing: the excitation of creaks and squeaks."""
    n = int(duration * sr)
    x = np.zeros(n)
    i = 0.0
    while i < n:
        rate = max(rate_curve[min(int(i), n - 1)], 1.0)
        amp = H.RNG.uniform(0.4, 1.0)
        x[int(i)] += amp
        i += sr / rate * (1 + H.RNG.uniform(-jitter, jitter))
    return x


# =======================================================================================
# Ambience
# =======================================================================================

def rain_outside():
    seed(11)
    dur = 46.0
    n = int(dur * SR)
    gust = 0.55 + 0.45 * smooth_noise(dur, 0.12)
    chans = []
    for c in range(2):
        hiss = bp(pink(dur), 500, 9500, order=2) * 0.55
        body = lp(brown(dur), 420) * 0.9
        # Individual drops on hard surfaces (gutters, bins, pavement).
        drops = np.zeros(n)
        count = int(dur * 260)
        for _ in range(count):
            i = H.RNG.integers(0, n - 400)
            f = H.RNG.uniform(1800, 6500)
            d = H.RNG.uniform(0.002, 0.008)
            tt = np.arange(400) / SR
            drops[i:i + 400] += H.RNG.uniform(0.05, 0.35) * np.exp(-tt / d) * np.sin(2 * np.pi * f * tt)
        mix = (hiss + body * 0.6 + drops * 0.5) * gust
        chans.append(mix)
    x = np.stack(chans)
    x = make_loop(x, 6.0)
    x = loudness(dc_block(x), -21)
    save(out('Ambience', 'A_Rain_Outside_Loop.wav'), x, category='Ambient', loop=True, spatial=False)


def rain_window():
    seed(12)
    dur = 24.0
    n = int(dur * SR)
    x = lp(pink(dur), 2600) * 0.35
    # Drops hitting glass.
    for _ in range(int(dur * 22)):
        i = H.RNG.integers(0, n - 4000)
        partials = np.array([2150, 3720, 5090, 6890]) * H.RNG.uniform(0.85, 1.2)
        hit = modal(partials, [0.012, 0.009, 0.007, 0.005], [1, 0.6, 0.35, 0.2], 0.08)
        x[i:i + len(hit)] += hit * H.RNG.uniform(0.05, 0.3)
    # Trickles down the pane.
    trickle = bp(white(dur), 380, 1300) * (0.4 + 0.6 * smooth_noise(dur, 1.5))
    trickle *= (H.RNG.uniform(0, 1, n) > 0.0) * (0.5 + 0.5 * np.sin(2 * np.pi * 9 * t(dur) + 4 * smooth_noise(dur, 0.8)))
    x += trickle * 0.12
    x = make_loop(x, 3.0)
    x = loudness(dc_block(x), -23)
    save(out('Ambience', 'A_Rain_Window_Loop.wav'), x, category='Ambient', loop=True, radius=900)


def room_tone():
    seed(13)
    dur = 32.0
    x = lp(brown(dur), 140) * 1.0
    x += bp(pink(dur), 180, 700) * 0.08
    x += 0.04 * sine(60, dur) * (0.8 + 0.2 * smooth_noise(dur, 0.2))
    x = make_loop(x, 4.0)
    x = loudness(dc_block(x), -30)
    save(out('Ambience', 'A_RoomTone_Basement_Loop.wav'), x, category='Ambient', loop=True, spatial=False)


def fluorescent_hum():
    seed(14)
    dur = 8.0  # integer number of 100 Hz cycles: loops without crossfade
    tt = t(dur)
    x = 0.5 * np.sin(2 * np.pi * 100 * tt) + 0.25 * np.sin(2 * np.pi * 200 * tt) + 0.12 * np.sin(2 * np.pi * 300 * tt) + 0.06 * np.sin(2 * np.pi * 400 * tt)
    pulses = (np.sin(2 * np.pi * 100 * tt) > 0.97).astype(float)
    buzz = bp(pulses, 1800, 7000) * 0.6
    x = x * 0.3 + buzz
    x *= 0.9 + 0.1 * np.sin(2 * np.pi * 0.25 * tt)
    x = loudness(x, -24)
    save(out('Ambience', 'A_Hum_Fluorescent_Loop.wav'), x, category='Ambient', loop=True, radius=700)


def boiler_hum():
    seed(15)
    dur = 20.0
    tt = t(dur)
    motor = 0.5 * np.sin(2 * np.pi * 50 * tt) + 0.2 * np.sin(2 * np.pi * 100 * tt) + 0.15 * np.sin(2 * np.pi * 25 * tt)
    roar = lp(white(dur), 220) * (0.7 + 0.3 * np.sin(2 * np.pi * 0.35 * tt))
    rattle = bp(white(dur), 900, 2500) * (np.sin(2 * np.pi * 25 * tt) > 0.9) * 0.15
    x = motor * 0.25 + roar * 1.2 + rattle
    x = make_loop(x, 2.0)
    x = loudness(dc_block(x), -25)
    save(out('Ambience', 'A_Hum_Boiler_Loop.wav'), x, category='Ambient', loop=True, radius=1100)


def tv_static():
    seed(16)
    dur = 10.0
    tt = t(dur)
    x = bp(white(dur), 250, 9000) * 0.5
    x *= 0.85 + 0.15 * (np.sin(2 * np.pi * 60 * tt) > 0)
    x += 0.006 * np.sin(2 * np.pi * 15734 * tt)  # CRT flyback whine
    x = make_loop(x, 1.0)
    x = loudness(x, -26)
    save(out('Ambience', 'A_TV_Static_Loop.wav'), x, category='Ambient', loop=True, radius=800)


def radio_static():
    seed(17)
    dur = 12.0
    tt = t(dur)
    x = bp(white(dur), 300, 3600) * 0.6
    whistle = np.sin(2 * np.pi * np.cumsum(900 + 250 * np.sin(2 * np.pi * 0.05 * tt)) / SR) * 0.04
    x += whistle
    x = radio(x, wow=0.001)
    x = make_loop(x, 1.5)
    x = loudness(x, -24)
    save(out('Ambience', 'A_Radio_Static_Loop.wav'), x, category='Music', loop=True, radius=900)


def clock_tick_sound(tock=False):
    partials = np.array([2800, 4150, 6300]) * (0.82 if tock else 1.0)
    click = modal(partials, [0.006, 0.004, 0.003], [1, 0.6, 0.3], 0.05)
    body = modal([410 * (0.9 if tock else 1.0), 820], [0.03, 0.015], [0.5, 0.2], 0.08)
    s = np.zeros(int(0.12 * SR))
    s[: len(click)] += click
    s[: len(body)] += body
    return s


def clock_loop():
    seed(18)
    x = np.zeros(int(2.0 * SR))
    place(x, clock_tick_sound(False), 0.01)
    place(x, clock_tick_sound(True), 1.01)
    x = reverb(x, SMALL, wet=0.25)
    x = loudness(x, -26)
    save(out('Ambience', 'A_Clock_Tick_Loop.wav'), x, category='Ambient', loop=True, radius=600)


def wind_gusts():
    for k in range(3):
        seed(30 + k)
        dur = H.RNG.uniform(5, 8)
        tt = t(dur)
        env = np.sin(np.pi * np.clip(tt / dur, 0, 1)) ** 1.5
        f0 = H.RNG.uniform(420, 700)
        freq = f0 * (1 + 0.25 * smooth_noise(dur, 0.8))
        noise = white(dur)
        x = np.zeros_like(noise)
        block = 1024
        for s in range(0, len(x), block):
            x[s:s + block] = resonator(noise[s:s + block], float(np.mean(freq[s:s + block])), q=14)
        x = x * 0.6 + lp(pink(dur), 500) * 0.4
        x = fade(x * env, 0.2, 0.5)
        x = loudness(x, -24)
        save(out('Ambience', f'A_Wind_Gust_{k + 1:02d}.wav'), x, category='Ambient', radius=1600)


def drips():
    for k in range(6):
        seed(40 + k)
        dur = 0.7
        tt = t(dur)
        f0 = H.RNG.uniform(650, 1100)
        rise = f0 * (1 + 1.2 * (1 - np.exp(-tt / 0.012)))
        plink = np.sin(2 * np.pi * np.cumsum(rise) / SR) * np.exp(-tt / H.RNG.uniform(0.04, 0.09))
        splash = hp(white(dur), 2500) * np.exp(-tt / 0.006) * 0.25
        x = plink + splash
        x = reverb(x, ROOM, wet=0.35)
        x = fade(loudness(x, -22), 0.001, 0.1)
        save(out('Ambience', f'A_Drip_{k + 1:02d}.wav'), x, category='Ambient', radius=900)


def pipe_creaks():
    for k in range(4):
        seed(50 + k)
        dur = H.RNG.uniform(1.2, 2.8)
        n = int(dur * SR)
        exc = np.zeros(n)
        # Thermal ticks.
        for _ in range(H.RNG.integers(2, 7)):
            i = H.RNG.integers(0, n - 100)
            exc[i] += H.RNG.uniform(0.5, 1.0)
        # A short groan.
        g0 = H.RNG.uniform(0.1, 0.5) * dur
        gl = H.RNG.uniform(0.3, 0.9)
        groan = stick_slip(gl, np.linspace(H.RNG.uniform(25, 40), H.RNG.uniform(50, 80), int(gl * SR)))
        exc[int(g0 * SR):int(g0 * SR) + len(groan)] += groan[: max(0, n - int(g0 * SR))] * 0.5
        partials = np.array([640, 1335, 2120, 3070, 4220]) * H.RNG.uniform(0.85, 1.15)
        x = np.zeros(n)
        for f, q in zip(partials, [60, 70, 80, 90, 100]):
            x += resonator(exc, f, q=q)
        x = reverb(x, ROOM, wet=0.4)
        x = fade(loudness(x, -24), 0.002, 0.3)
        save(out('Ambience', f'A_PipeCreak_{k + 1:02d}.wav'), x, category='Ambient', radius=1400)


def wood_creak(dur, rate_from, rate_to, body=1.0):
    n = int(dur * SR)
    tt = np.arange(n) / SR
    wobble = 1 + 0.15 * np.sin(2 * np.pi * H.RNG.uniform(1.5, 3.0) * tt)
    rate = np.linspace(rate_from, rate_to, n) * wobble
    exc = stick_slip(dur, rate, jitter=0.15)
    x = np.zeros(n)
    for f, q, a in zip(np.array([185, 410, 760, 1210, 1830]) * body, [8, 10, 12, 14, 16], [1.0, 0.8, 0.5, 0.3, 0.15]):
        x += a * resonator(exc, f, q=q)
    env = np.sin(np.pi * np.clip(tt / dur, 0, 1)) ** 0.6
    return lp(x * env, 3500)


def wood_creaks():
    for k in range(3):
        seed(60 + k)
        dur = H.RNG.uniform(0.8, 1.8)
        x = wood_creak(dur, H.RNG.uniform(14, 25), H.RNG.uniform(30, 55), H.RNG.uniform(0.9, 1.2))
        x = reverb(x, ROOM, wet=0.3)
        x = fade(loudness(x, -23), 0.01, 0.2)
        save(out('Ambience', f'A_WoodCreak_{k + 1:02d}.wav'), x, category='Ambient', radius=1300)


def thunder():
    specs = [(1, 0.6, True), (2, 1.0, True), (3, 2.2, False), (4, 3.4, False), (5, 4.5, False)]
    for k, distance, close in specs:
        seed(70 + k)
        dur = H.RNG.uniform(9, 13)
        n = int(dur * SR)
        tt = np.arange(n) / SR
        chans = []
        for c in range(2):
            rumble = lp(brown(dur), 160 / (1 + distance * 0.3)) * 1.4
            mids = lp(pink(dur), 900 / (1 + distance * 0.5)) * 0.4
            # Clustered rolls.
            env = np.zeros(n)
            # Close strikes roll immediately after the crack; distant ones swell in.
            first = H.RNG.uniform(0.02, 0.15) if close else H.RNG.uniform(0.2, 0.8)
            env += np.exp(-np.clip(tt - first, 0, None) / H.RNG.uniform(1.2, 2.2)) * (tt >= first) * 0.9
            for _ in range(H.RNG.integers(5, 10)):
                start = H.RNG.uniform(0.1, dur * 0.6)
                length = H.RNG.uniform(0.6, 2.5)
                e = np.exp(-np.clip(tt - start, 0, None) / length) * (tt >= start)
                env += e * H.RNG.uniform(0.3, 1.0)
            env = lp(env, 8, order=1)
            x = (rumble + mids) * env
            if close:
                crack = hp(white(0.5), 600) * np.exp(-t(0.5) / 0.05)
                crack = np.concatenate([crack, np.zeros(n - len(crack))])
                x += crack * (1.5 if c == 0 else 1.3)
            chans.append(x)
        x = np.stack(chans)
        x = reverb(x, HALL, wet=0.3)
        x = fade(x, 0.02 if close else 0.4, 2.0)
        x = loudness(dc_block(x), -18 if close else -21)
        save(out('Ambience', f'A_Thunder_{k:02d}.wav'), x, category='Ambient', spatial=False,
             caption='[Thunder]')


def overhead_steps():
    for k in range(6):
        seed(80 + k)
        dur = 0.9
        thud = modal([62, 118, 205], [0.09, 0.06, 0.04], [1.0, 0.6, 0.3], dur)
        knock = lp(white(dur), 900) * env_exp(dur, 0.02) * 0.4
        x = thud + knock
        if H.RNG.random() < 0.5:
            creak = wood_creak(0.35, 18, 35, 0.8) * 0.4
            x[int(0.05 * SR):int(0.05 * SR) + len(creak)] += creak
        x = muffle_through_floor(x)
        x = reverb(x, ROOM, wet=0.25)
        x = fade(loudness(x, -19), 0.001, 0.2)
        save(out('Ambience', f'A_Overhead_Step_{k + 1:02d}.wav'), x, category='Ambient', radius=2200)


def door_slam_distant():
    for k in range(2):
        seed(90 + k)
        dur = 2.5
        hit = modal([88, 176, 322, 540], [0.25, 0.15, 0.09, 0.05], [1, 0.7, 0.4, 0.2], dur)
        hit += lp(white(dur), 1500) * env_exp(dur, 0.03) * 0.5
        rattle = bp(white(dur), 1500, 4000) * env_exp(dur, 0.12) * (np.sin(2 * np.pi * 31 * t(dur)) > 0.6) * 0.2
        x = muffle_through_floor(hit + rattle)
        x = reverb(x, STAIR, wet=0.6)
        x = fade(loudness(x, -20), 0.001, 0.4)
        save(out('Ambience', f'A_DoorSlam_Distant_{k + 1:02d}.wav'), x, category='Ambient', radius=3000,
             caption='[A door slams somewhere above]')


def knock():
    seed(95)
    one = lambda: modal(np.array([215, 470, 905, 1520]) * H.RNG.uniform(0.95, 1.05), [0.09, 0.06, 0.04, 0.02], [1, 0.7, 0.4, 0.2], 0.4) \
        + hp(white(0.4), 2000) * env_exp(0.4, 0.004) * 0.3
    x = np.zeros(int(3.0 * SR))
    for i, at in enumerate([0.0, 0.62, 1.27]):
        place(x, one() * (1.0 if i < 2 else 1.15), at + 0.05)
    x = reverb(x, STAIR, wet=0.45)
    x = fade(loudness(x, -18), 0.001, 0.4)
    save(out('Ambience', 'A_Knock_Three.wav'), x, category='Ambient', radius=2600,
         caption='[Three slow knocks]')


FORMANTS = {
    'a': (730, 1090, 2440), 'e': (530, 1840, 2480), 'i': (290, 2290, 3010),
    'o': (570, 840, 2410), 'u': (300, 870, 2240), 'ah': (640, 1190, 2390),
}


def formant_filter(src, vowel_track, sr=SR, block=512):
    """Whisper-like voice: noise source through moving formants."""
    out_sig = np.zeros_like(src)
    for s in range(0, len(src), block):
        v = vowel_track[min(s // block, len(vowel_track) - 1)]
        seg = src[s:s + block]
        y = np.zeros_like(seg)
        for f, a in zip(FORMANTS[v], (1.0, 0.6, 0.3)):
            y += a * resonator(seg, f, q=8)
        out_sig[s:s + block] = y
    return out_sig


def whisper(duration, sr=SR):
    n = int(duration * sr)
    src = white(duration)
    vowels = list(FORMANTS.keys())
    blocks = n // 512 + 1
    track = []
    while len(track) < blocks:
        track += [vowels[H.RNG.integers(0, len(vowels))]] * H.RNG.integers(6, 16)
    voiced = formant_filter(src, track[:blocks])
    sibil = hp(white(duration), 4200) * 0.35
    # Syllables.
    env = np.zeros(n)
    pos = 0.0
    while pos < duration - 0.1:
        length = H.RNG.uniform(0.09, 0.22)
        a = H.RNG.uniform(0.4, 1.0)
        i0 = int(pos * sr)
        i1 = min(int((pos + length) * sr), n)
        seg = np.sin(np.linspace(0, np.pi, i1 - i0)) ** 1.2
        env[i0:i1] = np.maximum(env[i0:i1], seg * a)
        pos += length + H.RNG.uniform(0.0, 0.12)
    sib_env = (H.RNG.uniform(0, 1, n) > 0.0) * lp((H.RNG.uniform(0, 1, n) > 0.985).astype(float), 30) * 8
    x = voiced * env + sibil * np.clip(sib_env, 0, 1)
    return x


def whispers():
    for k in range(3):
        seed(100 + k)
        dur = H.RNG.uniform(2.2, 3.2)
        x = whisper(dur)
        if k == 2:
            x = x[::-1]  # one of them backwards
        x = reverb(x, ROOM, wet=0.35)
        x = fade(loudness(lp(x, 7000), -24), 0.05, 0.4)
        save(out('Ambience', f'A_Whisper_{k + 1:02d}.wav'), x, category='Dialogue', radius=900,
             caption='[Whispering]')


def electrical_stutter():
    seed(110)
    dur = 1.8
    tt = t(dur)
    gate = (smooth_noise(dur, 25) > 0.45).astype(float)
    buzz = (0.5 * np.sin(2 * np.pi * 100 * tt) + 0.3 * np.sign(np.sin(2 * np.pi * 100 * tt))) * gate
    crackle = np.zeros_like(tt)
    for _ in range(40):
        i = H.RNG.integers(0, len(tt) - 300)
        crackle[i:i + 300] += H.RNG.uniform(-1, 1) * np.exp(-np.arange(300) / 30)
    x = lp(buzz, 3000) * 0.5 + hp(crackle, 2000) * 0.6
    relay = modal([1900, 3100], [0.01, 0.006], [1, 0.5], 0.05)
    place(x, relay, 0.02)
    place(x, relay, dur - 0.1)
    x = reverb(x, ROOM, wet=0.2)
    x = fade(loudness(x, -20), 0.002, 0.1)
    save(out('Ambience', 'A_Electrical_Stutter.wav'), x, category='SFX', radius=1500,
         caption='[Electrical buzzing]')


def voice_like(duration, f0=118.0, sr=SR):
    """Buzzy formant voice speaking nonsense (for the radio bleed)."""
    n = int(duration * sr)
    tt = np.arange(n) / sr
    pitch = f0 * (1 + 0.08 * smooth_noise(duration, 3) - 0.1 * tt / duration)
    pulses = np.zeros(n)
    phase = np.cumsum(pitch) / sr
    pulses[np.where(np.diff(np.floor(phase)) > 0)[0]] = 1.0
    src = lp(pulses, 4000) + white(duration) * 0.02
    vowels = list(FORMANTS.keys())
    blocks = n // 512 + 1
    track = []
    while len(track) < blocks:
        track += [vowels[H.RNG.integers(0, len(vowels))]] * H.RNG.integers(5, 14)
    voiced = formant_filter(src, track[:blocks])
    env = 0.4 + 0.6 * (smooth_noise(duration, 6) > 0.35)
    return voiced * lp(env, 20)


def radio_bleed():
    seed(120)
    dur = 3.4
    v = voice_like(dur, 104)
    st = bp(white(dur), 300, 3500) * 0.6
    x = radio(v * 1.2 + st, drive=3.0)
    x = reverb(x, SMALL, wet=0.2)
    x = fade(loudness(x, -21), 0.2, 0.6)
    save(out('Ambience', 'A_Radio_Bleed.wav'), x, category='Dialogue', radius=1100,
         caption='[A voice inside the static]')


def radio_tune():
    seed(121)
    dur = 1.3
    tt = t(dur)
    whistle = np.sin(2 * np.pi * np.cumsum(np.linspace(2400, 600, len(tt)) + 300 * np.sin(2 * np.pi * 7 * tt)) / SR)
    st = bp(white(dur), 400, 3500) * (0.5 + 0.5 * (smooth_noise(dur, 14) > 0.5))
    x = radio(whistle * 0.25 + st, wow=0.0)
    x = fade(loudness(x, -22), 0.02, 0.1)
    save(out('SFX', 'S_Radio_Tune.wav'), x, category='SFX', radius=900)


# =======================================================================================
# Interaction sound effects
# =======================================================================================

def latch(scale=1.0):
    return modal(np.array([2450, 3980, 6100]) * scale, [0.02, 0.012, 0.008], [1, 0.6, 0.3], 0.1) \
        + hp(white(0.1), 3000) * env_exp(0.1, 0.003) * 0.4


def doors():
    seed(130)
    # Open: latch + short hinge creak.
    dur = 1.4
    x = np.zeros(int(dur * SR))
    place(x, latch(), 0.0)
    creak = wood_creak(0.75, 30, 70, 1.6) * 0.6
    place(x, creak, 0.12)
    place(x, lp(white(0.6), 600) * env_adsr(0.6, 0.2, 0.2, 0.3, 0.2) * 0.15, 0.1)
    x = reverb(x, ROOM, wet=0.3)
    save(out('SFX', 'S_Door_Wood_Open.wav'), fade(loudness(x, -19), 0.001, 0.2), radius=1500)

    seed(131)
    dur = 1.2
    x = np.zeros(int(dur * SR))
    thud = modal([82, 165, 310, 520], [0.18, 0.12, 0.07, 0.04], [1, 0.7, 0.35, 0.2], 0.8)
    place(x, thud + lp(white(0.8), 1200) * env_exp(0.8, 0.02) * 0.6, 0.0)
    place(x, latch(0.9) * 0.7, 0.03)
    x = reverb(x, ROOM, wet=0.35)
    save(out('SFX', 'S_Door_Wood_Close.wav'), fade(loudness(x, -17), 0.001, 0.2), radius=1800)

    seed(132)
    x = wood_creak(3.4, 9, 26, 1.35) * 0.9
    x += wood_creak(3.4, 12, 20, 2.1) * 0.25
    x = reverb(x, ROOM, wet=0.35)
    save(out('SFX', 'S_Door_Creak_Slow.wav'), fade(loudness(x, -21), 0.2, 0.5), radius=1600,
         caption='[A door creaks open]')


def lockers():
    seed(140)
    dur = 1.2
    x = np.zeros(int(dur * SR))
    clank = modal([610, 1130, 1870, 2950, 4100], [0.35, 0.25, 0.18, 0.12, 0.08], [1, 0.8, 0.6, 0.4, 0.2], 1.0)
    wobble = modal([142, 266], [0.25, 0.18], [0.8, 0.5], 1.0) * (1 + 0.5 * np.sin(2 * np.pi * 9 * t(1.0)))
    place(x, latch(0.8) * 0.8, 0.0)
    place(x, clank * 0.5 + wobble * 0.6, 0.08)
    squeak = stick_slip(0.4, np.linspace(250, 420, int(0.4 * SR)))
    place(x, resonator(squeak, 2300, q=25) * 0.4 + resonator(squeak, 3500, q=30) * 0.2, 0.1)
    x = reverb(x, ROOM, wet=0.3)
    save(out('SFX', 'S_Locker_Open.wav'), fade(loudness(x, -19), 0.001, 0.2), radius=1500)

    seed(141)
    x = np.zeros(int(dur * SR))
    place(x, clank * 0.9 + wobble, 0.0)
    place(x, latch(0.75), 0.02)
    x = reverb(x, ROOM, wet=0.3)
    save(out('SFX', 'S_Locker_Close.wav'), fade(loudness(x, -17), 0.001, 0.2), radius=1800)


def switches():
    seed(150)
    dur = 0.7
    body = modal([148, 320, 690], [0.12, 0.08, 0.05], [1, 0.6, 0.3], dur)
    click = np.zeros(int(dur * SR))
    place(click, latch(1.2), 0.0)
    x = reverb(body * 0.8 + click, ROOM, wet=0.25)
    save(out('SFX', 'S_Switch_Breaker.wav'), fade(loudness(x, -18), 0.001, 0.1), radius=1400)

    seed(151)
    x = np.zeros(int(0.25 * SR))
    place(x, modal([3100, 5200], [0.006, 0.004], [1, 0.5], 0.05) + hp(white(0.05), 2500) * env_exp(0.05, 0.002) * 0.4, 0.0)
    place(x, modal([2600, 4400], [0.005, 0.003], [0.7, 0.4], 0.05), 0.035)
    save(out('SFX', 'S_Switch_Click.wav'), fade(loudness(reverb(x, SMALL, wet=0.2), -21), 0.001, 0.05), radius=900)


def flashlight():
    seed(160)
    def click(scale, tink):
        x = np.zeros(int(0.3 * SR))
        place(x, modal(np.array([2900, 4700, 7200]) * scale, [0.006, 0.004, 0.003], [1, 0.6, 0.3], 0.05), 0.0)
        place(x, modal(np.array([2300, 3800]) * scale, [0.004, 0.003], [0.8, 0.4], 0.05), 0.028)
        if tink:
            place(x, modal([6800, 9100], [0.05, 0.03], [0.15, 0.08], 0.2), 0.03)
        return fade(loudness(reverb(x, SMALL, wet=0.15), -22), 0.001, 0.05)
    save(out('SFX', 'S_Flashlight_On.wav'), click(1.0, True), radius=700)
    save(out('SFX', 'S_Flashlight_Off.wav'), click(0.85, False), radius=700)


def van_door():
    seed(170)
    dur = 1.8
    tt = t(dur)
    roll_env = np.clip(tt / 0.15, 0, 1) * np.clip((1.25 - tt) / 0.1, 0, 1)
    roll = lp(brown(dur), 300) * roll_env * 1.2
    clack = bp(white(dur), 400, 2500) * (np.sin(2 * np.pi * 14 * tt) > 0.85) * roll_env * 0.4
    x = roll + clack
    thunk = modal([75, 140, 260, 610], [0.25, 0.15, 0.09, 0.05], [1, 0.8, 0.5, 0.3], 0.6) + lp(white(0.6), 2000) * env_exp(0.6, 0.02) * 0.5
    place(x, thunk, 1.22)
    place(x, latch(0.7), 1.25)
    x = reverb(x, ROOM, wet=0.35)
    save(out('SFX', 'S_Van_Door_Slide.wav'), fade(loudness(x, -17), 0.01, 0.2), radius=2000)


# =======================================================================================
# Footsteps
# =======================================================================================

def footsteps():
    dur = 0.45
    for k in range(6):
        seed(200 + k)
        heel = bp(white(dur), 180, 4200) * env_exp(dur, H.RNG.uniform(0.012, 0.02))
        thump = sine(H.RNG.uniform(80, 105), dur) * env_exp(dur, 0.035) * 0.8
        grit = np.zeros(int(dur * SR))
        for _ in range(H.RNG.integers(6, 14)):
            i = H.RNG.integers(int(0.003 * SR), int(0.08 * SR))
            grit[i] += H.RNG.uniform(-1, 1)
        grit = hp(grit, 3000) * 0.5
        scuff = bp(white(dur), 1200, 5500) * env_adsr(dur, 0.02, 0.05, 0.2, 0.08) * 0.12
        x = heel + thump + grit + scuff
        x = reverb(x, ROOM, wet=0.22)
        save(out('Footsteps', f'S_Footstep_Concrete_{k + 1:02d}.wav'), fade(loudness(x, -20), 0.0005, 0.08), category='SFX', radius=1300)

    for k in range(6):
        seed(210 + k)
        body = modal(np.array([108, 236, 418, 690]) * H.RNG.uniform(0.9, 1.1), [0.07, 0.05, 0.035, 0.02], [1, 0.6, 0.35, 0.2], dur)
        knock = lp(white(dur), 2500) * env_exp(dur, 0.01) * 0.5
        x = body + knock
        if H.RNG.random() < 0.35:
            creak = wood_creak(0.2, 30, 45, 1.1) * 0.25
            x[int(0.04 * SR):int(0.04 * SR) + len(creak)] += creak
        x = reverb(x, ROOM, wet=0.2)
        save(out('Footsteps', f'S_Footstep_Wood_{k + 1:02d}.wav'), fade(loudness(x, -20), 0.0005, 0.08), category='SFX', radius=1300)

    for k in range(6):
        seed(220 + k)
        ring = modal(np.array([520, 1180, 1960, 3150, 4600]) * H.RNG.uniform(0.92, 1.08), [0.16, 0.12, 0.09, 0.06, 0.04], [1, 0.7, 0.5, 0.3, 0.15], dur)
        clank = bp(white(dur), 800, 6000) * env_exp(dur, 0.008) * 0.6
        thump = sine(90, dur) * env_exp(dur, 0.03) * 0.5
        x = ring * 0.4 + clank + thump
        x = reverb(x, ROOM, wet=0.2)
        save(out('Footsteps', f'S_Footstep_Metal_{k + 1:02d}.wav'), fade(loudness(x, -20), 0.0005, 0.08), category='SFX', radius=1400)

    for k in range(4):
        seed(230 + k)
        thump = lp(white(dur), 650) * env_exp(dur, 0.03) + sine(85, dur) * env_exp(dur, 0.03) * 0.6
        rustle = bp(white(dur), 2000, 6500) * env_adsr(dur, 0.01, 0.06, 0.15, 0.06) * 0.08
        x = reverb(thump + rustle, SMALL, wet=0.15)
        save(out('Footsteps', f'S_Footstep_Fabric_{k + 1:02d}.wav'), fade(loudness(x, -24), 0.0005, 0.08), category='SFX', radius=900)

    for k in range(2):
        seed(240 + k)
        dur2 = 0.8
        thump = sine(70, dur2) * env_exp(dur2, 0.06) + lp(white(dur2), 1800) * env_exp(dur2, 0.025) * 0.7
        rustle = bp(white(dur2), 1500, 6000) * env_adsr(dur2, 0.02, 0.1, 0.2, 0.1) * 0.15
        x = reverb(thump + rustle, ROOM, wet=0.25)
        save(out('Footsteps', f'S_Land_{k + 1:02d}.wav'), fade(loudness(x, -17), 0.0005, 0.1), category='SFX', radius=1600)


# =======================================================================================
# UI
# =======================================================================================

def ui_sounds():
    seed(300)
    # Hover: barely-there tick.
    x = sine(2600, 0.06) * env_exp(0.06, 0.012) * 0.6 + hp(white(0.06), 4000) * env_exp(0.06, 0.002) * 0.3
    save(out('UI', 'UI_Hover.wav'), fade(loudness(x, -30), 0.0005, 0.02), category='UI', spatial=False)

    # Click: mechanical switch.
    x = np.zeros(int(0.12 * SR))
    place(x, modal([3200, 5100], [0.006, 0.004], [1, 0.5], 0.05), 0.0)
    place(x, modal([310], [0.02], [0.5], 0.05), 0.0)
    place(x, modal([2700, 4300], [0.004, 0.003], [0.5, 0.3], 0.05), 0.022)
    save(out('UI', 'UI_Click.wav'), fade(loudness(reverb(x, SMALL, wet=0.1), -24), 0.0005, 0.02), category='UI', spatial=False)

    # Back: soft downward thup.
    tt = t(0.18)
    x = np.sin(2 * np.pi * np.cumsum(np.linspace(520, 260, len(tt))) / SR) * env_exp(0.18, 0.05) + lp(white(0.18), 1500) * env_exp(0.18, 0.01) * 0.4
    save(out('UI', 'UI_Back.wav'), fade(loudness(x, -26), 0.0005, 0.03), category='UI', spatial=False)

    # Confirm: two warm notes.
    def tone(f, d):
        tt2 = t(d)
        return (np.sin(2 * np.pi * f * tt2) + 0.3 * np.sin(2 * np.pi * 2 * f * tt2) + 0.12 * np.sin(2 * np.pi * 3 * f * tt2)) * env_exp(d, d * 0.35, 0.004)
    x = np.zeros(int(0.7 * SR))
    place(x, tone(659.25, 0.5), 0.0)
    place(x, tone(987.77, 0.6) * 0.8, 0.07)
    x = reverb(x, SMALL, wet=0.3)
    save(out('UI', 'UI_Confirm.wav'), fade(loudness(lp(x, 6000), -25), 0.0005, 0.05), category='UI', spatial=False)

    # Error: dull double buzz.
    x = np.zeros(int(0.4 * SR))
    for at in (0.0, 0.16):
        place(x, lp(np.sign(sine(140, 0.11)), 900) * env_adsr(0.11, 0.005, 0.02, 0.7, 0.03), at)
    save(out('UI', 'UI_Error.wav'), fade(loudness(x, -26), 0.0005, 0.03), category='UI', spatial=False)

    # Open panel: paper sliding across a desk.
    d = 0.45
    tt = t(d)
    swell = np.sin(np.pi * np.clip(tt / d, 0, 1)) ** 2
    x = bp(white(d), 900, 6500) * swell * 0.5 + lp(white(d), 300) * swell * 0.2
    place(x, lp(white(0.1), 900) * env_exp(0.1, 0.02) * 0.4, d - 0.1)
    save(out('UI', 'UI_OpenPanel.wav'), fade(loudness(x, -27), 0.01, 0.05), category='UI', spatial=False)

    # Equip: fabric rustle + small buckle clink.
    d = 0.5
    rustle = bp(white(d), 1400, 7000) * lp(np.abs(H.RNG.standard_normal(int(d * SR))), 25) * 2.0 * env_adsr(d, 0.03, 0.1, 0.6, 0.15)
    x = rustle * 0.5
    place(x, modal([4200, 6100], [0.03, 0.02], [0.3, 0.15], 0.1), 0.32)
    save(out('UI', 'UI_Equip.wav'), fade(loudness(x, -26), 0.005, 0.05), category='UI', spatial=False)

    # Purchase: banknotes flicked + an old register bell.
    x = np.zeros(int(1.3 * SR))
    for i in range(4):
        flick = bp(white(0.06), 1800, 7000) * env_exp(0.06, 0.012)
        place(x, flick * (0.5 + 0.1 * i), 0.05 + i * 0.075)
    bell = modal([2093, 3420, 4980, 6640], [0.6, 0.4, 0.25, 0.15], [1, 0.5, 0.3, 0.15], 1.0)
    place(x, bell * 0.35, 0.38)
    x = reverb(x, SMALL, wet=0.25)
    save(out('UI', 'UI_Purchase.wav'), fade(loudness(x, -24), 0.002, 0.1), category='UI', spatial=False)

    # Ready: zipper pulled up + a clasp.
    d = 0.55
    rate = np.linspace(60, 260, int(d * SR))
    teeth = stick_slip(d, rate, jitter=0.1)
    zip_up = bp(teeth, 1200, 7000) * env_adsr(d, 0.02, 0.05, 0.9, 0.05)
    x = np.zeros(int(0.9 * SR))
    place(x, zip_up * 0.6, 0.0)
    place(x, modal([1800, 2900, 4400], [0.05, 0.03, 0.02], [1, 0.6, 0.3], 0.2) * 0.4, 0.58)
    save(out('UI', 'UI_Ready.wav'), fade(loudness(reverb(x, SMALL, wet=0.15), -24), 0.002, 0.05), category='UI', spatial=False)

    d = 0.35
    teeth = stick_slip(d, np.linspace(220, 90, int(d * SR)), jitter=0.1)
    x = bp(teeth, 1000, 6000) * env_adsr(d, 0.02, 0.05, 0.8, 0.08) * 0.6
    save(out('UI', 'UI_Unready.wav'), fade(loudness(x, -26), 0.002, 0.05), category='UI', spatial=False)

    # Countdown tick: heavier clock tick.
    x = reverb(clock_tick_sound(False), SMALL, wet=0.3)
    save(out('UI', 'UI_CountdownTick.wav'), fade(loudness(x, -21), 0.0005, 0.03), category='UI', spatial=False)

    # Notify: soft FM bell.
    d = 1.1
    tt = t(d)
    mod = np.sin(2 * np.pi * 1174.7 * 3.5 * tt) * 2.2 * np.exp(-tt / 0.15)
    x = np.sin(2 * np.pi * 1174.7 * tt + mod) * env_exp(d, 0.35)
    x = reverb(x, SMALL, wet=0.35)
    save(out('UI', 'UI_Notify.wav'), fade(loudness(x, -27), 0.001, 0.1), category='UI', spatial=False)

    # Level up: warm chord swell with a chime.
    d = 2.6
    x = np.zeros(int(d * SR))
    for f in (220.0, 277.18, 329.63, 415.3):
        tone2 = sum(np.sin(2 * np.pi * f * h * t(d) * detune_ratio(H.RNG.uniform(-6, 6))) / h for h in (1, 2, 3))
        x += tone2 * env_adsr(d, 0.6, 0.4, 0.7, 1.2) * 0.25
    for i, f in enumerate((880.0, 1108.7, 1318.5, 1760.0)):
        place(x, modal([f, f * 2.76], [0.6, 0.3], [0.3, 0.1], 1.2), 0.35 + i * 0.09)
    x = reverb(lp(x, 5000), make_ir(2.5, 1.6, stereo=False), wet=0.4)
    save(out('UI', 'UI_LevelUp.wav'), fade(loudness(x, -22), 0.01, 0.3), category='UI', spatial=False)


if __name__ == '__main__':
    os.chdir(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
    jobs = [rain_outside, rain_window, room_tone, fluorescent_hum, boiler_hum, tv_static, radio_static,
            clock_loop, wind_gusts, drips, pipe_creaks, wood_creaks, thunder, overhead_steps, door_slam_distant,
            knock, whispers, electrical_stutter, radio_bleed, radio_tune, doors, lockers, switches, flashlight,
            van_door, footsteps, ui_sounds]
    only = set(sys.argv[1:])
    for job in jobs:
        if only and job.__name__ not in only:
            continue
        print('->', job.__name__, flush=True)
        job()
    write_manifest('SourceArt/Audio/manifest.json')
    print('done', len(H.MANIFEST), 'files')
