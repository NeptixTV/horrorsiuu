"""
Second audio batch: town and house ambience, haunting events, lights/devices, gameplay SFX,
outdoor footsteps, UI and music cues. Everything is synthesized from scratch with hhsynth
(noise, filters, modal resonators, algorithmic impulse responses) exactly like gen_sfx.py and
gen_music.py: no recordings, no samples, no third-party material.

Run from the HorrorHeist project folder:
    python Tools/AssetGen/audio/gen_world_sfx.py                    # generate everything
    python Tools/AssetGen/audio/gen_world_sfx.py rain_light owls    # only some jobs
    python Tools/AssetGen/audio/gen_world_sfx.py --list             # list the job names
    python Tools/AssetGen/audio/gen_world_sfx.py --check [text]     # only analyse manifest files

Output format follows the existing assets: 16-bit PCM, 48 kHz for ambience/SFX/UI/footsteps
(mono when spatial, stereo allowed for non-spatial beds), 44.1 kHz stereo for music cues.

The new entries are MERGED into SourceArt/Audio/manifest.json: an entry whose 'file' already
exists is replaced in place, new entries are appended, every other entry is kept unchanged and
in its original order. Running the script twice never duplicates entries.

Loops are synthesized periodically instead of being crossfaded: noise is shaped in the FFT
domain over exactly one loop length, tones use a whole number of cycles per loop, events that
run past the end wrap around to the start, and filters / reverbs are applied circularly. The
last sample therefore flows straight into the first one. The analysis printed at the end lists
duration, peak, RMS, DC offset, clipping and the loop-junction discontinuity of every file.
"""
import json
import os
import sys
from contextlib import contextmanager

import numpy as np
from scipy import signal
from scipy.ndimage import minimum_filter1d, uniform_filter1d

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import hhsynth as H  # noqa: E402
from hhsynth import (bp, brown, detune_ratio, env_adsr, env_exp, fade, hp, lp, make_ir, modal,  # noqa: E402
                     note_hz, pan, peak, pink, place, resonator, reverb, rms_db, seed, sine, white)
import gen_sfx as S  # noqa: E402  shared room IRs + creak / latch / stick-slip helpers (main() is guarded)
import gen_music as M  # noqa: E402  instruments of the score (importing it switches H.SR to 44.1 kHz)

H.SR = 48000  # gen_music switched the library default on import; SFX/ambience are 48 kHz

OUT = 'SourceArt/Audio'
MANIFEST_PATH = 'SourceArt/Audio/manifest.json'

# Same rooms as gen_sfx (identical impulse responses, built from the same seed on import).
ROOM, SMALL, STAIR, HALL = S.ROOM, S.SMALL, S.STAIR, S.HALL


# =======================================================================================
# Generic helpers
# =======================================================================================

def N(d):
    """Sample count for a duration, consistent with hhsynth.t()/white()."""
    return int(d * H.SR)


def u(x):
    """Scale to unit RMS (for mixing layers by relative level)."""
    return x / (np.sqrt(np.mean(np.square(x))) + 1e-12)


def pk(x):
    """Scale to unit peak (for sparse layers made of events)."""
    return x / (np.max(np.abs(x)) + 1e-12)


def poisson(d, rate):
    """Random impulses (random sign, exponential size) at a time-varying rate in events/second."""
    n = N(d)
    r = np.full(n, float(rate)) if np.ndim(rate) == 0 else np.asarray(rate, dtype=float)[:n]
    hits = H.RNG.uniform(0, 1, n) < r / H.SR
    size = np.minimum(H.RNG.exponential(1.0, n), 3.0)  # capped: no freak outliers
    return hits * size * np.where(H.RNG.uniform(0, 1, n) < 0.5, -1.0, 1.0)


def bell_env(d, at, width):
    tt = H.t(d)
    return np.exp(-0.5 * ((tt - at) / width) ** 2)


def chirp_tone(freq, env, h2=0.05, h3=0.0):
    """Sine voice following a per-sample frequency curve (birds, owls, sirens)."""
    ph = 2 * np.pi * np.cumsum(freq) / H.SR
    y = np.sin(ph) + h2 * np.sin(2 * ph + 0.6) + h3 * np.sin(3 * ph + 1.1)
    return y * env


def outdoor_ir(duration=2.2, decay=1.5, echoes=6, spread=(0.04, 0.5), damping=4000, density=0.06,
               stereo=False, echo_gain=0.8):
    """Open-air space: a few smeared slap echoes off facades + a sparse, dark diffuse tail."""
    sr = H.SR
    n = int(duration * sr)
    tt = np.arange(n) / sr
    blen = int(0.008 * sr)
    win = np.hanning(blen)
    chans = []
    for _ in range(2 if stereo else 1):
        ir = np.zeros(n)
        for _ in range(echoes):
            dly = H.RNG.uniform(*spread)
            i = int(dly * sr)
            if i + blen >= n:
                continue
            amp = H.RNG.uniform(0.35, 0.9) * np.exp(-dly * 2.5) * echo_gain
            ir[i:i + blen] += amp * H.RNG.standard_normal(blen) * win
        sparse = (H.RNG.uniform(0, 1, n) < density) * H.RNG.standard_normal(n)
        tail = sparse * np.exp(-tt * 6.9 / decay) * np.clip((tt - 0.015) / 0.06, 0, 1)
        ir += tail * 0.5
        ir = hp(lp(ir, damping), 150)
        chans.append(ir / (np.sqrt(np.sum(ir ** 2)) + 1e-9))
    return np.stack(chans) if stereo else chans[0]


seed(4000)
HOUSE = make_ir(duration=2.0, decay=1.0, predelay=0.008, damping=5500, stereo=False)
TILE = make_ir(duration=1.4, decay=0.6, predelay=0.004, damping=9000, stereo=False, brightness=0.8)
OUTDOOR = outdoor_ir()
OUTDOOR_ST = outdoor_ir(stereo=True, echo_gain=0.6)
OUTDOOR_FAR = outdoor_ir(duration=4.5, decay=3.2, echoes=9, spread=(0.08, 1.1), damping=2600, density=0.1)
STREET = outdoor_ir(duration=1.2, decay=0.6, echoes=3, spread=(0.012, 0.09), damping=6000)
NEAR = outdoor_ir(duration=0.6, decay=0.25, echoes=2, spread=(0.006, 0.03), damping=7000, density=0.03)


# ---------------------------------------------------------------------------------------
# Loop-aware (periodic) synthesis

def qf(f, L):
    """Frequency rounded to a whole number of cycles per loop of length L seconds."""
    return max(1, round(f * L)) / L


def ctone(f, L, phase=None):
    ph = H.RNG.uniform(0, 2 * np.pi) if phase is None else phase
    return np.sin(2 * np.pi * qf(f, L) * H.t(L) + ph)


def cnoise(L, color='white'):
    """Noise that is exactly periodic over L (spectrally shaped in the FFT domain)."""
    n = N(L)
    X = np.fft.rfft(H.RNG.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / H.SR)
    f[0] = 1.0
    if color == 'pink':
        X /= np.sqrt(f)
    elif color == 'brown':
        X *= 1.0 / f * (f ** 2 / (f ** 2 + 20.0 ** 2))
    X[0] = 0
    y = np.fft.irfft(X, n)
    if color == 'white':
        return y / (np.std(y) + 1e-12)
    return y / (np.max(np.abs(y)) + 1e-9)


def ccurve(L, rate, tilt=1.0):
    """Smooth periodic random curve in [0, 1] (gusts, load, intensity)."""
    n = N(L)
    kmax = max(1, int(round(L * rate)))
    X = np.zeros(n // 2 + 1, dtype=complex)
    k = np.arange(1, kmax + 1)
    X[1:kmax + 1] = (H.RNG.standard_normal(kmax) + 1j * H.RNG.standard_normal(kmax)) / k ** (0.5 * tilt)
    y = np.fft.irfft(X, n)
    y -= y.min()
    return y / (y.max() + 1e-12)


def circ(fn, x, *args, **kw):
    """Apply a causal filter circularly: run it over two periods and keep the second one."""
    n = x.shape[-1]
    y = fn(np.concatenate([x, x], axis=-1), *args, **kw)
    return y[..., n:]


def ctv_lowpass(x, cut):
    n = x.shape[-1]
    return H.tv_lowpass(np.concatenate([x, x]), np.concatenate([cut, cut]))[n:]


def cfold(h, n):
    out = np.zeros(n)
    for s in range(0, len(h), n):
        seg = h[s:s + n]
        out[:len(seg)] += seg
    return out


def creverb(x, ir, wet=0.3, dry=1.0):
    """Circular convolution reverb: the tail of the loop end rings into its start."""
    n = x.shape[-1]
    if x.ndim == 1 and ir.ndim == 2:
        x = np.stack([x, x])
    if x.ndim == 2 and ir.ndim == 1:
        ir = np.stack([ir, ir])
    if x.ndim == 1:
        w = np.fft.irfft(np.fft.rfft(x) * np.fft.rfft(cfold(ir, n)), n)
    else:
        w = np.stack([np.fft.irfft(np.fft.rfft(x[c]) * np.fft.rfft(cfold(ir[c], n)), n) for c in range(2)])
    return dry * x + wet * w


def cplace(buffer, sound, start, gain=1.0):
    """Like hhsynth.place(), but whatever runs past the end wraps to the start of the loop."""
    n = buffer.shape[-1]
    m = sound.shape[-1]
    i = int(round(start * H.SR)) % n
    pos = 0
    while pos < m:
        k = min(m - pos, n - i)
        seg = sound[..., pos:pos + k]
        if buffer.ndim == 2 and sound.ndim == 1:
            buffer[:, i:i + k] += gain * seg
        else:
            buffer[..., i:i + k] += gain * seg
        pos += k
        i = 0
    return buffer


# ---------------------------------------------------------------------------------------
# Levels & output

def limiter(x, ceiling, look=0.004, hold=0.02, wrap=False):
    """Transparent look-ahead peak limiter (smooth gain curve instead of waveshaping)."""
    a = np.abs(x) if x.ndim == 1 else np.max(np.abs(x), axis=0)
    need = np.minimum(1.0, ceiling / np.maximum(a, 1e-12))
    if need.min() >= 1.0:
        return x
    mode = 'wrap' if wrap else 'nearest'
    ws = max(1, int(look * H.SR))
    hs = int(hold * H.SR)
    g = minimum_filter1d(need, size=2 * (ws + hs) + 1, mode=mode)
    g = uniform_filter1d(g, size=2 * ws + 1, mode=mode)
    return x * g


def finish(x, rms, peak_db=-1.0, loop=False):
    """DC block, gain to a target RMS (dBFS, whole file like the existing assets), limit peaks."""
    x = circ(H.dc_block, x) if loop else H.dc_block(x)
    ceiling = 10 ** (peak_db / 20)
    for _ in range(6):
        x = x * 10 ** ((rms - rms_db(x)) / 20)
        x = limiter(x, ceiling * 0.98, wrap=loop)
    return np.clip(x, -ceiling, ceiling)


def one_shot(x, rms, fade_in=0.002, fade_out=0.15, peak_db=-1.0):
    return fade(finish(x, rms, peak_db), fade_in, fade_out)


def emit(rel, x, category, loop=False, spatial=True, radius=0, caption=''):
    path = os.path.join(OUT, *rel.split('/'))
    x = np.asarray(x, dtype=np.float64)
    if spatial and x.ndim != 1:
        raise ValueError(f'{rel}: spatial sounds must be mono')
    H.save(path, x, sr=H.SR, category=category, loop=loop, spatial=spatial, radius=radius,
           caption=caption, volume=1.0)


@contextmanager
def music_rate():
    """Music cues are 44.1 kHz like gen_music.py."""
    old = H.SR
    H.SR = M.SR
    try:
        yield
    finally:
        H.SR = old


# =======================================================================================
# Hideout machinery (loops)
# =======================================================================================

def vent_hum():
    seed(4101)
    L = 8.0
    tt = H.t(L)
    turb = 0.7 + 0.3 * ccurve(L, 0.9)
    air = circ(bp, cnoise(L, 'pink'), 70, 7500) * turb
    for f, q, g in ((190, 2.0, 6), (430, 3.0, 4), (1150, 4.0, 3), (2300, 3.0, -3)):
        air = circ(peak, air, f, q, g)
    rumble = circ(lp, cnoise(L, 'brown'), 130) * (0.8 + 0.2 * ccurve(L, 0.4))
    line = qf(60.0, L)
    motor = sum(a * np.sin(2 * np.pi * line * h * tt + H.RNG.uniform(0, 6.28))
                for h, a in ((1, 0.45), (2, 1.0), (3, 0.2), (4, 0.3), (6, 0.12)))
    motor *= 0.9 + 0.1 * ccurve(L, 0.3)
    shaft = qf(14.625, L)  # fan wheel revolutions per second
    blade = qf(14.625 * 7, L)
    blades = (np.sin(2 * np.pi * blade * tt) + 0.35 * np.sin(2 * np.pi * 2 * blade * tt + 1.1)) * (0.75 + 0.25 * ccurve(L, 0.6))
    # A loose duct panel buzzes once per wheel revolution; it comes and goes.
    pulse = np.clip(np.sin(2 * np.pi * shaft * tt + 0.4), 0, None) ** 8
    rattle_amt = np.clip(ccurve(L, 0.5) * 1.8 - 0.6, 0, 1)
    rsrc = cnoise(L) * pulse * rattle_amt
    rattle = sum(a * circ(resonator, rsrc, f, q) for f, q, a in ((1240, 18, 1.0), (2030, 22, 0.7), (3310, 25, 0.4)))
    x = u(air) * 1.0 + u(rumble) * 0.6 + u(motor) * 0.4 + u(blades) * 0.22 + u(rattle) * 0.22
    x = creverb(x, ROOM, wet=0.18)
    emit('Ambience/A_Vent_Hum_Loop.wav', finish(x, -25, loop=True), 'Ambient', loop=True, radius=900)


def machine_hum():
    seed(4102)
    L = 8.0
    n = N(L)
    tt = H.t(L)
    # Two motor tones 0.75 Hz apart beat against each other: the slow throb.
    fa, fb = qf(58.5, L), qf(59.25, L)
    motor = np.zeros(n)
    for h, a in ((1, 1.0), (2, 0.6), (3, 0.35), (4, 0.2), (5, 0.12), (7, 0.06)):
        motor += a * (np.sin(2 * np.pi * fa * h * tt + H.RNG.uniform(0, 6.28))
                      + 0.8 * np.sin(2 * np.pi * fb * h * tt + H.RNG.uniform(0, 6.28)))
    # Reciprocating compressor: one compression stroke per revolution -> buzzy pulse train.
    fp = qf(24.75, L)
    ph = (fp * tt) % 1.0
    stroke = np.exp(-ph / 0.07)
    stroke -= stroke.mean()
    throb = circ(resonator, stroke, 140, 4) + 0.6 * circ(resonator, stroke, 265, 6) + 0.3 * circ(resonator, stroke, 520, 8)
    beat = 0.5 + 0.5 * np.cos(2 * np.pi * (fb - fa) * tt)
    roar = circ(lp, cnoise(L, 'pink'), 450) * (0.75 + 0.25 * beat)
    hiss = circ(bp, cnoise(L), 2500, 7000) * (0.7 + 0.3 * ccurve(L, 0.5))
    # A loose bracket ticking roughly twice a second.
    ticks = np.zeros(n)
    k = 15
    for i in range(k):
        if H.RNG.random() < 0.15:
            continue
        at = (i + H.RNG.uniform(-0.12, 0.12)) * L / k
        f0 = H.RNG.uniform(2300, 2900)
        tick = modal([f0, f0 * 1.62, f0 * 2.71], [0.006, 0.004, 0.003], [1, 0.6, 0.3], 0.03)
        tick += hp(white(0.03), 3000) * env_exp(0.03, 0.0015) * 0.4
        tick += modal([380, 720], [0.012, 0.008], [0.5, 0.3], 0.03)
        cplace(ticks, tick * H.RNG.uniform(0.5, 1.0), at)
    load = 0.85 + 0.15 * ccurve(L, 0.25)
    x = (u(motor) * 0.55 + u(throb) * 0.5 + u(roar) * 1.0 + u(hiss) * 0.05) * load + pk(ticks) * 0.6
    x = circ(peak, x, 95, 1.5, 4)
    x = creverb(x, ROOM, wet=0.22)
    emit('Ambience/A_Machine_Hum_Loop.wav', finish(x, -25, loop=True), 'Ambient', loop=True, radius=800)


def electric_buzz():
    seed(4103)
    L = 6.0
    n = N(L)
    tt = H.t(L)
    f = qf(120.0, L)
    hum = np.zeros(n)
    for h in range(1, 60):
        if f * h > 9000:
            break
        a = 1.0 / h ** 1.1 * (1.3 if h % 2 == 0 else 1.0)
        hum += a * np.sin(2 * np.pi * f * h * tt + H.RNG.uniform(0, 6.28))
    hum = circ(lp, hum, 3500)
    # Loose laminations slap at every flux peak: narrow pulses -> metallic "zzz".
    s = np.sin(2 * np.pi * f * tt + 0.3)
    zz = np.clip(s, 0.6, 1.0) - 0.6
    zz = circ(bp, zz - zz.mean(), 400, 6500)
    depth = 0.55 + 0.45 * ccurve(L, 0.8)
    # Arcing: crackle episodes locked to the mains peaks, plus a few isolated pops.
    crack = np.zeros(n)
    for _ in range(5):
        start = H.RNG.uniform(0, L)
        length = H.RNG.uniform(0.04, 0.35)
        for c in range(int(length * 120)):
            if H.RNG.random() < 0.6:
                burst = white(0.0015) * np.exp(-H.t(0.0015) / 0.0003) * H.RNG.uniform(0.3, 1.0)
                cplace(crack, burst, start + c / 120 + H.RNG.uniform(-0.0006, 0.0006))
    for _ in range(6):
        cplace(crack, white(0.004) * np.exp(-H.t(0.004) / 0.0006) * H.RNG.uniform(0.6, 1.4), H.RNG.uniform(0, L))
    crack = circ(hp, crack, 1800)
    x = u(hum) * 1.0 + u(zz) * 0.45 * depth + pk(crack) * 0.9
    x *= 0.9 + 0.1 * ccurve(L, 0.5)
    x = circ(peak, x, 850, 2.5, 4)
    x = creverb(x, SMALL, wet=0.15)
    emit('Ambience/A_Electric_Buzz_Loop.wav', finish(x, -26, loop=True), 'Ambient', loop=True, radius=500)


# =======================================================================================
# Town exterior
# =======================================================================================

def town_wind():
    seed(4201)
    L = 20.0
    n = N(L)
    gust = 0.3 + 0.7 * ccurve(L, 0.16) ** 1.4
    centers = np.array([300, 380, 470, 590, 740, 930])
    chans = []
    for c in range(2):
        g = np.roll(gust, int((0.25 + 0.3 * c) * H.SR)) * (0.85 + 0.15 * ccurve(L, 0.7))
        cut = 220 + 1100 * g
        body = ctv_lowpass(ctv_lowpass(cnoise(L, 'pink'), cut), cut * 1.3)
        rumble = circ(lp, cnoise(L, 'brown'), 110) * (0.35 + 0.65 * g)
        # Whistle through branches and wires: a resonator bank cross-faded by gust strength.
        src = cnoise(L)
        target = 300 + 550 * g
        whistle = np.zeros(n)
        for fc in centers:
            w = np.exp(-0.5 * ((target - fc) / 90.0) ** 2)
            whistle += circ(resonator, src, fc * (1 + 0.02 * c), 18) * w
        whistle *= g ** 2
        # Leaves: crackly rustle whose density follows the gust with a little lag.
        lag = np.roll(g, int(0.4 * H.SR))
        crackle = circ(bp, poisson(L, 300 + 5000 * lag ** 2), 2200, 9500)
        grain = circ(lp, np.abs(cnoise(L)), 25)
        rustle = circ(bp, cnoise(L), 1500, 7000) * grain
        leaves = (u(crackle) * 0.6 + u(rustle) * 0.8) * lag ** 1.5
        chans.append(u(body) * g + u(rumble) * 0.6 + u(whistle) * 0.12 + u(leaves) * 0.35)
    x = np.stack(chans)
    x = creverb(x, OUTDOOR_ST, wet=0.12)
    emit('Ambience/A_Town_Wind_Loop.wav', finish(x, -24, loop=True), 'Ambient', loop=True, spatial=False, radius=0)


def car_pass(dur, d0=30.0, speed=15.0, engine_hz=33.0, wet=1.0, t_close=None, power=2.0):
    """A car passing at closest distance d0 (m): doppler via a moving delay line, 1/r^power level,
    distance-dependent air absorption. Tyre roar + wet spray + engine firing."""
    tt = H.t(dur)
    tc = dur * 0.5 if t_close is None else t_close
    pos = speed * (tt - tc)
    dist = np.sqrt(pos ** 2 + d0 ** 2)
    delay = dist / 343.0
    pre = float(delay.max()) + 0.05
    total = dur + pre
    sn = N(total)
    tyre = bp(pink(total), 220, 5200) * (0.75 + 0.25 * pk(lp(np.abs(white(total)), 60)))
    spray = hp(white(total), 2200) * (0.5 + 0.5 * pk(lp(np.abs(white(total)), 35))) * wet
    fe = engine_hz * (1 + 0.015 * S.smooth_noise(total, 1.5))
    ph = np.cumsum(fe) / H.SR
    eng = sum(np.sin(2 * np.pi * k * ph + H.RNG.uniform(0, 6)) / k ** 1.1 for k in range(1, 12))
    eng += u(lp(white(total), 600)) * (0.5 + 0.5 * np.sin(2 * np.pi * ph)) * 0.6
    eng = lp(eng, 700)
    src = u(tyre) * 1.0 + u(spray) * 0.6 + u(eng) * 0.5
    y = np.interp((tt + pre - delay) * H.SR, np.arange(sn), src)
    near = d0 / dist
    c_near = float(np.clip(5500 * 30.0 / d0, 1500, 5500))
    cut = c_near * (0.15 + 0.85 * near ** 3)
    y = H.tv_lowpass(H.tv_lowpass(y, cut), cut * 1.3)
    return y * near ** power


def traffic_distant():
    seed(4202)
    L = 20.0
    n = N(L)
    dens = 0.65 + 0.35 * ccurve(L, 0.12)
    roar = circ(lp, cnoise(L, 'brown'), 160)
    body = circ(bp, cnoise(L, 'pink'), 90, 700)
    hiss = circ(bp, cnoise(L, 'pink'), 500, 2200) * (0.6 + 0.4 * ccurve(L, 0.25))
    bed = (u(roar) * 1.0 + u(body) * 0.55 + u(hiss) * 0.18) * dens
    passes = np.zeros(n)
    for at, dur, gain in ((H.RNG.uniform(1.5, 4.0), 8.0, 1.0), (H.RNG.uniform(10.5, 13.0), 7.0, 0.7)):
        ev = car_pass(dur, d0=H.RNG.uniform(90, 140), speed=H.RNG.uniform(18, 26),
                      engine_hz=H.RNG.uniform(28, 40), wet=0.8, power=1.5)
        ev = fade(ev, 1.0, 1.5)
        w = N(0.5)
        loudest = np.max(np.sqrt(np.convolve(ev ** 2, np.ones(w) / w, mode='same')))
        cplace(passes, ev / (loudest + 1e-12) * gain, at)
    x = u(bed) + passes * 0.9
    x = circ(lp, x, 2200)
    x = creverb(x, OUTDOOR, wet=0.4)
    emit('Ambience/A_Town_Traffic_Distant_Loop.wav', finish(x, -27, loop=True), 'Ambient', loop=True, radius=6000)


def whistle(dur, f0, f1, curve=1.0, vib=(0.0, 0.0), h2=0.05, attack=0.25):
    tt = H.t(dur)
    uu = tt / dur
    f = f0 + (f1 - f0) * uu ** curve
    if vib[1]:
        f = f * (1 + vib[1] * np.sin(2 * np.pi * vib[0] * tt))
    a = np.clip(uu / attack, 0, 1)
    r = np.clip((1 - uu) / (1 - attack), 0, 1)
    return chirp_tone(f, np.sin(0.5 * np.pi * a) ** 2 * r ** 1.3, h2=h2)


def bird_feebee(scale=1.0, both=True):
    """Two pure, falling whistles a minor third apart: the melancholic 'fee-bee'."""
    out = np.zeros(N(0.8))
    place(out, whistle(0.3, 3950 * scale, 3870 * scale, attack=0.2), 0.0)
    if both:
        place(out, whistle(0.36, 3330 * scale, 3240 * scale, vib=(27, 0.006), attack=0.15) * 0.85, 0.37)
    return out


def bird_phrase(n_syll, base):
    out = np.zeros(N(2.5))
    at = 0.0
    for _ in range(n_syll):
        kind = H.RNG.integers(0, 4)
        dur = H.RNG.uniform(0.06, 0.15)
        b = base * H.RNG.uniform(0.85, 1.2)
        if kind == 0:
            s = whistle(dur, b * 0.75, b * 1.25, curve=0.7)
        elif kind == 1:
            s = whistle(dur, b * 1.3, b * 0.8, curve=0.6)
        elif kind == 2:
            tt = H.t(dur)
            uu = tt / dur
            env = np.sin(np.pi * uu) ** 0.8
            s = chirp_tone(b * (0.85 + 0.45 * np.sin(np.pi * uu)), env, h2=0.08)
        else:
            s = whistle(dur, b, b * 0.95, vib=(H.RNG.uniform(35, 60), 0.07))
        place(out, s * H.RNG.uniform(0.6, 1.0), at)
        at += dur + H.RNG.uniform(0.02, 0.08)
    return out[:N(at + 0.05)]


def bird_trill(dur=1.2, base=5600):
    out = np.zeros(N(dur + 0.1))
    rate = H.RNG.uniform(14, 18)
    count = int(dur * rate)
    for i in range(count):
        g = np.sin(np.pi * (i + 0.5) / count) ** 0.6
        el = whistle(0.035, base * 1.1, base * 0.7, curve=0.6, attack=0.15)
        place(out, el * g, i / rate)
    return out


def caw(dur, f0):
    """Crow: rough, nasal, period-doubled harmonic voice through broad formants."""
    tt = H.t(dur)
    uu = tt / dur
    f = f0 * (0.92 + 0.16 * np.sin(np.pi * np.clip(uu * 1.3, 0, 1)) - 0.1 * uu)
    jit = lp(H.RNG.standard_normal(len(tt)), 90)
    f *= 1 + 0.03 * jit / (np.std(jit) + 1e-9)
    ph = np.cumsum(f) / H.SR
    src = sum(np.sin(2 * np.pi * k * ph) / k ** 0.7 for k in range(1, int(7000 / f0)))
    src *= 1 + 0.45 * np.sin(np.pi * ph)
    exc = u(src) + u(bp(white(dur), 800, 4500)) * 0.55
    y = resonator(exc, 1300, 3.5) + 0.8 * resonator(exc, 1900, 4) + 0.35 * resonator(exc, 3100, 5)
    env = np.clip(tt / 0.025, 0, 1) * np.clip((dur - tt) / 0.09, 0, 1) * (1 - 0.3 * uu)
    return y * env


def birds_day():
    seed(4203)
    L = 20.0
    n = N(L)
    x = np.zeros(n)

    def put(sound, at, dist):
        s = lp(sound, 9000 - 5500 * dist)
        cplace(x, pk(s) * (1.0 - 0.7 * dist), at)

    for at in (1.0, 8.6, 15.2):
        put(bird_feebee(1.0, H.RNG.random() < 0.8), at + H.RNG.uniform(-0.2, 0.2), 0.35)
    for at in (3.4, 6.1, 11.8, 17.4):
        put(bird_phrase(int(H.RNG.integers(3, 7)), 2900), at, 0.2)
    for at in (5.0, 13.6):
        put(bird_phrase(int(H.RNG.integers(3, 6)), 3600), at, 0.7)
    for at in (9.9, 19.2):  # the second one wraps into the loop start
        put(bird_trill(H.RNG.uniform(1.0, 1.4)), at, 0.5)
    crows = np.zeros(N(2.2))
    at = 0.0
    for _ in range(3):
        d = H.RNG.uniform(0.28, 0.38)
        place(crows, caw(d, H.RNG.uniform(560, 640)) * H.RNG.uniform(0.8, 1.0), at)
        at += d + H.RNG.uniform(0.22, 0.32)
    put(crows, 12.4, 0.85)
    air = circ(bp, cnoise(L, 'pink'), 150, 3000) * (0.7 + 0.3 * ccurve(L, 0.2))
    x = x + u(air) * 0.006
    x = creverb(x, OUTDOOR, wet=0.3)
    emit('Ambience/A_Town_Birds_Day_Loop.wav', finish(x, -30, loop=True), 'Ambient', loop=True, radius=3000)


def cricket_chirp(fc, pulses, rate, plen):
    y = np.zeros(N(pulses / rate + plen + 0.01))
    for k in range(pulses):
        tt = H.t(plen)
        uu = tt / plen
        env = np.sin(np.pi * uu) ** 1.5 * (1 - 0.3 * uu)
        tone = chirp_tone(fc * (1.0 - 0.025 * uu), np.ones_like(tt), h2=0.12)
        tone += bp(white(plen), fc * 0.85, fc * 1.15) * 0.15
        place(y, tone * env * H.RNG.uniform(0.75, 1.0), k / rate)
    return y


def crickets_night():
    seed(4204)
    L = 12.0
    n = N(L)
    tt = H.t(L)
    x = np.zeros(n)
    for _ in range(8):
        fc = H.RNG.uniform(4200, 5200)
        dist = H.RNG.uniform(0, 1)
        count = int(H.RNG.integers(18, 34))
        period = L / count
        pulses = int(H.RNG.integers(3, 6))
        rate = H.RNG.uniform(26, 34)
        plen = H.RNG.uniform(0.012, 0.02)
        offset = H.RNG.uniform(0, period)
        amp = 1.0 - 0.75 * dist
        for i in range(count):
            if H.RNG.random() < 0.08:
                continue
            ch = cricket_chirp(fc * H.RNG.uniform(0.997, 1.003), pulses, rate, plen)
            ch = lp(ch, 9000 - 4000 * dist)
            cplace(x, ch * amp * H.RNG.uniform(0.8, 1.0), offset + (i + H.RNG.uniform(-0.03, 0.03)) * period)
    # Tree crickets: continuous pulsed trills swelling in and out.
    for _ in range(2):
        fc = qf(H.RNG.uniform(2700, 3100), L)
        pr = qf(H.RNG.uniform(42, 55), L)
        am = np.clip(np.sin(2 * np.pi * pr * tt + H.RNG.uniform(0, 6)), 0, None) ** 2
        tone = np.sin(2 * np.pi * fc * tt + H.RNG.uniform(0, 6)) * am * (0.3 + 0.7 * ccurve(L, 0.2))
        x += tone * 0.1
    air = circ(bp, cnoise(L, 'pink'), 120, 2500)
    x = x + u(air) * 0.012
    x = creverb(x, OUTDOOR, wet=0.3)
    emit('Ambience/A_Town_Crickets_Night_Loop.wav', finish(x, -28, loop=True), 'Ambient', loop=True, radius=2500)


def rain_light():
    seed(4205)
    L = 16.0
    n = N(L)
    intensity = 0.8 + 0.2 * ccurve(L, 0.1)
    chans = []
    for _ in range(2):
        hiss = circ(bp, cnoise(L, 'pink'), 1500, 11000)
        wash = circ(bp, cnoise(L, 'pink'), 200, 1500)
        drizzle = circ(bp, poisson(L, 2200), 2000, 9000)  # countless tiny droplets on the street
        ticks = circ(bp, poisson(L, 350), 1200, 7000)     # slightly bigger ones
        drops = np.zeros(n)
        for _ in range(int(L * 18)):
            fr = np.array([1900, 3300, 4700]) * H.RNG.uniform(0.75, 1.4)
            hit = modal(fr, [0.006, 0.004, 0.003], [1, 0.5, 0.25], 0.03) + hp(white(0.03), 3000) * env_exp(0.03, 0.001) * 0.5
            cplace(drops, hit * H.RNG.uniform(0.1, 1.0) ** 2, H.RNG.uniform(0, L))
        plinks = np.zeros(n)
        for _ in range(int(L * 4)):
            f0 = H.RNG.uniform(900, 2400)
            dd = H.RNG.uniform(0.02, 0.05)
            tb = H.t(dd * 3)
            f = f0 * (1 + 0.8 * (1 - np.exp(-tb / 0.008)))
            s = np.sin(2 * np.pi * np.cumsum(f) / H.SR) * np.exp(-tb / dd) * np.clip(tb / 0.0008, 0, 1)
            cplace(plinks, s * H.RNG.uniform(0.2, 1.0), H.RNG.uniform(0, L))
        chans.append((u(hiss) * 0.45 + u(wash) * 0.3 + u(drizzle) * 0.8 + u(ticks) * 0.35 + pk(drops) * 0.6 + pk(plinks) * 0.3) * intensity)
    x = np.stack(chans)
    # Gutter drips: a downpipe dripping into a puddle (left), a tin window sill (right).
    drips = np.zeros((2, n))
    at = H.RNG.uniform(0, 0.5)
    while at < L:
        f0 = H.RNG.uniform(520, 760)
        dd = H.RNG.uniform(0.04, 0.07)
        tb = H.t(0.25)
        f = f0 * (1 + 0.9 * (1 - np.exp(-tb / 0.012)))
        plop = np.sin(2 * np.pi * np.cumsum(f) / H.SR) * np.exp(-tb / dd) * np.clip(tb / 0.001, 0, 1)
        plop += bp(white(0.25), 800, 5000) * env_exp(0.25, 0.006) * 0.35
        cplace(drips, pan(plop * H.RNG.uniform(0.6, 1.0), -0.6), at)
        at += H.RNG.uniform(0.55, 0.95) if H.RNG.random() > 0.12 else 0.12
    at = H.RNG.uniform(0, 1.0)
    while at < L:
        fr = np.array([1450, 2780, 4100, 5900]) * H.RNG.uniform(0.97, 1.03)
        tink = modal(fr, [0.08, 0.05, 0.03, 0.02], [1, 0.6, 0.4, 0.2], 0.4) + hp(white(0.4), 2500) * env_exp(0.4, 0.002) * 0.4
        cplace(drips, pan(tink * H.RNG.uniform(0.5, 1.0), 0.55), at)
        at += H.RNG.uniform(1.1, 2.0)
    x = x + pk(drips) * 1.1
    x = creverb(x, OUTDOOR_ST, wet=0.18)
    emit('Ambience/A_Rain_Light_Loop.wav', finish(x, -24, loop=True), 'Ambient', loop=True, spatial=False, radius=0)


def bark(f0, size, dur):
    """One bark: rough harmonic voice + breath noise through jaw-opening formants."""
    tt = H.t(dur)
    uu = tt / dur
    f = f0 * (0.8 + 0.55 * np.exp(-((uu - 0.22) / 0.2) ** 2) - 0.15 * uu)
    jit = lp(H.RNG.standard_normal(len(tt)), 120)
    f *= 1 + 0.04 * jit / (np.std(jit) + 1e-9)
    ph = np.cumsum(f) / H.SR
    src = np.zeros_like(tt)
    for k in range(1, 40):
        if f0 * k * 1.4 > 7000:
            break
        src += np.sin(2 * np.pi * k * ph + H.RNG.uniform(0, 6)) / k ** 0.9
    src *= 1 + 0.35 * np.sin(np.pi * ph)
    exc = u(src) + u(white(dur)) * 0.45
    F = np.array([650, 1450, 2550]) / size
    y = resonator(exc, F[0], 3) + 0.7 * resonator(exc, F[1], 4) + 0.35 * resonator(exc, F[2], 5)
    env = np.clip(tt / 0.008, 0, 1) * np.exp(-np.clip(tt - 0.03, 0, None) / (dur * 0.35)) * np.clip((dur - tt) / 0.03, 0, 1)
    return y * env


def dog_barks():
    specs = [(420, 1.0, 3, (0.3, 0.45)), (250, 1.45, 2, (0.45, 0.6)), (620, 0.75, 4, (0.2, 0.32))]
    for k, (f0, size, count, gaps) in enumerate(specs):
        seed(4220 + k)
        d = 2.3
        x = np.zeros(N(d))
        at = 0.05
        for _ in range(count):
            dur = H.RNG.uniform(0.13, 0.2) * size ** 0.5
            place(x, bark(f0 * H.RNG.uniform(0.94, 1.06), size, dur) * H.RNG.uniform(0.75, 1.0), at)
            at += dur + H.RNG.uniform(*gaps)
        x = lp(hp(x, 180), 2400)
        x = reverb(x, OUTDOOR, wet=0.55)
        emit(f'Ambience/A_Dog_Bark_Distant_{k + 1:02d}.wav', one_shot(x, -24, 0.002, 0.4), 'Ambient',
             radius=6000, caption='[A dog barks in the distance]')


def car_pass_distant():
    for k in range(3):
        seed(4210 + k)
        dur = 6.0
        y = car_pass(dur, d0=H.RNG.uniform(28, 45), speed=H.RNG.uniform(11, 17), engine_hz=H.RNG.uniform(26, 42),
                     wet=H.RNG.uniform(0.7, 1.1), t_close=H.RNG.uniform(2.6, 3.4), power=3.0)
        y = hp(y, 60)
        y = reverb(y, OUTDOOR, wet=0.35)
        emit(f'Ambience/A_Car_Pass_Distant_{k + 1:02d}.wav', one_shot(y, -25, 0.4, 0.8), 'Ambient', radius=5000)


def siren_distant():
    seed(4230)
    d = 8.6
    tt = H.t(d)
    sweep = 0.5 - 0.5 * np.cos(2 * np.pi * tt / 4.1 + 0.6)
    f = (680 + 760 * sweep ** 1.2) * (1 + 0.004 * (S.smooth_noise(d, 2.0) - 0.5))
    ph = np.cumsum(f) / H.SR
    tone = sum(a * np.sin(2 * np.pi * k * ph) for k, a in ((1, 1.0), (2, 0.25), (3, 0.42), (4, 0.12), (5, 0.18), (7, 0.07)))
    turb = 0.65 + 0.35 * S.smooth_noise(d, 1.2)
    direct = lp(tone, 2600) * turb
    i = int(0.31 * H.SR)
    echo = np.zeros_like(direct)
    echo[i:] = lp(direct, 1500)[:-i] * 0.55
    x = (direct + echo) * np.clip(tt / 1.6, 0, 1) ** 2 * np.clip((d - tt) / 2.8, 0, 1) ** 1.5
    x = hp(x, 300)
    x = reverb(x, OUTDOOR_FAR, wet=0.6)
    emit('Ambience/A_Siren_Distant_01.wav', one_shot(x, -24, 0.05, 0.6), 'Ambient', radius=8000,
         caption='[A siren wails far away]')


def chime_strike(f, vel, dur):
    """Free-free tube modes, each split into a slightly detuned doublet (slow shimmer)."""
    tt = H.t(dur)
    y = np.zeros_like(tt)
    for r, dcy, a in ((1.0, 2.6, 1.0), (2.756, 1.2, 0.55), (5.404, 0.5, 0.3), (8.933, 0.22, 0.15)):
        fr = f * r
        if fr > H.SR * 0.45:
            continue
        dcy *= (700 / f) ** 0.4
        split = 1 + H.RNG.uniform(0.0006, 0.0018)
        for fq, w in ((fr, 1.0), (fr * split, 0.7)):
            y += a * w * np.exp(-tt / dcy) * np.sin(2 * np.pi * fq * tt + H.RNG.uniform(0, 6.28))
    click = hp(white(0.004), 2500) * env_exp(0.004, 0.0006)
    y[:len(click)] += click * 0.4
    return y * vel * np.clip(tt / 0.0005, 0, 1)


def wind_chimes():
    sets = [['A5', 'C6', 'D#6', 'E6', 'G#6'], ['F#5', 'G5', 'C#6', 'D6', 'A6']]
    for k, names in enumerate(sets):
        seed(4240 + k)
        d = 4.4
        tubes = [note_hz(nm) * detune_ratio(H.RNG.uniform(-18, 18)) for nm in names]
        x = np.zeros(N(d))
        times = sorted(np.clip(H.RNG.normal(0.9, 0.55, int(H.RNG.integers(7, 11))), 0.02, 2.3))
        for at in times:
            j = int(H.RNG.integers(0, len(tubes)))
            vel = H.RNG.uniform(0.25, 1.0) * (0.35 + 0.65 * np.exp(-((at - 0.8) / 1.0) ** 2))
            place(x, chime_strike(tubes[j], vel, d - at), at)
            if H.RNG.random() < 0.25:  # clapper bounces into a neighbour
                place(x, chime_strike(tubes[(j + 1) % len(tubes)], vel * 0.4, d - at - 0.02), at + 0.02)
        x = lp(x, 9000)
        x = reverb(x, OUTDOOR, wet=0.25)
        emit(f'Ambience/A_Wind_Chime_{k + 1:02d}.wav', one_shot(x, -24, 0.001, 0.8), 'Ambient', radius=1200)


def church_bell():
    seed(4250)
    d = 9.5
    prime = note_hz('D3')
    partials = [(0.5, 9.0, 0.55), (1.0, 6.0, 0.5), (1.19, 4.5, 0.6), (1.5, 2.8, 0.22), (2.0, 4.0, 1.0),
                (2.51, 2.2, 0.45), (2.66, 1.8, 0.3), (3.01, 1.5, 0.35), (4.17, 1.0, 0.28), (5.43, 0.7, 0.2),
                (6.8, 0.45, 0.12), (8.3, 0.3, 0.08)]
    tt = H.t(d)
    y = np.zeros_like(tt)
    for r, dcy, a in partials:
        f = prime * r
        beat = H.RNG.uniform(0.15, 0.7)
        for fq, w in ((f, 1.0), (f + beat, 0.6)):
            y += a * w * np.exp(-tt / dcy) * np.sin(2 * np.pi * fq * tt + H.RNG.uniform(0, 6.28))
    y *= np.clip(tt / 0.002, 0, 1)
    strike = lp(white(0.08), 3000) * env_exp(0.08, 0.008) * 0.8
    strike += modal(H.RNG.uniform(1200, 5000, 8), H.RNG.uniform(0.02, 0.08, 8), H.RNG.uniform(0.1, 0.3, 8), 0.08)
    y[:len(strike)] += strike
    y = hp(lp(y, 2800), 55)
    x = np.zeros(N(d))
    place(x, y, 0.08)
    x = reverb(x, OUTDOOR_FAR, wet=0.6)
    emit('Ambience/A_Church_Bell_Distant_01.wav', one_shot(x, -22, 0.01, 1.5), 'Ambient', radius=12000,
         caption='[A church bell tolls]')


def hoot(dur, f0, rise=0.05, fall=0.08, trem=0.0, trem_rate=18.0):
    tt = H.t(dur)
    uu = tt / dur
    f = f0 * (1 + rise * np.sin(np.pi * np.clip(uu * 2.2, 0, 1)) - fall * uu)
    env = np.clip(tt / min(0.05, dur * 0.3), 0, 1) ** 2 * np.clip((dur - tt) / min(0.12, dur * 0.5), 0, 1) ** 1.5
    if trem:
        env *= 1 - trem * (0.5 + 0.5 * np.sin(2 * np.pi * trem_rate * tt))
    y = chirp_tone(f, env, h2=0.1, h3=0.03)
    return y + bp(white(dur), f0 * 0.7, f0 * 3.5) * 0.06 * env


def owls():
    seed(4260)  # great horned owl rhythm: hoo, h'hoo-hoo, hoo, hoo
    d = 2.6
    x = np.zeros(N(d))
    f0 = 330
    for at, dur, g, fr in ((0.05, 0.36, 1.0, 1.0), (0.6, 0.11, 0.6, 1.04), (0.74, 0.24, 0.85, 1.02),
                           (1.18, 0.4, 0.95, 1.0), (1.78, 0.42, 0.8, 0.97)):
        place(x, hoot(dur, f0 * fr) * g, at)
    x = reverb(lp(x, 2500), OUTDOOR, wet=0.45)
    emit('Ambience/A_Owl_01.wav', one_shot(x, -24, 0.005, 0.4), 'Ambient', radius=4000)

    seed(4261)  # tawny-like: hooo ... hu, hu-hoooooo (quavering)
    x = np.zeros(N(d))
    f0 = 690
    place(x, hoot(0.55, f0, rise=0.08, fall=0.12), 0.05)
    place(x, hoot(0.08, f0 * 1.05) * 0.5, 0.95)
    place(x, hoot(0.07, f0 * 1.08) * 0.55, 1.1)
    place(x, hoot(0.85, f0 * 1.03, rise=0.04, fall=0.06, trem=0.6, trem_rate=21) * 0.9, 1.23)
    x = reverb(lp(x, 3000), OUTDOOR, wet=0.45)
    emit('Ambience/A_Owl_02.wav', one_shot(x, -24, 0.005, 0.4), 'Ambient', radius=4000)


# =======================================================================================
# House interior
# =======================================================================================

def house_room_tone():
    seed(4301)
    L = 12.0
    tt = H.t(L)
    ff = qf(57.5, L)
    fridge = sum(a * np.sin(2 * np.pi * ff * h * tt + H.RNG.uniform(0, 6)) for h, a in ((1, 0.4), (2, 1.0), (3, 0.3), (4, 0.25)))
    fridge = circ(lp, fridge, 260) * (0.85 + 0.15 * ccurve(L, 0.3))
    chans = []
    for _ in range(2):
        air = circ(bp, cnoise(L, 'pink'), 80, 3500)
        rumble = circ(lp, cnoise(L, 'brown'), 95)
        presence = circ(bp, cnoise(L), 2500, 9000)
        chans.append(u(rumble) * 1.0 + u(air) * 0.25 + u(fridge) * 0.08 + u(presence) * 0.012)
    x = creverb(np.stack(chans), HALL, wet=0.15)
    emit('Ambience/A_House_RoomTone_Loop.wav', finish(x, -32, loop=True), 'Ambient', loop=True, spatial=False, radius=0)


def fridge_hum():
    seed(4302)
    L = 8.0
    n = N(L)
    tt = H.t(L)
    fc = qf(58.25, L)
    comp = np.zeros(n)
    for h, a in ((1, 0.5), (2, 1.0), (3, 0.35), (4, 0.4), (5, 0.12), (6, 0.18), (8, 0.06), (10, 0.04)):
        comp += a * np.sin(2 * np.pi * fc * h * tt + H.RNG.uniform(0, 6))
    mains = np.sin(2 * np.pi * qf(120, L) * tt) * 0.3 + np.sin(2 * np.pi * qf(240, L) * tt) * 0.1
    load = 0.85 + 0.15 * ccurve(L, 0.35)
    fan = circ(bp, cnoise(L, 'pink'), 150, 3500)
    blade = np.sin(2 * np.pi * qf(152, L) * tt) * (0.7 + 0.3 * ccurve(L, 0.8))
    whine = np.sin(2 * np.pi * qf(1630, L) * tt + 0.2 * np.sin(2 * np.pi * qf(0.5, L) * tt))
    gurgle = np.zeros(n)
    for _ in range(9):  # refrigerant bubbling through the lines now and then
        f0 = H.RNG.uniform(260, 620)
        dd = H.RNG.uniform(0.02, 0.05)
        tb = H.t(dd * 4)
        fb = f0 * (1 + 0.5 * (1 - np.exp(-tb / 0.015)))
        b = np.sin(2 * np.pi * np.cumsum(fb) / H.SR) * np.exp(-tb / dd) * np.clip(tb / 0.002, 0, 1)
        cplace(gurgle, b * H.RNG.uniform(0.3, 1.0), 4.6 + H.RNG.uniform(0, 1.6))
    gurgle = circ(lp, gurgle, 1200)
    x = (u(comp) * 1.0 + u(mains) * 0.3) * load + u(fan) * 0.45 + u(blade) * 0.12 + u(whine) * 0.015 + pk(gurgle) * 0.35
    x = circ(peak, x, 210, 2, 4)
    x = circ(peak, x, 480, 3, 3)
    x = creverb(x, TILE, wet=0.18)
    emit('Ambience/A_Fridge_Hum_Loop.wav', finish(x, -26, loop=True), 'Ambient', loop=True, radius=600)


def timber_tick(scale=1.0, d=0.15):
    y = modal(np.array([380, 870, 1520, 2450, 3700]) * scale, [0.03, 0.018, 0.012, 0.008, 0.005], [0.55, 1.0, 0.7, 0.4, 0.2], d)
    return y + hp(white(d), 1800) * env_exp(d, 0.0012) * 0.7


def house_settle():
    for k in range(4):
        seed(4310 + k)
        d = 1.7
        x = np.zeros(N(d))
        if k == 0:  # a groan, then a tick
            place(x, pk(S.wood_creak(0.75, 14, 24, 0.62)), 0.1)
            place(x, pk(timber_tick(H.RNG.uniform(0.85, 1.1))) * 0.45, 1.0)
        elif k == 1:  # a joist ticking as it cools
            at = 0.05
            for _ in range(int(H.RNG.integers(4, 6))):
                place(x, timber_tick(H.RNG.uniform(0.8, 1.2)) * H.RNG.uniform(0.4, 1.0), at)
                at += H.RNG.uniform(0.12, 0.3)
        elif k == 2:  # long low creak of a beam under load
            place(x, pk(S.wood_creak(1.25, 12, 20, 0.55)), 0.05)
            place(x, pk(S.wood_creak(0.5, 18, 26, 0.8)) * 0.35, 0.65)
        else:  # a loud pop, then the timber eases
            place(x, pk(timber_tick(0.7) + modal([95, 170], [0.05, 0.03], [0.6, 0.3], 0.15)), 0.05)
            place(x, pk(S.wood_creak(0.45, 16, 26, 0.75)) * 0.5, 0.22)
            place(x, pk(timber_tick(1.15)) * 0.25, 1.05)
        x = reverb(lp(x, 5000), STAIR, wet=0.4)
        emit(f'Ambience/A_House_Settle_{k + 1:02d}.wav', one_shot(x, -23, 0.002, 0.3), 'Ambient', radius=2500,
             caption='[The house creaks]')


# =======================================================================================
# Haunting events
# =======================================================================================

def haunt_door_slam():
    for k in range(2):
        seed(4401 + k)
        d = 2.6
        x = np.zeros(N(d))
        s = H.RNG.uniform(0.92, 1.08)
        sw = 0.16
        place(x, bp(pink(sw), 120, 900) * (H.t(sw) / sw) ** 3 * 0.25, 0.0)  # air pushed ahead of the door
        hit = 0.16
        slab = modal(np.array([66, 101, 149, 228, 311, 447, 630, 910]) * s,
                     [0.24, 0.17, 0.13, 0.1, 0.075, 0.055, 0.04, 0.025], [1.0, 0.85, 0.7, 0.55, 0.42, 0.3, 0.2, 0.12], 1.4)
        slab += modal([43, 71], [0.3, 0.2], [0.5, 0.3], 1.4)  # the wall flexes
        crack = lp(white(0.1), 6500) * env_exp(0.1, 0.0035) * 0.9
        knock = bp(white(0.25), 250, 2500) * env_exp(0.25, 0.022) * 0.7
        tb = H.t(0.5)
        thump = np.sin(2 * np.pi * np.cumsum(48 + 60 * np.exp(-tb / 0.03)) / H.SR) * np.exp(-tb / 0.11) * 0.7
        for snd in (slab * 0.6, crack, knock, thump):
            place(x, snd, hit)
        place(x, S.latch(0.85) * 0.9, hit + 0.004)
        place(x, S.latch(1.05) * 0.5, hit + 0.028)  # bolt springs into the strike plate
        at, amp = hit + 0.05, 0.35
        for _ in range(int(H.RNG.integers(3, 6))):  # handle and escutcheon rattle
            fr = np.array([1750, 2900, 4300]) * H.RNG.uniform(0.9, 1.1)
            place(x, modal(fr, [0.012, 0.008, 0.005], [1, 0.6, 0.3], 0.06) * amp, at)
            at += H.RNG.uniform(0.03, 0.06)
            amp *= 0.6
        tr = H.t(0.5)
        gate = np.clip(np.sin(2 * np.pi * 27 * tr), 0, 1) ** 4
        place(x, bp(white(0.5), 1400, 5000) * env_exp(0.5, 0.12) * gate * 0.08, hit + 0.02)  # picture frame buzz
        x = reverb(x, ROOM, wet=0.35) + reverb(x, STAIR, wet=0.3, dry=0)
        emit(f'Ambience/A_Haunt_Door_Slam_{k + 1:02d}.wav', one_shot(x, -18, 0.002, 0.4), 'Ambient', radius=4000,
             caption='[A door slams]')


def haunt_drag():
    for k in range(2):
        seed(4410 + k)
        d = 3.4
        tt = H.t(d)
        pushes = [(0.12, 1.05), (1.45, 1.35)] if k == 0 else [(0.1, 0.55), (0.85, 0.6), (1.75, 1.1)]
        speed = np.zeros(N(d))
        for st, ln in pushes:
            uu = np.clip((tt - st) / ln, 0, 1)
            seg = np.sin(np.pi * uu) ** 0.7 * ((tt >= st) & (tt <= st + ln))
            speed = np.maximum(speed, seg * H.RNG.uniform(0.75, 1.0))
        sp = speed * (1 + 0.35 * (S.smooth_noise(d, 9) - 0.5))
        exc = S.stick_slip(d, 22 + 70 * sp, jitter=0.35) * sp
        body = sum(a * resonator(exc, f, q) for f, q, a in ((92, 7, 1.0), (168, 9, 0.8), (255, 10, 0.6), (405, 12, 0.4), (640, 14, 0.25)))
        boards = sum(a * resonator(exc, f, q) for f, q, a in ((140, 8, 0.6), (330, 10, 0.4), (720, 12, 0.2)))
        scrape = bp(white(d), 180, 2200) * lp(np.abs(white(d)), 120) * sp
        rumble = lp(brown(d), 90) * speed
        x = u(body) * 1.0 + u(boards) * 0.6 + u(scrape) * 0.55 + u(rumble) * 0.5
        for st, ln in pushes:  # jolt as it starts moving, thump as it is set down
            place(x, modal([70, 128, 215], [0.09, 0.06, 0.04], [1, 0.6, 0.3], 0.4) * 2.0, st + 0.02)
            place(x, modal([62, 115, 190], [0.12, 0.08, 0.05], [1, 0.6, 0.3], 0.5) * 2.5, st + ln - 0.05)
            for seam in np.arange(st + 0.2, st + ln - 0.15, H.RNG.uniform(0.22, 0.32)):  # legs bump board seams
                place(x, modal([150, 290], [0.03, 0.02], [1, 0.5], 0.1) * H.RNG.uniform(0.5, 1.0), seam)
        x = 0.8 * H.muffle_through_floor(x) + 0.2 * lp(x, 1400)  # upstairs, through the ceiling
        x = reverb(x, ROOM, wet=0.3)
        emit(f'Ambience/A_Haunt_Drag_{k + 1:02d}.wav', one_shot(x, -20, 0.01, 0.3), 'Ambient', radius=2500,
             caption='[Something is dragged across the floor]')


def wall_knock_one(s=1.0, force=1.0):
    d = 0.6
    y = modal(np.array([112, 188, 295, 440, 620]) * s, [0.07, 0.05, 0.04, 0.028, 0.02], [0.8, 1.0, 0.7, 0.45, 0.25], d)
    y += lp(white(d), 1100) * env_exp(d, 0.006) * 0.7
    y += sine(68 * s, d) * env_exp(d, 0.045) * 0.6
    y += resonator(y, 165, 4) * 0.5 + resonator(y, 340, 5) * 0.3  # hollow stud cavity
    return y * force


def haunt_knock_wall():
    patterns = [[(0.05, 1.0), (0.78, 0.95), (1.5, 1.1)], [(0.05, 0.9), (0.27, 0.8), (1.2, 1.15), (1.95, 0.5)]]
    for k, pat in enumerate(patterns):
        seed(4420 + k)
        d = 2.6
        x = np.zeros(N(d))
        s = H.RNG.uniform(0.9, 1.1)
        for at, g in pat:
            place(x, wall_knock_one(s * H.RNG.uniform(0.97, 1.03), g), at + H.RNG.uniform(-0.02, 0.02))
        x = peak(lp(x, 750, order=3), 140, 1.2, 4)  # plaster and studs swallow the highs
        x = reverb(x, ROOM, wet=0.28)
        emit(f'Ambience/A_Haunt_Knock_Wall_{k + 1:02d}.wav', one_shot(x, -20, 0.002, 0.35), 'Ambient', radius=2000,
             caption='[Knocking inside the walls]')


def haunt_footsteps_run():
    seed(4430)
    d = 3.2
    x = np.zeros(N(d))
    count = 13
    at = 0.12
    for i in range(count):
        uu = i / (count - 1)
        travel = 0.45 + 0.55 * np.sin(np.pi * (0.15 + 0.85 * uu))  # crosses overhead
        g = travel * (1.0 if i % 2 == 0 else 0.82) * H.RNG.uniform(0.85, 1.0)
        s = H.RNG.uniform(0.95, 1.12)
        step = modal(np.array([88, 152, 265, 410]) * s, [0.05, 0.035, 0.025, 0.015], [1.0, 0.65, 0.35, 0.15], 0.35)
        step += lp(white(0.35), 1400) * env_exp(0.35, 0.007) * 0.55
        if H.RNG.random() < 0.3:
            place(step, S.wood_creak(0.16, 35, 55, 0.9) * 0.18, 0.02)
        place(x, step * g, at)
        at += 0.185 + (0.012 if i % 2 else -0.012) + H.RNG.uniform(-0.012, 0.012)
    place(x, S.wood_creak(0.4, 16, 26, 0.8) * 0.25, at + 0.25)  # stops dead; one board eases
    x = 0.75 * H.muffle_through_floor(x) + 0.25 * lp(x, 1000)
    x = reverb(x, ROOM, wet=0.28)
    emit('Ambience/A_Haunt_Footsteps_Run_01.wav', one_shot(x, -21, 0.002, 0.3), 'Ambient', radius=3000,
         caption='[Running footsteps upstairs]')


def tine(f, dur, vel):
    """Music-box comb tine: clamped-bar partials (1 : 5.9 : 16.7), doubled tine, pin pluck."""
    tt = H.t(dur)
    base = float(np.clip(2.8 * (700.0 / f) ** 0.6, 0.6, 4.0))
    y = np.zeros_like(tt)
    for r, dk, a in ((1.0, 1.0, 1.0), (5.93, 0.12, 0.35), (16.7, 0.04, 0.12)):
        fr = f * r
        if fr > H.SR * 0.45:
            continue
        for fq, w in ((fr, 1.0), (fr * detune_ratio(3.0), 0.35)):
            y += a * w * np.exp(-tt / (base * dk)) * np.sin(2 * np.pi * fq * tt + H.RNG.uniform(0, 6.28))
    pluck = hp(white(0.006), 2500) * env_exp(0.006, 0.0007)
    y[:len(pluck)] += pluck * 0.25
    return y * vel * np.clip(tt / 0.0004, 0, 1) * np.clip((dur - tt) / 0.02, 0, 1)


def haunt_music_box():
    seed(4440)
    d = 10.6
    # An original lullaby in A minor, 3/4: (beats, melody note, accompaniment note).
    melody = [
        (1.0, 'E5', 'A3'), (1.0, 'A5', None), (1.0, 'C6', None),
        (1.5, 'B5', 'E3'), (0.5, 'A5', None), (1.0, 'G#5', None),
        (1.0, 'A5', 'F3'), (1.0, 'E5', None), (1.0, 'F5', None),
        (2.0, 'E5', 'E3'), (1.0, 'D#5', None),
        (1.0, 'E5', 'A3'), (1.0, 'C6', None), (1.0, 'E6', None),
        (1.5, 'D6', 'D4'), (0.5, 'C6', None), (1.0, 'B5', None),
        (1.0, 'A5', 'E3'), (1.0, 'G#5', None), (1.0, 'B5', None),
        (2.0, 'A5', 'A3'),
    ]
    total_beats = sum(b for b, _, _ in melody)
    beat0 = 0.29
    x = np.zeros(N(d))
    at, done = 0.06, 0.0
    onsets = []
    for beats, nm, acc in melody:
        frac = done / total_beats
        wind = max(0.0, (frac - 0.55) / 0.45)        # the spring runs down: tempo drags
        late = max(0.0, (frac - 0.68) / 0.32)        # ...and the last notes sag out of tune
        cents = -55 * late ** 1.5 + H.RNG.uniform(-4, 4)
        vel = H.RNG.uniform(0.7, 0.9) * (1 - 0.35 * late)
        place(x, tine(note_hz(nm) * detune_ratio(cents), 3.5, vel), at)
        if acc:
            place(x, tine(note_hz(acc) * detune_ratio(cents * 1.2), 3.5, vel * 0.55), at + 0.004)
        onsets.append(at)
        at += beats * beat0 * (1 + 1.7 * wind ** 2)
        done += beats
    stop = onsets[-1] + 1.2
    place(x, tine(note_hz('E5') * detune_ratio(-80), 2.5, 0.22), stop)  # one last, lonely pluck
    # Governor fan whirring, slowing with the cylinder, then the mechanism clicks to a halt.
    tt = H.t(d)
    rate = np.interp(tt, [0, onsets[int(len(onsets) * 0.55)], onsets[-1], stop + 0.1], [32, 32, 12, 5])
    whirr = bp(white(d), 900, 5000) * (0.5 + 0.5 * np.sin(2 * np.pi * np.cumsum(rate) / H.SR)) ** 3
    whirr *= np.clip((stop + 0.15 - tt) / 0.4, 0, 1)
    x += u(whirr) * 0.008
    place(x, modal([2200, 3500], [0.006, 0.004], [1, 0.5], 0.04) * 0.06, stop + 0.2)
    x = peak(peak(x, 520, 1.5, 4), 1250, 2.0, 2.5)
    x = reverb(lp(x, 10000), ROOM, wet=0.3)
    emit('Ambience/A_Haunt_MusicBox_01.wav', one_shot(x, -22, 0.002, 0.8), 'Ambient', radius=1500,
         caption='[A music box plays]')


def haunt_glass_fall():
    specs = [
        dict(ring=[1850, 4350, 7200, 10100], decays=[0.25, 0.12, 0.06, 0.03], frag=(3000, 12000), frag_decay=(0.01, 0.06), count=95),
        dict(ring=[1240, 2830, 4460, 6200], decays=[0.05, 0.035, 0.02, 0.012], frag=(1800, 8000), frag_decay=(0.006, 0.03), count=70),
    ]
    for k, sp in enumerate(specs):
        seed(4450 + k)
        d = 2.3
        x = np.zeros(N(d))
        ring = np.array(sp['ring']) * H.RNG.uniform(0.95, 1.05)
        sc = 0.16  # it scrapes over the shelf edge...
        place(x, bp(white(sc), 1500, 6000) * pk(lp(np.abs(white(sc)), 60)) * np.sin(np.pi * H.t(sc) / sc) * 0.05, 0.02)
        place(x, modal(ring, sp['decays'], [1, 0.6, 0.35, 0.2], 0.6) * 0.12, 0.05)
        place(x, modal(ring * 1.01, sp['decays'], [1, 0.6, 0.35, 0.2], 0.6) * 0.3 + timber_tick(1.3, 0.6) * 0.15, 0.2)
        hit = 0.7  # ...falls about a metre and hits the floor
        place(x, hp(white(0.03), 1000) * env_exp(0.03, 0.0025) * 0.5, hit)
        place(x, bp(white(0.12), 600, 12000) * env_exp(0.12, 0.014) * 0.6, hit)
        place(x, modal([95, 180, 320], [0.04, 0.03, 0.02], [1, 0.5, 0.3], 0.2) * 0.35, hit)
        place(x, modal(ring * 0.98, np.array(sp['decays']) * 0.25, [1, 0.7, 0.5, 0.3], 0.3) * 0.3, hit)
        for _ in range(sp['count']):  # shards: a dense burst thinning out, a few bouncing away late
            dt = H.RNG.exponential(0.11) if H.RNG.random() < 0.8 else H.RNG.uniform(0.15, 1.1)
            amp = H.RNG.uniform(0.15, 1.0) * np.exp(-dt / 0.35) * 0.3
            fr = np.sort(H.RNG.uniform(*sp['frag'], 3))
            dec = H.RNG.uniform(*sp['frag_decay'])
            place(x, modal(fr, [dec, dec * 0.7, dec * 0.5], [1, 0.6, 0.35], dec * 5 + 0.01) * amp, hit + 0.003 + dt)
        for _ in range(3):  # a shard skitters across the tiles
            ln = H.RNG.uniform(0.04, 0.12)
            sk = hp(white(ln), 3500) * np.sin(np.pi * H.t(ln) / ln) * 0.08
            place(x, sk, hit + H.RNG.uniform(0.1, 0.6))
        x = reverb(x, TILE, wet=0.32)
        emit(f'Ambience/A_Haunt_Glass_Fall_{k + 1:02d}.wav', one_shot(x, -20, 0.003, 0.3), 'Ambient', radius=2500,
             caption='[Glass shatters]')


def haunt_bang():
    seed(4460)
    d = 2.8
    x = np.zeros(N(d))
    hit = 0.01
    tb = H.t(0.8)
    sub = np.sin(2 * np.pi * np.cumsum(38 + 75 * np.exp(-tb / 0.04)) / H.SR) * np.exp(-tb / 0.22) * 1.5
    body = modal([51, 79, 116, 165, 231, 309, 418, 560], [0.32, 0.26, 0.2, 0.15, 0.11, 0.08, 0.06, 0.04],
                 [1, 0.9, 0.8, 0.65, 0.5, 0.35, 0.25, 0.15], 1.6)
    crack = lp(white(0.12), 7000) * env_exp(0.12, 0.005) * 1.8
    boom = bp(white(0.4), 180, 2200) * env_exp(0.4, 0.045) * 0.8
    for snd in (sub, body, crack, boom):
        place(x, snd, hit)
    tr = H.t(1.2)  # things on the wall jump: a frame rattles, a glass chatters
    gate = np.clip(np.sin(2 * np.pi * 23 * tr + 3 * S.smooth_noise(1.2, 6)), 0, 1) ** 3
    place(x, bp(white(1.2), 1500, 6000) * gate * env_exp(1.2, 0.22) * 0.12, hit + 0.025)
    at = hit + 0.03
    for _ in range(5):
        fr = np.array([2600, 4300, 6100]) * H.RNG.uniform(0.85, 1.2)
        place(x, modal(fr, [0.03, 0.02, 0.012], [1, 0.5, 0.3], 0.12) * H.RNG.uniform(0.05, 0.15), at)
        at += H.RNG.uniform(0.04, 0.09)
    x = reverb(x, STAIR, wet=0.45) + reverb(x, ROOM, wet=0.2, dry=0)
    emit('Ambience/A_Haunt_Bang_01.wav', one_shot(x, -16, 0.0005, 0.5), 'Ambient', radius=4000, caption='[A loud bang]')


def breath_noise(dur, formants, q=3.5, bright=0.3):
    src = white(dur)
    y = sum(a * resonator(src, f, q) for f, a in zip(formants, (1.0, 0.6, 0.35)))
    y = u(y) + u(bp(src, 1800, 6000)) * bright * 0.25 + u(bp(src, 90, 400)) * 0.25
    return y


def shaky(dur):
    return 1 + 0.35 * (S.smooth_noise(dur, 11) - 0.5) + 0.25 * (S.smooth_noise(dur, 4) - 0.5)


def exhale(dur, formants, catches, fry_from=0.55, fry_rate=(38, 55)):
    tt = H.t(dur)
    uu = tt / dur
    env = np.clip(tt / 0.12, 0, 1) * (1 - uu) ** 0.9 * shaky(dur)
    for c, w in catches:  # the breath catches
        env *= 1 - 0.8 * np.exp(-((tt - c) / w) ** 2)
    air = breath_noise(dur, formants, q=3.0)
    rate = np.full(N(dur), H.RNG.uniform(*fry_rate)) * (1 - 0.25 * uu)
    pulses = S.stick_slip(dur, rate, jitter=0.3)
    fry = resonator(pulses, formants[0], 6) + 0.6 * resonator(pulses, formants[1], 7)
    ramp = np.clip((uu - fry_from) / (1 - fry_from), 0, 1) ** 1.5
    return (u(air) + u(fry) * 0.55 * ramp) * env


def haunt_breath():
    seed(4470)  # a slow inhale, then a long ragged exhale ending in a throaty rattle
    d = 3.2
    x = np.zeros(N(d))
    di = 0.75
    ti = H.t(di)
    inh = breath_noise(di, (380, 2100, 2900), q=5, bright=0.6) * np.sin(np.pi * ti / di) ** 1.2 * (ti / di) ** 0.4 * shaky(di)
    place(x, inh * 0.45, 0.08)
    place(x, exhale(1.95, (560, 1150, 2500), [(0.42, 0.03), (0.95, 0.045)]), 1.05)
    x = reverb(lp(peak(x, 160, 1.0, 3), 6500), SMALL, wet=0.1)
    emit('Ambience/A_Haunt_Breath_01.wav', one_shot(x, -24, 0.02, 0.3), 'Dialogue', radius=600,
         caption='[Breathing close by]')

    seed(4471)  # one long, shaky exhale that catches twice and dies in a rattle
    x = np.zeros(N(d))
    place(x, exhale(2.75, (500, 1050, 2350), [(0.6, 0.04), (1.35, 0.06)], fry_from=0.45, fry_rate=(32, 45)), 0.1)
    x = reverb(lp(peak(x, 150, 1.0, 3.5), 6000), SMALL, wet=0.1)
    emit('Ambience/A_Haunt_Breath_02.wav', one_shot(x, -24, 0.02, 0.3), 'Dialogue', radius=600,
         caption='[Breathing close by]')


def haunt_drone():
    with music_rate():
        seed(4480)
        L = 30.0
        n = N(L)
        tt = H.t(L)
        hall = make_ir(duration=5.0, decay=3.6, predelay=0.03, damping=5000, stereo=True, brightness=0.4)
        swell = 0.5 - 0.5 * np.cos(2 * np.pi * tt / 15.0)       # two dread swells per loop
        slow = 0.5 - 0.5 * np.cos(2 * np.pi * tt / 30.0 + 1.0)  # one slow tide per loop
        cut = 260 + 520 * swell

        def voice(f, cutoff, bow=0.08):
            f = qf(f, L)
            v = np.zeros(n)
            for k in range(1, 25):
                if f * k > 6000:
                    break
                v += np.sin(2 * np.pi * f * k * tt + H.RNG.uniform(0, 6.28)) / k
            v = ctv_lowpass(ctv_lowpass(v, cutoff), cutoff * 1.4)
            return u(v) + u(circ(bp, cnoise(L), f * 0.9, min(f * 6, 9000))) * bow

        st = np.zeros((2, n))
        sub = (np.sin(2 * np.pi * qf(36.71, L) * tt) + 0.3 * np.sin(2 * np.pi * qf(73.42, L) * tt + 0.5)) * (0.55 + 0.45 * swell)
        st += u(sub) * 0.55
        for f, beat, p, a in ((note_hz('D2'), 0.2, -0.5, 1.0), (note_hz('A2'), 0.133, 0.45, 0.7), (note_hz('D3'), 0.1, -0.2, 0.45)):
            v = voice(f, cut) + voice(f + beat, cut * 1.1)  # detuned pair: slow beating
            st += pan(u(v) * a, p)
        st += pan(u(voice(note_hz('D#3'), cut * 0.8)) * (0.15 + 0.85 * slow) ** 2 * 0.3, 0.35)   # minor second creeps in
        st += pan(u(voice(note_hz('G#3'), cut * 0.8)) * (1 - slow) ** 2 * 0.25, -0.35)            # tritone ebbs away
        organ = sum(a * np.sin(2 * np.pi * qf(note_hz('D4') * r, L) * tt + H.RNG.uniform(0, 6))
                    for r, a in ((1, 0.6), (1.5, 0.3), (2, 0.25), (3, 0.12)))
        st += np.stack([organ, np.roll(organ, N(0.011))]) * swell ** 2 * 0.06
        env = np.sin(np.pi * tt / L) ** 4
        for c in range(2):
            shimmer = sum(circ(resonator, cnoise(L), qf(fr, L), 150) for fr in (1174.66, 1661.2, 2489.0))
            st[c] += u(shimmer) * env * 0.035
        st = creverb(st, hall, wet=0.4)
        emit('Music/A_Haunt_Drone_Loop.wav', finish(st, -22, loop=True), 'Music', loop=True, spatial=False, radius=0)


def scrape_string(f, dur, bend=0.0, trem_rate=0.0, pressure=1.0):
    """Sul ponticello / overpressure bowing: jittery saw, scratch noise, violin-body resonances."""
    tt = H.t(dur)
    uu = tt / dur
    jit = lp(H.RNG.standard_normal(len(tt)), 30)
    fc = f * 2 ** (bend * uu / 12) * (1 + 0.006 * np.sin(2 * np.pi * H.RNG.uniform(5, 7) * tt) + 0.004 * jit / (np.std(jit) + 1e-9))
    ph = np.cumsum(fc) / H.SR
    v = sum(np.sin(2 * np.pi * k * ph + H.RNG.uniform(0, 6)) / k for k in range(1, min(30, int(9000 / f)) + 1))
    scratch = bp(white(dur), 1500, 8000) * (0.5 + 0.5 * pk(lp(np.abs(white(dur)), 80))) * pressure
    y = u(hp(v, 700)) + u(scratch) * 0.35
    for fr, q, g in ((450, 2, 3), (1100, 2.5, 4), (2800, 3, 3)):
        y = peak(y, fr, q, g)
    if trem_rate:
        y *= 0.55 + 0.45 * np.sin(2 * np.pi * trem_rate * tt)
    return y


def haunt_stingers():
    specs = [(['E5', 'F5', 'A#5', 'B5', 'F6'], 1.0, 0.0), (['C#5', 'D5', 'G#5', 'A5', 'D#6'], -2.0, 0.55)]
    with music_rate():
        for k, (notes, bend, pre) in enumerate(specs):
            seed(4490 + k)
            hall = make_ir(duration=4.0, decay=2.8, predelay=0.02, damping=5000, stereo=True, brightness=0.4)
            d = 3.6
            mix = np.zeros((2, N(d)))
            hit = 0.04 + pre
            if pre:  # reversed reverb swell sucking into the hit
                burst = bp(white(0.12), 300, 6000) * env_exp(0.12, 0.02)
                swell = np.stack([signal.fftconvolve(burst, hall[c])[:N(pre)][::-1] for c in range(2)])
                swell *= np.linspace(0, 1, swell.shape[1]) ** 2
                place(mix, pk(swell) * 0.5, hit - pre)
            for i, nm in enumerate(notes):
                ln = d - hit
                v = scrape_string(note_hz(nm) * detune_ratio(H.RNG.uniform(-12, 12)), ln, bend=bend,
                                  trem_rate=H.RNG.uniform(13, 17) if i % 2 else 0.0, pressure=1.2)
                ts = H.t(ln)
                env = np.clip(ts / 0.015, 0, 1) * (0.7 * np.exp(-ts / 1.1) + 0.3 * np.exp(-ts / 0.15))
                place(mix, pan(v * env, -0.8 + 1.6 * i / (len(notes) - 1)), hit, gain=0.3)
            low = M.boom(3.0) + modal([58, 91], [0.5, 0.3], [0.6, 0.3], 3.0)
            growl = (M.saw_voice(note_hz('D2'), 2.5, voices=3) + M.saw_voice(note_hz('D#2'), 2.5, voices=3))
            growl = lp(growl, 500) * env_exp(2.5, 0.7, attack=0.01)
            place(mix, np.stack([low, low]), hit)
            place(mix, pan(growl, 0.0), hit, gain=0.6)
            mix = reverb(mix, hall, wet=0.45)
            emit(f'Music/M_Haunt_Stinger_{k + 1:02d}.wav', one_shot(mix, -17, 0.002, 0.6), 'Music', spatial=False, radius=0)


def heartbeat_loop():
    seed(4510)
    L = 3.2
    x = np.zeros(N(L))
    tb = H.t(0.35)
    period = L / 3  # 56 bpm
    for i in range(3):
        at = 0.02 + i * period + H.RNG.uniform(-0.012, 0.012)
        g = H.RNG.uniform(0.9, 1.0)
        s1 = np.sin(2 * np.pi * np.cumsum(38 + 28 * np.exp(-tb / 0.03)) / H.SR) * np.clip(tb / 0.006, 0, 1) * np.exp(-tb / 0.075)
        s1 += lp(white(0.35), 160) * env_exp(0.35, 0.03, attack=0.004) * 0.4
        s2 = np.sin(2 * np.pi * np.cumsum(55 + 25 * np.exp(-tb / 0.02)) / H.SR) * np.clip(tb / 0.005, 0, 1) * np.exp(-tb / 0.05)
        s2 += lp(white(0.35), 200) * env_exp(0.35, 0.02, attack=0.003) * 0.3
        cplace(x, s1 * g, at)
        cplace(x, s2 * g * 0.7, at + 0.3 + H.RNG.uniform(-0.01, 0.01))
    tt = H.t(L)
    flow = np.zeros(N(L))  # blood rushing after each beat
    for i in range(3):
        flow += np.exp(-((((tt - (0.12 + i * period)) % L) - 0.0) / 0.18) ** 2)
    whoosh = circ(lp, cnoise(L, 'brown'), 70) * flow
    x = pk(x) + u(whoosh) * 0.04
    x = circ(peak, circ(lp, x, 220), 75, 1.2, 3)
    x = creverb(x, SMALL, wet=0.06)
    emit('SFX/S_Heartbeat_Loop.wav', finish(x, -21, loop=True), 'SFX', loop=True, spatial=False, radius=0)


# =======================================================================================
# Lights / devices
# =======================================================================================

def light_bulb_pop():
    seed(4700)
    d = 1.1
    x = np.zeros(N(d))
    pop_at = 0.42
    tb = H.t(pop_at)
    hum = sum(np.sin(2 * np.pi * 120 * h * tb + H.RNG.uniform(0, 6.28)) / h ** 0.8 for h in range(1, 58))
    flick = lp((S.smooth_noise(pop_at, 30) > 0.35).astype(float), 300, order=1)
    grow = (tb / pop_at) ** 1.5
    buzz = hp(hum, 400) * flick * grow  # the filament sings and flickers before it goes
    arcs = bp(poisson(pop_at, 60 * grow), 2000, 9000)
    place(x, u(buzz) * 0.12 + pk(arcs) * 0.12, 0.0)
    crack = hp(white(0.02), 900) * env_exp(0.02, 0.0012)
    tp = H.t(0.05)
    thump = np.sin(2 * np.pi * np.cumsum(90 + 120 * np.exp(-tp / 0.004)) / H.SR) * np.exp(-tp / 0.012) * 0.6
    glass = modal(np.array([3150, 4720, 6900, 9350]) * H.RNG.uniform(0.95, 1.05), [0.06, 0.04, 0.025, 0.015], [1, 0.7, 0.45, 0.25], 0.4) * 0.35
    place(x, crack, pop_at)
    place(x, thump, pop_at)
    place(x, glass, pop_at + 0.001)
    at = pop_at + 0.05
    for _ in range(int(H.RNG.integers(3, 6))):  # broken filament tinkles
        fr = np.array([6800, 9600]) * H.RNG.uniform(0.85, 1.15)
        place(x, modal(fr, [0.006, 0.004], [1, 0.5], 0.03) * H.RNG.uniform(0.05, 0.15), at)
        at += H.RNG.uniform(0.03, 0.09)
    x = reverb(x, ROOM, wet=0.3)
    emit('SFX/S_Light_Bulb_Pop.wav', one_shot(x, -21, 0.002, 0.25), 'SFX', radius=1500, caption='[A light bulb pops]')


def light_buzz_on():
    seed(4701)
    d = 1.15
    tt = H.t(d)
    x = np.zeros(N(d))
    hum = sum(np.sin(2 * np.pi * 120 * h * tt + H.RNG.uniform(0, 6.28)) / h ** 0.9 for h in range(1, 66))
    s = np.sin(2 * np.pi * 120 * tt)
    zz = bp(np.clip(s, 0.92, 1) - 0.92, 1800, 7000)
    gate = np.zeros(N(d))
    for st, ln in ((0.03, 0.06), (0.17, 0.05), (0.29, 0.025), (0.36, 0.79)):
        gate[N(st):N(st + ln)] = 1.0
    gate = lp(gate, 150, order=1) * np.clip((d - tt) / 0.3, 0, 1)
    whine = np.sin(2 * np.pi * 10200 * tt) * gate * 0.01
    x += (u(hum) * 0.6 + u(zz) * 0.35) * gate + whine
    for at in (0.0, 0.14, 0.26):  # starter ticks
        place(x, modal([2900, 4500], [0.004, 0.003], [1, 0.5], 0.03) * 0.5, at)
    place(x, modal([2500, 5800], [0.03, 0.02], [1, 0.4], 0.15) * 0.2, 0.36)  # the tube strikes
    x = reverb(x, ROOM, wet=0.22)
    emit('SFX/S_Light_Buzz_On.wav', one_shot(x, -23, 0.001, 0.15), 'SFX', radius=1200)


def tv_power_on():
    seed(4702)
    d = 1.6
    x = np.zeros(N(d))
    click = modal([1450, 2650, 3900], [0.008, 0.006, 0.004], [1, 0.6, 0.35], 0.2) + hp(white(0.2), 2500) * env_exp(0.2, 0.001) * 0.4
    thunk = modal([125, 215, 380], [0.05, 0.035, 0.02], [1, 0.6, 0.3], 0.2)
    place(x, click * 0.25 + thunk * 0.3, 0.0)
    dg = 0.7
    tg = H.t(dg)
    buzz = np.tanh(3 * np.sin(2 * np.pi * 60 * tg)) + 0.3 * np.sin(2 * np.pi * 120 * tg)
    buzz = lp(buzz, 1800) * np.exp(-tg / 0.16) * np.clip(tg / 0.01, 0, 1)  # degauss coil
    boing = modal([168, 271, 452, 610], [0.25, 0.18, 0.12, 0.08], [1, 0.6, 0.4, 0.2], dg) * np.clip(tg / 0.005, 0, 1)
    place(x, buzz * 0.3 + boing * 0.22, 0.05)
    sc = 0.7
    ts = H.t(sc)
    crackle = hp(poisson(sc, 900 * np.exp(-ts / 0.25)), 1800) * 0.5 + bp(white(sc), 3000, 10000) * np.exp(-ts / 0.12) * 0.15
    place(x, crackle * np.clip(ts / 0.005, 0, 1), 0.08)  # high-voltage static on the glass
    tw = H.t(d - 0.1)
    fw = 15734 - 7000 * np.exp(-tw / 0.06)
    place(x, np.sin(2 * np.pi * np.cumsum(fw) / H.SR) * np.clip(tw / 0.15, 0, 1) * 0.02, 0.1)  # flyback whine
    st_d = 1.2
    tst = H.t(st_d)
    static = bp(white(st_d), 250, 9000) * (0.85 + 0.15 * np.clip(np.sin(2 * np.pi * 60 * tst) * 4, -1, 1))
    place(x, static * np.clip(tst / 0.04, 0, 1) * (0.45 + 0.55 * np.exp(-tst / 0.2)) * 0.25, 0.35)
    x = reverb(x, ROOM, wet=0.25)
    emit('SFX/S_TV_Power_On.wav', one_shot(x, -21, 0.0005, 0.3), 'SFX', radius=1500, caption='[A television switches on]')


# =======================================================================================
# Gameplay SFX
# =======================================================================================

def cloth_rustle(d, env):
    folds = pk(lp(np.abs(white(d)), 35))  # fabric catching and releasing
    crinkle = bp(poisson(d, 900 * env), 1800, 7000)
    swish = bp(white(d), 600, 4500) * env * (0.35 + 0.65 * folds)
    return u(crinkle) * 0.35 + u(swish) * 0.6


def loot_pickup_small():
    for k in range(3):
        seed(4710 + k)
        d = 0.55
        tt = H.t(d)
        x = np.zeros(N(d))
        lift = modal(np.array([1900, 3400]) * H.RNG.uniform(0.8, 1.2), [0.005, 0.003], [1, 0.5], 0.03) * 0.25
        place(x, lift + hp(white(0.03), 2000) * env_exp(0.03, 0.002) * 0.15, 0.01)  # leaves the surface
        a = H.RNG.uniform(0.07, 0.11)
        b = H.RNG.uniform(0.33, 0.4)
        env = lp(np.clip((tt - a) / 0.05, 0, 1) * np.clip((b - tt) / 0.12, 0, 1), 40)
        x += cloth_rustle(d, env) * 0.18  # into the pocket
        if k == 0:  # coins chink
            for at in (0.3, 0.335):
                fr = np.array([5200, 8100, 11300]) * H.RNG.uniform(0.9, 1.1)
                place(x, modal(fr, [0.04, 0.025, 0.015], [1, 0.6, 0.35], 0.12) * H.RNG.uniform(0.25, 0.4), at)
        elif k == 1:  # a ring on a fine chain
            fr = np.array([3800, 6350, 9100]) * H.RNG.uniform(0.95, 1.05)
            place(x, modal(fr, [0.09, 0.05, 0.03], [1, 0.5, 0.3], 0.25) * 0.3, 0.28)
            for at in 0.3 + np.cumsum(H.RNG.uniform(0.008, 0.02, 5)):
                place(x, modal(np.array([7400, 9900]) * H.RNG.uniform(0.9, 1.1), [0.004, 0.003], [1, 0.5], 0.02) * 0.08, at)
        else:  # a pat on the pocket
            place(x, lp(white(0.08), 500) * env_exp(0.08, 0.012) * 0.5, 0.37)
        x = reverb(x, SMALL, wet=0.15)
        emit(f'SFX/S_Loot_Pickup_Small_{k + 1:02d}.wav', one_shot(x, -23, 0.002, 0.08), 'SFX', radius=800)


def loot_pickup_heavy():
    seed(4715)
    d = 0.9
    tt = H.t(d)
    x = np.zeros(N(d))
    sc = 0.2  # slides a little on the surface first
    exc = S.stick_slip(sc, np.full(N(sc), 60.0), jitter=0.4) * np.sin(np.pi * H.t(sc) / sc)
    scrape = resonator(exc, 310, 6) + 0.7 * resonator(exc, 720, 8) + 0.4 * resonator(exc, 1500, 10)
    place(x, pk(scrape) * 0.3 + u(bp(white(sc), 200, 2500) * np.sin(np.pi * H.t(sc) / sc)) * 0.12, 0.0)
    at = 0.2
    for i in range(12):  # cables and plugs clatter as it comes up
        if H.RNG.random() < 0.6:
            fr, dec = np.array([1100, 2300, 3700]) * H.RNG.uniform(0.8, 1.25), H.RNG.uniform(0.01, 0.02)
        else:
            fr, dec = np.array([3500, 5600, 8200]) * H.RNG.uniform(0.9, 1.1), 0.01
        place(x, modal(fr, [dec, dec * 0.7, dec * 0.5], [1, 0.5, 0.3], 0.08) * H.RNG.uniform(0.12, 0.4) * (1 - i / 14), at)
        at += H.RNG.exponential(0.03) + 0.008
    sw = bp(white(d), 600, 3000) * bell_env(d, 0.35, 0.06) * 0.12
    x += sw
    thump = lp(white(0.3), 500) * env_exp(0.3, 0.03) + sine(95, 0.3) * env_exp(0.3, 0.05) * 0.8
    place(x, thump * 0.8, 0.55)  # hugged against the chest
    x = reverb(x, ROOM, wet=0.2)
    emit('SFX/S_Loot_Pickup_Heavy_01.wav', one_shot(x, -20, 0.002, 0.1), 'SFX', radius=1200)


def loot_drop():
    for k in range(2):
        seed(4716 + k)
        d = 0.65
        x = np.zeros(N(d))
        at = 0.01
        if k == 0:  # a heavy, soft bag of loot
            thud = modal(np.array([72, 128, 210, 340]) * H.RNG.uniform(0.95, 1.05), [0.09, 0.06, 0.04, 0.03], [1, 0.7, 0.4, 0.2], 0.5)
            thud += lp(white(0.5), 800) * env_exp(0.5, 0.012) * 0.8
            thud += bp(white(0.5), 500, 3000) * env_exp(0.5, 0.04) * 0.25
            place(x, thud, at)
            for _ in range(3):
                place(x, modal(np.array([2600, 4100]) * H.RNG.uniform(0.9, 1.1), [0.008, 0.005], [1, 0.5], 0.03) * 0.12,
                      at + H.RNG.uniform(0.01, 0.06))
        else:  # a box with things rattling inside, one small bounce
            box = modal(np.array([150, 320, 560, 900]) * H.RNG.uniform(0.95, 1.05), [0.04, 0.03, 0.02, 0.015], [1, 0.7, 0.45, 0.25], 0.4)
            box += lp(white(0.4), 2500) * env_exp(0.4, 0.005) * 0.7 + modal([80], [0.06], [0.6], 0.4)
            place(x, box, at)
            place(x, box * 0.3, at + 0.11)
            for _ in range(7):
                fr = np.array([1400, 2600, 3900]) * H.RNG.uniform(0.8, 1.3)
                place(x, modal(fr, [0.01, 0.007, 0.005], [1, 0.5, 0.3], 0.04) * H.RNG.uniform(0.1, 0.35), at + H.RNG.uniform(0.01, 0.16))
        x = reverb(x, ROOM, wet=0.25)
        emit(f'SFX/S_Loot_Drop_{k + 1:02d}.wav', one_shot(x, -19, 0.001, 0.12), 'SFX', radius=1500)


def bag_zip():
    seed(4720)
    d = 1.05
    tt = H.t(d)
    speed = np.zeros(N(d))
    for st, ln, g in ((0.03, 0.4, 1.0), (0.5, 0.36, 0.85)):
        uu = np.clip((tt - st) / ln, 0, 1)
        speed = np.maximum(speed, g * np.sin(np.pi * uu ** 0.8) ** 0.7 * ((tt >= st) & (tt <= st + ln)))
    sp = speed * (0.85 + 0.15 * S.smooth_noise(d, 20))
    teeth = S.stick_slip(d, 60 + 520 * sp, jitter=0.12) * sp
    metal = sum(a * resonator(teeth, f, q) for f, q, a in ((2250, 8, 1.0), (3450, 10, 0.8), (5200, 12, 0.5)))
    tick = bp(teeth, 1200, 9000)
    fabric = bp(white(d), 600, 4500) * sp * lp(np.abs(white(d)), 90)
    x = peak(u(metal) * 0.6 + u(tick) * 0.6 + u(fabric) * 0.35, 950, 1.5, 3)
    place(x, modal([2900, 4600, 6900], [0.012, 0.008, 0.005], [1, 0.6, 0.3], 0.06) * 1.5, 0.87)  # slider hits the stop
    x = reverb(x, SMALL, wet=0.15)
    emit('SFX/S_Bag_Zip_01.wav', one_shot(x, -21, 0.002, 0.1), 'SFX', radius=800)


def pin_click(bright=1.0, spring=False):
    f = H.RNG.uniform(3200, 4400) * bright
    y = modal([f, f * 1.87, f * 3.05], [0.004, 0.0025, 0.0015], [1, 0.5, 0.25], 0.03)
    y += hp(white(0.03), 3500) * env_exp(0.03, 0.0005) * 0.5
    if spring:
        y += modal([H.RNG.uniform(2500, 3100)], [0.02], [0.25], 0.03)
    return y


def lockpick_loop():
    seed(4730)
    L = 2.0
    x = np.zeros(N(L))
    for c in (0.08, 0.47, 0.83, 1.22, 1.63):  # raking: little clusters of pin clicks
        at = c + H.RNG.uniform(-0.05, 0.05)
        for _ in range(int(H.RNG.integers(2, 6))):
            cplace(x, pin_click(H.RNG.uniform(0.85, 1.15), H.RNG.random() < 0.3) * H.RNG.uniform(0.25, 1.0), at)
            at += H.RNG.uniform(0.02, 0.07)
    gate = np.clip((ccurve(L, 2.0) - 0.45) * 6, 0, 1)
    exc = S.stick_slip(L, 18 + 14 * ccurve(L, 1.5), jitter=0.4) * gate  # tension wrench creak
    creak = circ(resonator, exc, 1750, 18) + 0.6 * circ(resonator, exc, 2900, 22) + 0.3 * circ(resonator, exc, 4100, 25)
    scr = circ(bp, cnoise(L), 2500, 9000) * np.clip((ccurve(L, 3.0) - 0.6) * 5, 0, 1)  # pick along the wards
    x = pk(x) + pk(creak) * 0.15 + pk(scr) * 0.08
    x = creverb(x, SMALL, wet=0.12)
    emit('SFX/S_Lockpick_Loop.wav', finish(x, -26, loop=True), 'SFX', loop=True, radius=600)


def lockpick_success():
    seed(4731)
    d = 0.65
    x = np.zeros(N(d))
    tg = 0.12
    g = S.stick_slip(tg, np.full(N(tg), 380.0), jitter=0.3)
    grind = (resonator(g, 2600, 20) + 0.6 * resonator(g, 4100, 25)) * np.sin(np.pi * H.t(tg) / tg)
    place(x, pk(grind) * 0.18, 0.0)  # the plug turns
    place(x, modal([2400, 3900, 5600], [0.006, 0.004, 0.003], [1, 0.6, 0.3], 0.04) * 0.5, 0.11)  # cam engages
    clack = modal([1080, 1890, 2840, 4200], [0.025, 0.018, 0.012, 0.007], [1, 0.8, 0.5, 0.3], 0.3)
    clack += hp(white(0.3), 1500) * env_exp(0.3, 0.002) * 0.6
    door = modal([140, 265, 430], [0.05, 0.035, 0.02], [1, 0.6, 0.3], 0.3) * 0.7
    place(x, clack + door, 0.16)  # bolt retracts
    place(x, modal([2900, 4700], [0.004, 0.003], [1, 0.4], 0.03) * 0.25, 0.235)
    x = reverb(x, ROOM, wet=0.22)
    emit('SFX/S_Lockpick_Success_01.wav', one_shot(x, -20, 0.001, 0.15), 'SFX', radius=800)


def crowbar_pry():
    seed(4740)
    d = 2.8
    x = np.zeros(N(d))

    def bar():
        return modal(np.array([870, 2380, 4630, 7600]) * H.RNG.uniform(0.97, 1.03), [0.3, 0.14, 0.07, 0.035], [1, 0.7, 0.45, 0.25], 0.6)

    place(x, bar() * 0.5 + modal([180, 330, 560], [0.04, 0.03, 0.02], [1, 0.6, 0.3], 0.6) * 0.6, 0.02)  # wedged in
    place(x, bar() * 0.25, 0.11)
    s0, s1 = 0.18, 1.6  # the frame takes the strain
    ls = s1 - s0
    us = H.t(ls) / ls
    rate = (7 + 32 * us ** 1.4) * (1 + 0.2 * (S.smooth_noise(ls, 6) - 0.5))
    exc = S.stick_slip(ls, rate, jitter=0.3) * (0.25 + 0.75 * us ** 1.5)
    groan = sum(a * resonator(exc, f, q) for f, q, a in ((120, 7, 1.0), (265, 9, 0.8), (520, 11, 0.55), (880, 13, 0.35), (1500, 15, 0.2)))
    fibres = bp(poisson(ls, 3 + 120 * us ** 3), 900, 7000) * us
    strain = resonator(S.stick_slip(ls, 260 + 200 * us, jitter=0.2), 2250, 30) * us ** 2
    place(x, (pk(groan) * 0.8 + pk(fibres) * 0.45 + pk(strain) * 0.06) * 0.5, s0)
    c = s1  # CRACK
    place(x, white(0.05) * env_exp(0.05, 0.0035) * 1.0, c)
    place(x, lp(white(0.25), 3500) * env_exp(0.25, 0.03), c)
    place(x, modal([63, 104, 168, 251, 380, 560, 820], [0.22, 0.17, 0.12, 0.09, 0.06, 0.045, 0.03],
                   [1, 0.9, 0.75, 0.55, 0.4, 0.28, 0.18], 1.0) * 1.6, c)
    place(x, bp(poisson(0.5, 1800 * np.exp(-H.t(0.5) / 0.09)), 700, 7500) * 0.8, c)  # splinters
    place(x, modal([1650, 3300, 5100], [0.15, 0.09, 0.05], [1, 0.5, 0.3], 0.5) * 0.35, c + 0.012)  # strike plate tears
    place(x, bar() * 0.3, c + 0.07)  # bar slips and clanks
    place(x, S.wood_creak(0.55, 20, 38, 1.35) * 0.3, c + 0.4)  # the door eases open
    x = reverb(x, ROOM, wet=0.25) + reverb(x, STAIR, wet=0.18, dry=0)
    emit('SFX/S_Crowbar_Pry_01.wav', one_shot(x, -16.5, 0.001, 0.3), 'SFX', radius=3500, caption='[Wood cracks loudly]')


def door_locked_rattle():
    seed(4741)
    d = 0.85
    x = np.zeros(N(d))
    for at, g in ((0.02, 1.0), (0.26, 0.85), (0.47, 0.75)):
        at += H.RNG.uniform(-0.015, 0.015)
        place(x, modal(np.array([2100, 3500, 5200]) * H.RNG.uniform(0.95, 1.05), [0.008, 0.005, 0.003], [1, 0.5, 0.3], 0.04) * 0.35 * g, at)
        hit = at + H.RNG.uniform(0.035, 0.05)  # latch bolt hits the keeper, the door shifts in its frame
        place(x, modal(np.array([1300, 2250, 3600]) * H.RNG.uniform(0.95, 1.05), [0.015, 0.01, 0.006], [1, 0.6, 0.3], 0.08) * 0.7 * g, hit)
        place(x, (modal([105, 190, 330], [0.05, 0.035, 0.02], [1, 0.6, 0.35], 0.25) * 1.3
                  + lp(white(0.25), 1200) * env_exp(0.25, 0.006) * 0.5) * g, hit)
        place(x, modal([118, 210], [0.03, 0.02], [1, 0.5], 0.1) * 0.5 * g, hit + 0.018)
        place(x, modal([2600, 4200], [0.006, 0.004], [1, 0.5], 0.03) * 0.2 * g, at + 0.12)  # handle springs back
    x = reverb(x, ROOM, wet=0.25)
    emit('SFX/S_Door_Locked_Rattle_01.wav', one_shot(x, -19, 0.001, 0.12), 'SFX', radius=1200, caption='[The door is locked]')


def key_pickup():
    seed(4750)
    d = 0.55
    x = np.zeros(N(d))
    keys = [np.array([2650, 5900, 9300]) * H.RNG.uniform(0.85, 1.35) for _ in range(4)]
    ring = np.array([4400, 7300, 10400])

    def hit(at, amp):
        if H.RNG.random() < 0.25:
            fr, dec = ring * H.RNG.uniform(0.98, 1.02), 0.06
        else:
            fr, dec = keys[int(H.RNG.integers(0, 4))], H.RNG.uniform(0.025, 0.06)
        place(x, modal(fr, [dec, dec * 0.6, dec * 0.4], [1, 0.55, 0.3], dec * 5) * amp, at)

    for _ in range(9):
        hit(0.01 + H.RNG.exponential(0.035), H.RNG.uniform(0.3, 1.0))
    for _ in range(5):
        hit(0.2 + H.RNG.exponential(0.03), H.RNG.uniform(0.15, 0.5))
    place(x, lp(white(0.06), 900) * env_exp(0.06, 0.01) * 0.25, 0.34)  # the hand closes around them
    x = reverb(x, SMALL, wet=0.15)
    emit('SFX/S_Key_Pickup_01.wav', one_shot(x, -23, 0.001, 0.08), 'SFX', radius=600)


def window_slide():
    seed(4770)
    d = 1.8
    tt = H.t(d)
    segs = [(0.1, 0.42, 0.8), (0.62, 0.5, 1.0), (1.2, 0.22, 0.7)]  # forced up in spurts
    speed = np.zeros(N(d))
    for st, ln, g in segs:
        uu = np.clip((tt - st) / ln, 0, 1)
        speed = np.maximum(speed, g * np.sin(np.pi * uu) ** 0.6 * ((tt >= st) & (tt <= st + ln)))
    sp = speed * (0.75 + 0.25 * S.smooth_noise(d, 18))
    exc = S.stick_slip(d, 35 + 120 * sp, jitter=0.3) * sp
    wood = sum(a * resonator(exc, f, q) for f, q, a in ((240, 9, 0.9), (520, 12, 1.0), (980, 15, 0.7), (1650, 20, 0.45), (2400, 24, 0.25)))
    squeal = resonator(exc, 1280, 70) * lp((sp > 0.65).astype(float), 30)
    rub = bp(white(d), 350, 3200) * sp * lp(np.abs(white(d)), 200)
    x = u(wood) + u(squeal) * 0.3 + u(rub) * 0.5
    for st, ln, g in segs:  # jolts, the pane rattling in its putty
        place(x, modal([140, 300, 520], [0.05, 0.035, 0.02], [1, 0.6, 0.3], 0.2) * 1.2 * g, st)
        tr = H.t(0.12)
        place(x, bp(white(0.12), 2000, 6000) * env_exp(0.12, 0.03) * np.clip(np.sin(2 * np.pi * 31 * tr), 0, 1) ** 2 * 0.5 * g, st + 0.005)
    place(x, modal([118, 226, 410], [0.07, 0.05, 0.03], [1, 0.6, 0.3], 0.3) * 1.8, 1.43)  # stops at the top
    place(x, S.latch(0.7) * 0.3, 1.44)
    x = reverb(x, ROOM, wet=0.28)
    emit('SFX/S_Window_Slide_01.wav', one_shot(x, -21, 0.002, 0.2), 'SFX', radius=1500)


def glass_cut():
    seed(4760)
    d = 2.2
    x = np.zeros(N(d))
    s0, ls = 0.06, 1.3
    ts = H.t(ls)
    us = ts / ls
    pressure = (0.6 + 0.4 * np.sin(2 * np.pi * us + 0.5)) * np.clip(ts / 0.05, 0, 1) * np.clip((ls - ts) / 0.05, 0, 1)
    grit = poisson(ls, 6000 * pressure)  # the wheel crushing a line into the glass
    pane = sum(a * resonator(grit, f, q) for f, q, a in ((1840, 60, 0.4), (2710, 90, 1.0), (3950, 110, 0.8), (5230, 120, 0.5), (7400, 140, 0.3)))
    wheel = bp(grit, 4000, 14000)
    hiss = bp(white(ls), 3000, 12000) * pressure
    place(x, u(wheel) * 0.8 + u(pane) * 0.5 + u(hiss) * 0.35, s0)
    place(x, modal([3100, 5200], [0.01, 0.006], [1, 0.5], 0.05) * 0.8, s0 + ls - 0.02)
    pop_at = 1.72
    tp = H.t(0.08)
    pop = np.sin(2 * np.pi * np.cumsum(230 + 300 * np.exp(-tp / 0.008)) / H.SR) * np.exp(-tp / 0.018)
    pop += lp(white(0.08), 2500) * env_exp(0.08, 0.004) * 0.5
    place(x, hp(white(0.01), 2000) * env_exp(0.01, 0.0008) * 0.8, pop_at - 0.012)  # score line snaps
    place(x, pop * 1.2, pop_at)  # suction cup pulls the disc free
    place(x, modal([2300, 4150, 6200], [0.06, 0.04, 0.025], [1, 0.6, 0.35], 0.3) * 0.35, pop_at + 0.01)
    x = reverb(x, SMALL, wet=0.2)
    emit('SFX/S_Glass_Cut_01.wav', one_shot(x, -24, 0.002, 0.2), 'SFX', radius=1200)


def drawer_sound(opening, sticky):
    d = 0.65
    tt = H.t(d)
    x = np.zeros(N(d))
    st = 0.02
    ln = H.RNG.uniform(0.3, 0.38) if opening else H.RNG.uniform(0.22, 0.28)
    uu = np.clip((tt - st) / ln, 0, 1)
    speed = np.sin(np.pi * uu) ** 0.8 * ((tt >= st) & (tt <= st + ln))
    if sticky:
        speed *= 0.55 + 0.45 * lp((S.smooth_noise(d, 14) > 0.4).astype(float), 40)
    exc = S.stick_slip(d, 30 + (110 if sticky else 70) * speed, jitter=0.4) * speed
    box = sum(a * resonator(exc, f, q) for f, q, a in ((185, 6, 1.0), (340, 8, 0.7), (620, 10, 0.45), (1150, 12, 0.25)))
    rub = bp(white(d), 250, 2800) * lp(np.abs(white(d)), 150) * speed
    slide = 1.0 if opening else 0.55
    x += (u(box) * 0.6 + u(rub)) * slide
    if sticky:
        x += u(resonator(exc, 1650, 25)) * 0.2
    for _ in range(int(H.RNG.integers(2, 5))):  # contents shift
        fr = (np.array([2400, 4100]) if H.RNG.random() < 0.5 else np.array([900, 1750])) * H.RNG.uniform(0.85, 1.2)
        place(x, modal(fr, [0.01, 0.006], [1, 0.5], 0.05) * H.RNG.uniform(0.3, 0.8), st + H.RNG.uniform(0.05, ln + 0.06))
    if opening:
        stop = modal([210, 390, 680], [0.03, 0.02, 0.012], [1, 0.6, 0.3], 0.2) * 1.2 + lp(white(0.2), 1500) * env_exp(0.2, 0.004) * 0.6
    else:
        stop = modal([128, 236, 405, 690], [0.07, 0.05, 0.03, 0.02], [1, 0.75, 0.45, 0.25], 0.3) * 2.5
        stop += lp(white(0.3), 2000) * env_exp(0.3, 0.005) * 1.2
    place(x, stop, st + ln - 0.01)
    return reverb(x, ROOM, wet=0.22)


def drawers():
    for k in range(2):
        seed(4780 + k)
        emit(f'SFX/S_Drawer_Open_{k + 1:02d}.wav', one_shot(drawer_sound(True, k == 1), -21, 0.002, 0.1), 'SFX', radius=900)
    seed(4782)
    emit('SFX/S_Drawer_Close_01.wav', one_shot(drawer_sound(False, False), -19, 0.002, 0.1), 'SFX', radius=900)


def cabinet_open():
    seed(4790)
    d = 0.7
    x = np.zeros(N(d))
    place(x, modal([1650, 2750], [0.006, 0.004], [1, 0.5], 0.04) * 0.4 + modal([185, 350], [0.03, 0.02], [1, 0.5], 0.04) * 0.6, 0.01)
    place(x, S.wood_creak(0.36, 45, 85, 1.9) * 0.35, 0.06)  # hinge creak
    sq = S.stick_slip(0.3, np.linspace(380, 520, N(0.3)), jitter=0.15)
    place(x, resonator(sq, 2650, 30) * 0.08 * np.sin(np.pi * H.t(0.3) / 0.3), 0.08)
    sw = 0.4
    place(x, lp(pink(sw), 450) * np.sin(np.pi * H.t(sw) / sw) ** 2 * 0.12, 0.05)
    x = reverb(x, ROOM, wet=0.25)
    emit('SFX/S_Cabinet_Open_01.wav', one_shot(x, -22, 0.001, 0.12), 'SFX', radius=900)


def engine(d, rpm, load, rough=0.04, misfire=0.0, cyl=4):
    """Combustion pulses at the firing rate through exhaust-pipe resonances, plus crank hum and
    valve-train clatter."""
    n = N(d)
    rpm = np.asarray(rpm, dtype=float)[:n]
    load = np.asarray(load, dtype=float)[:n]
    fire = rpm / 60.0 * cyl / 2.0
    ph = np.cumsum(fire) / H.SR
    imp = np.zeros(n)
    for i in np.nonzero(np.diff(np.floor(ph)) > 0)[0] + 1:
        if H.RNG.random() < misfire:
            continue
        imp[i] = load[i] * max(0.2, 1 + rough * H.RNG.standard_normal())
    kd = 0.025
    tk = H.t(kd)
    kern = np.exp(-tk / 0.0035) + white(kd) * np.exp(-tk / 0.006) * 0.6
    puffs = signal.fftconvolve(imp, kern)[:n]
    exhaust = resonator(puffs, 85, 2.0) + 0.8 * resonator(puffs, 170, 2.5) + 0.45 * resonator(puffs, 360, 3) + 0.2 * lp(puffs, 1500)
    crank = 2 * np.pi * ph / (cyl / 2)
    hum = lp(sum(np.sin(k * crank + H.RNG.uniform(0, 6.28)) / k for k in range(1, 9)), 500) * (0.4 + 0.6 * load) * (rpm > 50)
    clatter = bp(white(d), 1800, 7000) * (0.5 + 0.5 * np.sin(crank * 2)) ** 6 * np.clip(rpm / 2500, 0.0, 1.5)
    return u(exhaust) + u(hum) * 0.3 + u(clatter) * 0.06


def van_engine_start():
    seed(4810)
    d = 3.4
    tt = H.t(d)
    x = np.zeros(N(d))
    place(x, modal([150, 410, 1900, 3100], [0.05, 0.03, 0.012, 0.008], [1, 0.7, 0.45, 0.3], 0.2) * 0.6, 0.04)  # solenoid
    cs, cl = 0.08, 1.2  # cranking: starter whine loaded by each compression stroke
    tc = H.t(cl)
    cph = np.cumsum(5.6 + 1.2 * tc / cl) / H.SR
    loadc = (0.5 + 0.5 * np.cos(2 * np.pi * cph)) ** 4
    wph = np.cumsum(230 * (1 - 0.2 * loadc) * (1 + 0.1 * tc / cl)) / H.SR
    whine = bp(sum(np.sin(2 * np.pi * k * wph + H.RNG.uniform(0, 6)) / k ** 1.2 for k in range(1, 14)), 150, 4000)
    churn = lp(white(cl), 450) * (0.3 + 0.7 * loadc)
    chuff = bp(white(cl), 300, 1500) * (1 - loadc) ** 8
    crank = (u(whine) * 0.55 + u(churn) + u(chuff) * 0.35) * np.clip(tc / 0.03, 0, 1) * np.clip((cl - tc) / 0.05, 0, 1)
    place(x, crank * 0.55, cs)
    catch = cs + cl - 0.02  # it catches, revs and settles into a lumpy idle
    rpm = np.interp(tt, [0, catch, catch + 0.08, catch + 0.2, catch + 0.5, catch + 1.1, d], [0, 0, 350, 900, 1750, 850, 780])
    rpm *= 1 + 0.03 * (S.smooth_noise(d, 3) - 0.5)
    load = np.interp(tt, [0, catch, catch + 0.1, catch + 0.4, catch + 0.6, catch + 1.2, d], [0, 0, 0.7, 1.0, 0.45, 0.35, 0.32])
    x += u(engine(d, rpm, load, rough=0.18, misfire=0.05)) * 0.9
    x += u(bp(white(d), 350, 1800) * np.exp(-((tt - catch - 0.45) / 0.15) ** 2) * lp(np.abs(white(d)), 60)) * 0.08
    x = reverb(lp(x, 7000), STREET, wet=0.22)
    emit('SFX/S_Van_Engine_Start_01.wav', one_shot(x, -18, 0.002, 0.35), 'SFX', radius=3000,
         caption="[The van's engine starts]")


def van_drive_away():
    seed(4820)
    d = 7.4
    tt = H.t(d)
    rpm = np.interp(tt, [0, 0.3, 1.9, 2.05, 2.35, 4.4, d], [800, 950, 2900, 2500, 1650, 2600, 2450])
    rpm *= 1 + 0.015 * (S.smooth_noise(d, 4) - 0.5)
    load = np.interp(tt, [0, 0.3, 0.5, 1.9, 2.0, 2.35, 2.6, 4.4, d], [0.35, 0.45, 1.0, 1.0, 0.12, 0.2, 0.9, 0.8, 0.6])
    speed = np.interp(tt, [0, 0.4, 2.0, 2.4, 4.4, d], [0, 0.3, 6.0, 6.3, 11.5, 13.5])
    sv = speed / 13.5
    tyre = bp(pink(d), 220, 5500) * sv
    spray = hp(white(d), 2400) * sv ** 1.5 * (0.6 + 0.4 * pk(lp(np.abs(white(d)), 40)))
    rumble = lp(brown(d), 110) * sv
    clunk = np.zeros(N(d))
    place(clunk, modal([165, 330, 880], [0.04, 0.03, 0.012], [1, 0.6, 0.3], 0.2), 2.03)  # into second gear
    src = u(engine(d, rpm, load, rough=0.05)) + u(tyre) * 0.45 + u(spray) * 0.3 + u(rumble) * 0.35 + clunk * 0.6
    dist = 4.0 + np.cumsum(speed) / H.SR  # pulling away from the listener
    delay = (dist - dist[0]) / 343.0
    y = np.interp((tt - delay) * H.SR, np.arange(N(d)), src)
    near = 4.0 / dist
    cut = 1200 + 9000 * near ** 0.8
    y = H.tv_lowpass(H.tv_lowpass(y, cut), cut * 1.3) * near ** 0.7
    pos = np.clip((dist - 4.0) / 40.0, 0, 1) * 0.5
    st = np.stack([y * np.cos((pos + 1) * np.pi / 4), y * np.sin((pos + 1) * np.pi / 4)])
    st = st + reverb(st, OUTDOOR_ST, wet=1.0, dry=0.0) * (0.15 + 0.35 * (1 - near))
    st *= np.clip((d - tt) / 3.0, 0, 1) ** 1.5  # the screen fades out
    emit('SFX/S_Van_Drive_Away_01.wav', one_shot(st, -19, 0.01, 0.2), 'SFX', spatial=False, radius=0)


def car_door_close():
    seed(4800)
    d = 0.8
    x = np.zeros(N(d))
    tb = H.t(0.5)
    thump = np.sin(2 * np.pi * np.cumsum(55 + 40 * np.exp(-tb / 0.015)) / H.SR) * np.exp(-tb / 0.07) * 0.9
    body = modal([92, 148, 236, 372, 590], [0.08, 0.06, 0.045, 0.03, 0.02], [1, 0.8, 0.55, 0.35, 0.2], 0.5)
    whump = lp(white(0.5), 1100) * env_exp(0.5, 0.012) * 0.6
    place(x, thump + body + whump, 0.01)
    for dt, g in ((0.003, 0.6), (0.014, 0.45)):  # two-stage latch
        place(x, modal(np.array([1800, 3000, 4400]) * H.RNG.uniform(0.95, 1.05), [0.012, 0.008, 0.005], [1, 0.6, 0.3], 0.06) * g, 0.01 + dt)
    place(x, bp(white(0.12), 600, 2200) * env_exp(0.12, 0.03) * 0.12, 0.02)
    place(x, modal([2100, 3650], [0.05, 0.03], [1, 0.5], 0.2) * 0.05, 0.012)
    x = reverb(x, STREET, wet=0.25)
    emit('SFX/S_Car_Door_Close_01.wav', one_shot(x, -17.5, 0.0005, 0.15), 'SFX', radius=1500)


# =======================================================================================
# UI
# =======================================================================================

def ui_sounds():
    seed(4900)  # soft hotbar click
    x = np.zeros(N(0.15))
    place(x, modal([1850, 3150], [0.0045, 0.003], [1, 0.4], 0.04), 0.0)
    place(x, modal([410], [0.012], [0.55], 0.06), 0.0)
    place(x, lp(white(0.02), 3500) * env_exp(0.02, 0.0015) * 0.25, 0.0)
    place(x, modal([1500, 2600], [0.003, 0.002], [0.35, 0.2], 0.03), 0.018)
    x = reverb(lp(x, 6000), SMALL, wet=0.1)
    emit('UI/S_Inventory_Select.wav', one_shot(x, -26, 0.0005, 0.03), 'UI', spatial=False, radius=0)

    seed(4901)  # subdued confirmation: an open fifth of soft FM bells over a low D

    def soft_bell(f, dur, vel):
        tt = H.t(dur)
        mod = np.sin(2 * np.pi * f * 2.0 * tt) * 1.1 * np.exp(-tt / 0.18)
        y = np.sin(2 * np.pi * f * tt + mod) + 0.25 * np.sin(2 * np.pi * f * 3.01 * tt) * np.exp(-tt / 0.12)
        mallet = lp(white(0.01), 2000) * env_exp(0.01, 0.002) * 0.15
        y[:len(mallet)] += mallet
        return y * np.exp(-tt / (dur * 0.35)) * np.clip(tt / 0.003, 0, 1) * vel

    x = np.zeros(N(1.3))
    place(x, soft_bell(note_hz('D4'), 1.2, 0.8), 0.0)
    place(x, soft_bell(note_hz('D3'), 1.25, 0.45), 0.0)
    place(x, soft_bell(note_hz('A4'), 1.1, 0.65), 0.13)
    x = reverb(lp(x, 4500), make_ir(1.4, 0.9, predelay=0.01, damping=6000, stereo=False), wet=0.35)
    emit('UI/S_Objective_Complete.wav', one_shot(x, -24, 0.001, 0.25), 'UI', spatial=False, radius=0)


# =======================================================================================
# Footsteps (outdoor surfaces), same length / level / variation scheme as gen_sfx
# =======================================================================================

def footsteps_outdoor():
    dur = 0.45
    tt = H.t(dur)
    for k in range(4):
        seed(4600 + k)
        thump = lp(white(dur), 420) * env_exp(dur, 0.028) + sine(H.RNG.uniform(70, 90), dur) * env_exp(dur, 0.03) * 0.45
        toe_at = H.RNG.uniform(0.11, 0.15)
        heel = bell_env(dur, 0.025, 0.025)
        toe = bell_env(dur, toe_at, 0.04) * H.RNG.uniform(0.5, 0.8)
        crunch = bp(poisson(dur, 4000 * (heel + toe)), 1400, 8500)  # blades snapping
        swish = bp(white(dur), 900, 5000) * (heel * 0.6 + toe)
        x = u(thump) + u(crunch) * 0.45 + u(swish) * 0.3
        if k % 2:  # a few dry leaves underfoot
            x += u(bp(poisson(dur, 1500 * (heel + toe)), 700, 4000)) * 0.3
        x = reverb(x, NEAR, wet=0.1)
        emit(f'Footsteps/S_Footstep_Grass_{k + 1:02d}.wav', fade(finish(x, -22.5), 0.0005, 0.08), 'SFX', radius=1200)
    for k in range(4):
        seed(4610 + k)
        thump = lp(white(dur), 650) * env_exp(dur, 0.02) + sine(H.RNG.uniform(80, 100), dur) * env_exp(dur, 0.025) * 0.5
        stones = np.zeros(N(dur))
        for _ in range(int(H.RNG.integers(45, 70))):
            at = 0.003 + H.RNG.exponential(0.025) if H.RNG.random() < 0.6 else H.RNG.uniform(0.09, 0.12) + H.RNG.exponential(0.04)
            f = H.RNG.uniform(1500, 6500)
            dec = H.RNG.uniform(0.001, 0.004)
            amp = H.RNG.exponential(1.0) * np.exp(-at / 0.15)
            place(stones, modal([f, f * H.RNG.uniform(1.4, 2.3)], [dec, dec * 0.6], [1, 0.4], 0.02) * amp, at)
        grind = bp(white(dur), 600, 4500) * (bell_env(dur, 0.03, 0.03) + 0.8 * bell_env(dur, 0.14, 0.05)) * lp(np.abs(white(dur)), 300)
        x = u(thump) * 0.8 + u(stones) * 0.9 + u(grind) * 0.35
        x = reverb(x, NEAR, wet=0.1)
        emit(f'Footsteps/S_Footstep_Gravel_{k + 1:02d}.wav', fade(finish(x, -20.5), 0.0005, 0.08), 'SFX', radius=1400)
    del tt


# =======================================================================================
# Mission stings (music, 44.1 kHz stereo, in the style of M_Departure_Sting)
# =======================================================================================

def bent_cluster(freqs, dur, bend_cents, attack, release):
    """M.string_cluster with all voices sagging by bend_cents over the note."""
    x = M.tt(dur)
    bend = 2 ** (bend_cents * (x / dur) ** 1.5 / 1200)
    stereo = np.zeros((2, len(x)))
    for i, f in enumerate(freqs):
        vib = (1 + 0.004 * np.sin(2 * np.pi * (5.0 + 0.4 * i) * x + i)) * bend
        v = np.zeros_like(x)
        for k in range(1, 18):
            if f * k > M.SR * 0.45:
                break
            v += np.sin(2 * np.pi * np.cumsum(f * k * vib) / M.SR) / k
        v = lp(v + bp(white(dur), f, min(f * 6, 9000)) * 0.05, 2400)
        stereo += pan(v, -0.7 + 1.4 * (i / max(len(freqs) - 1, 1)))
    return stereo * env_adsr(dur, attack, 0.3, 0.9, release) / max(len(freqs), 1)


def mission_stings():
    with music_rate():
        seed(4501)  # complete: Bbmaj7 -> D(add9), the F natural lifting to F# -- relief with an ache
        total = 6.6
        mix = np.zeros((2, N(total)))
        hall = make_ir(duration=5.0, decay=3.5, predelay=0.03, damping=4500, stereo=True)
        place(mix, M.string_cluster([note_hz(nm) for nm in ('A#2', 'F3', 'A3', 'D4')], 3.2, attack=1.0, release=1.4) * 0.8, 0.0)
        place(mix, M.string_cluster([note_hz(nm) for nm in ('D2', 'A2', 'F#3', 'A3', 'E4')], 4.2, attack=1.2, release=2.6) * 0.9, 2.3)
        sub = M.sub_drone(note_hz('D1'), total) * env_adsr(total, 1.5, 0.5, 0.8, 2.5) * 0.18
        mix += np.stack([sub, sub])
        for at, nm, vel, p in ((0.25, 'A4', 0.45, 0.15), (1.1, 'F4', 0.38, -0.1), (1.9, 'E4', 0.36, 0.1),
                               (2.55, 'F#4', 0.5, -0.05), (3.3, 'A4', 0.36, 0.2), (3.35, 'D5', 0.3, -0.2)):
            place(mix, pan(M.felt_piano(note_hz(nm), 3.2, vel=vel), p), at)
        mix = fade(reverb(mix, hall, wet=0.45), 0.05, 2.2)
        emit('Music/M_Mission_Complete_Sting.wav', finish(mix, -18.5), 'Music', spatial=False, radius=0)

        seed(4502)  # failed: low hit, a dissonant cluster that sinks and never resolves
        mix = np.zeros((2, N(total)))
        hall = make_ir(duration=5.0, decay=3.8, predelay=0.03, damping=4000, stereo=True, brightness=0.35)
        place(mix, np.stack([M.boom(4.0), M.boom(4.0)]) * 0.9, 0.0)
        place(mix, bent_cluster([note_hz(nm) for nm in ('D2', 'D#2', 'A2', 'G#3')], 6.0, -70, attack=0.25, release=3.0) * 0.9, 0.0)
        place(mix, pan(M.felt_piano(note_hz('D2'), 5.0, vel=0.55), -0.1), 0.35)
        place(mix, pan(M.felt_piano(note_hz('G#4'), 4.5, vel=0.3), 0.25), 0.4)
        place(mix, pan(M.felt_piano(note_hz('D#5'), 4.0, vel=0.25), -0.25), 1.9)
        place(mix, pan(M.bowed_metal(note_hz('D#5'), 4.5), 0.6), 1.2, gain=0.05)
        high = M.string_cluster([note_hz('A4'), note_hz('A#4')], 3.5, attack=1.5, release=1.8)
        place(mix, high * 0.35, 2.4)
        mix = fade(reverb(mix, hall, wet=0.5), 0.005, 2.5)
        emit('Music/M_Mission_Failed_Sting.wav', finish(mix, -18.5), 'Music', spatial=False, radius=0)


# =======================================================================================
# Manifest merge & analysis
# =======================================================================================

def merge_manifest(path, entries):
    """Replace entries with the same 'file' in place, append new ones, keep everything else."""
    existing = []
    if os.path.exists(path):
        with open(path) as f:
            existing = json.load(f)
    fresh = {}
    for e in entries:
        fresh[e['file']] = e
    merged, seen = [], set()
    for e in existing:
        name = e['file']
        if name in seen:
            continue
        seen.add(name)
        merged.append(fresh.get(name, e))
    for name, e in fresh.items():
        if name not in seen:
            merged.append(e)
            seen.add(name)
    with open(path, 'w') as f:
        json.dump(merged, f, indent=1)
    return merged


def measure(path, loop):
    import soundfile as sf
    data, sr = sf.read(path, always_2d=True)
    x = data.T
    n = x.shape[1]
    mono = x.mean(axis=0)
    w = int(0.05 * sr)
    k = max(1, n // w)
    short = np.sqrt(np.mean(mono[:k * w].reshape(k, w) ** 2, axis=1)) if n >= w else np.array([np.sqrt(np.mean(mono ** 2))])
    r = dict(sr=sr, ch=x.shape[0], dur=n / sr,
             peak=20 * np.log10(np.max(np.abs(x)) + 1e-12),
             rms=20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12),
             short=20 * np.log10(np.max(short) + 1e-12),
             dc=float(np.max(np.abs(x.mean(axis=1)))),
             clip=int(np.sum(np.abs(x) >= 32767 / 32768)))
    if loop:
        d1 = np.abs(np.diff(x, axis=1))
        typ = np.percentile(d1, 99.5) + 1e-9
        jump = np.max(np.abs(x[:, 0] - x[:, -1]))
        d2 = np.abs(np.diff(x, n=2, axis=1))
        typ2 = np.percentile(d2, 99.5) + 1e-9
        curv = np.max(np.abs((x[:, 0] - x[:, -1]) - (x[:, -1] - x[:, -2])))
        m = int(0.1 * sr)
        tail_db = 20 * np.log10(np.sqrt(np.mean(x[:, -m:] ** 2)) + 1e-12)
        head_db = 20 * np.log10(np.sqrt(np.mean(x[:, :m] ** 2)) + 1e-12)
        r.update(junction=jump / typ, curvature=curv / typ2, level_step=tail_db - head_db, jump=jump,
                 seam_quiet=min(tail_db, head_db) < -45)
    else:
        r.update(edge=20 * np.log10(max(np.max(np.abs(x[:, 0])), np.max(np.abs(x[:, -1]))) + 1e-12))
    return r


def analyse(new_files, reference=True):
    with open(MANIFEST_PATH) as f:
        manifest = json.load(f)
    by_file = {e['file']: e for e in manifest}
    print()
    print(f"{'file':44s} {'cat':8s} {'kHz':>4s} ch {'sec':>6s} {'peak':>6s} {'rms':>6s} {'max50':>6s} {'dc':>7s} clip  check")
    rows = {}
    problems = []
    for name in new_files:
        e = by_file[name]
        r = measure(name, e['loop'])
        rows[name] = r
        if e['loop']:
            ok = r['junction'] < 1.5 and r['curvature'] < 1.5 and (r['seam_quiet'] or abs(r['level_step']) < 3.0)
            lvl = 'sparse seam' if r['seam_quiet'] else f"lvl {r['level_step']:+.1f}dB"
            chk = f"loop jump={r['jump']:.4f} ({r['junction']:.2f}x typ, slope {r['curvature']:.2f}x) {lvl}"
        else:
            ok = r['edge'] < -50
            chk = f"edges {r['edge']:.0f} dBFS"
        ok = ok and r['clip'] == 0 and r['dc'] < 0.002 and r['peak'] <= -0.9
        if not ok:
            problems.append(name)
        print(f"{os.path.basename(name):44s} {e['category']:8s} {r['sr'] / 1000:4.1f} {r['ch']:2d} {r['dur']:6.2f} {r['peak']:6.1f} "
              f"{r['rms']:6.1f} {r['short']:6.1f} {r['dc']:7.5f} {r['clip']:4d}  {chk}{'' if ok else '  <-- CHECK'}")
    print()
    print('Summary per category (dBFS)            new: peak range / rms range       existing: peak range / rms range')
    cats = sorted({by_file[f]['category'] for f in new_files})
    new_set = set(new_files)
    for cat in cats:
        sel = [rows[f] for f in new_files if by_file[f]['category'] == cat]
        line = f"  {cat:8s} n={len(sel):3d}  peak {min(r['peak'] for r in sel):6.1f}..{max(r['peak'] for r in sel):5.1f}  " \
               f"rms {min(r['rms'] for r in sel):6.1f}..{max(r['rms'] for r in sel):5.1f}"
        if reference:
            old = [measure(e['file'], e['loop']) for e in manifest if e['category'] == cat and e['file'] not in new_set
                   and os.path.exists(e['file'])]
            if old:
                line += f"   | old n={len(old):3d} peak {min(r['peak'] for r in old):6.1f}..{max(r['peak'] for r in old):5.1f}  " \
                        f"rms {min(r['rms'] for r in old):6.1f}..{max(r['rms'] for r in old):5.1f}"
        print(line)
    print()
    print('All checks passed.' if not problems else f'{len(problems)} file(s) need a look: {problems}')
    return problems


JOBS = [
    vent_hum, machine_hum, electric_buzz,
    town_wind, traffic_distant, birds_day, crickets_night, rain_light, dog_barks, car_pass_distant,
    siren_distant, wind_chimes, church_bell, owls,
    house_room_tone, fridge_hum, house_settle,
    haunt_door_slam, haunt_drag, haunt_knock_wall, haunt_footsteps_run, haunt_music_box, haunt_glass_fall,
    haunt_bang, haunt_breath, haunt_drone, haunt_stingers, heartbeat_loop,
    light_bulb_pop, light_buzz_on, tv_power_on,
    loot_pickup_small, loot_pickup_heavy, loot_drop, bag_zip, lockpick_loop, lockpick_success, crowbar_pry,
    door_locked_rattle, key_pickup, window_slide, glass_cut, drawers, cabinet_open,
    van_engine_start, van_drive_away, car_door_close,
    ui_sounds, footsteps_outdoor, mission_stings,
]


def main(argv):
    os.chdir(os.path.join(HERE, '..', '..', '..'))
    names = [j.__name__ for j in JOBS]
    if '--list' in argv:
        print('\n'.join(names))
        return 0
    if '--check' in argv:
        with open(MANIFEST_PATH) as f:
            manifest = json.load(f)
        pattern = [a for a in argv if not a.startswith('--')]
        files = [e['file'] for e in manifest if not pattern or any(p in e['file'] for p in pattern)]
        return 1 if analyse(files, reference=False) else 0
    only = {a for a in argv if not a.startswith('--')}
    unknown = only - set(names)
    if unknown:
        print('unknown job(s):', ', '.join(sorted(unknown)), '\nuse --list to see the job names')
        return 2
    for job in JOBS:
        if only and job.__name__ not in only:
            continue
        print('->', job.__name__, flush=True)
        job()
    merged = merge_manifest(MANIFEST_PATH, H.MANIFEST)
    print('done', len(H.MANIFEST), 'files;', len(merged), 'manifest entries')
    return 1 if analyse([e['file'] for e in H.MANIFEST]) else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
