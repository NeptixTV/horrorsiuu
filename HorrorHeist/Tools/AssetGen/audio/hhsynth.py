"""
Small DSP toolkit used to synthesize every sound and music cue of the project.
Everything is generated from scratch (no samples), so all audio is original and free of
third-party licenses. Requires numpy + scipy (+ soundfile for writing).
"""
import json
import os
import numpy as np
from scipy import signal

SR = 48000
RNG = np.random.default_rng(1987)


def seed(value):
    global RNG
    RNG = np.random.default_rng(value)


def t(duration, sr=None):
    sr = SR if sr is None else sr
    return np.arange(int(duration * sr)) / sr


# ---------------------------------------------------------------------------------------
# Noise sources

def white(duration, sr=None):
    sr = SR if sr is None else sr
    return RNG.standard_normal(int(duration * sr))


def pink(duration, sr=None):
    sr = SR if sr is None else sr
    n = int(duration * sr)
    x = RNG.standard_normal(n)
    # Paul Kellet's refined filter
    b = [0.049922035, -0.095993537, 0.050612699, -0.004408786]
    a = [1, -2.494956002, 2.017265875, -0.522189400]
    y = signal.lfilter(b, a, x)
    return y / (np.max(np.abs(y)) + 1e-9)


def brown(duration, sr=None):
    sr = SR if sr is None else sr
    x = np.cumsum(RNG.standard_normal(int(duration * sr)))
    x = signal.sosfilt(signal.butter(1, 20, 'hp', fs=sr, output='sos'), x)
    return x / (np.max(np.abs(x)) + 1e-9)


# ---------------------------------------------------------------------------------------
# Filters

def lp(x, cutoff, order=2, sr=None):
    sr = SR if sr is None else sr
    cutoff = min(cutoff, sr * 0.45)
    return signal.sosfilt(signal.butter(order, cutoff, 'lp', fs=sr, output='sos'), x, axis=-1)


def hp(x, cutoff, order=2, sr=None):
    sr = SR if sr is None else sr
    return signal.sosfilt(signal.butter(order, cutoff, 'hp', fs=sr, output='sos'), x, axis=-1)


def bp(x, low, high, order=2, sr=None):
    sr = SR if sr is None else sr
    high = min(high, sr * 0.45)
    return signal.sosfilt(signal.butter(order, [low, high], 'bp', fs=sr, output='sos'), x, axis=-1)


def peak(x, freq, q=4.0, gain_db=6.0, sr=None):
    """RBJ peaking EQ."""
    sr = SR if sr is None else sr
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * freq / sr
    alpha = np.sin(w0) / (2 * q)
    b = [1 + alpha * A, -2 * np.cos(w0), 1 - alpha * A]
    a = [1 + alpha / A, -2 * np.cos(w0), 1 - alpha / A]
    return signal.lfilter(np.array(b) / a[0], np.array(a) / a[0], x)


def resonator(x, freq, q=30.0, sr=None):
    """Narrow band-pass resonance (2-pole)."""
    sr = SR if sr is None else sr
    w0 = 2 * np.pi * freq / sr
    alpha = np.sin(w0) / (2 * q)
    b = [alpha, 0, -alpha]
    a = [1 + alpha, -2 * np.cos(w0), 1 - alpha]
    return signal.lfilter(np.array(b) / a[0], np.array(a) / a[0], x)


def tv_lowpass(x, cutoffs, sr=None, block=256):
    """Time-varying one-pole low-pass (cutoff per sample, smoothed per block)."""
    sr = SR if sr is None else sr
    y = np.zeros_like(x)
    state = 0.0
    for start in range(0, len(x), block):
        c = float(np.mean(cutoffs[start:start + block]))
        k = 1 - np.exp(-2 * np.pi * max(c, 5.0) / sr)
        seg = x[start:start + block]
        out = signal.lfilter([k], [1, -(1 - k)], seg, zi=[state * (1 - k)])
        y[start:start + block] = out[0]
        state = out[0][-1] if len(out[0]) else state
    return y


# ---------------------------------------------------------------------------------------
# Envelopes & helpers

def env_exp(duration, decay, attack=0.002, sr=None):
    sr = SR if sr is None else sr
    tt = t(duration, sr)
    e = np.exp(-tt / max(decay, 1e-4))
    a = np.clip(tt / max(attack, 1e-5), 0, 1)
    return e * a


def env_adsr(duration, a, d, s, r, sr=None):
    sr = SR if sr is None else sr
    n = int(duration * sr)
    tt = np.arange(n) / sr
    e = np.where(tt < a, tt / max(a, 1e-5), 0.0)
    dmask = (tt >= a) & (tt < a + d)
    e[dmask] = 1 - (1 - s) * (tt[dmask] - a) / max(d, 1e-5)
    smask = (tt >= a + d) & (tt < duration - r)
    e[smask] = s
    rmask = tt >= duration - r
    e[rmask] = s * np.clip((duration - tt[rmask]) / max(r, 1e-5), 0, 1)
    return e


def fade(x, fade_in=0.005, fade_out=0.02, sr=None):
    sr = SR if sr is None else sr
    x = x.copy()
    n_in = int(fade_in * sr)
    n_out = int(fade_out * sr)
    if n_in > 0:
        x[..., :n_in] *= np.linspace(0, 1, n_in) ** 2
    if n_out > 0:
        x[..., -n_out:] *= np.linspace(1, 0, n_out) ** 2
    return x


def place(buffer, sound, start, gain=1.0, sr=None):
    """Mix sound into buffer at time start (seconds). Works for mono or stereo (2, n)."""
    sr = SR if sr is None else sr
    i = int(start * sr)
    if i >= buffer.shape[-1]:
        return buffer
    n = min(sound.shape[-1], buffer.shape[-1] - i)
    if buffer.ndim == 2 and sound.ndim == 1:
        buffer[:, i:i + n] += gain * sound[:n]
    else:
        buffer[..., i:i + n] += gain * sound[..., :n]
    return buffer


def pan(mono, position):
    """Equal-power pan, position -1 (left) .. 1 (right)."""
    angle = (position + 1) * np.pi / 4
    return np.stack([mono * np.cos(angle), mono * np.sin(angle)])


def detune_ratio(cents):
    return 2 ** (cents / 1200)


def note_hz(name):
    """'A4' -> 440.0, supports sharps (#) and flats (b)."""
    names = {'C': -9, 'D': -7, 'E': -5, 'F': -4, 'G': -2, 'A': 0, 'B': 2}
    letter = name[0]
    rest = name[1:]
    shift = 0
    while rest and rest[0] in '#b':
        shift += 1 if rest[0] == '#' else -1
        rest = rest[1:]
    octave = int(rest)
    semis = names[letter] + shift + (octave - 4) * 12
    return 440.0 * 2 ** (semis / 12)


def midi_hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


# ---------------------------------------------------------------------------------------
# Oscillators

def sine(freq, duration, phase=0.0, sr=None):
    sr = SR if sr is None else sr
    return np.sin(2 * np.pi * freq * t(duration, sr) + phase)


def osc_freq(freq_curve, sr=None, shape='sine'):
    """Oscillator with a per-sample frequency curve."""
    sr = SR if sr is None else sr
    phase = 2 * np.pi * np.cumsum(freq_curve) / sr
    if shape == 'sine':
        return np.sin(phase)
    if shape == 'saw':
        return 2 * ((phase / (2 * np.pi)) % 1.0) - 1
    if shape == 'tri':
        return 2 * np.abs(2 * ((phase / (2 * np.pi)) % 1.0) - 1) - 1
    if shape == 'square':
        return np.sign(np.sin(phase))
    raise ValueError(shape)


def saw_bl(freq, duration, sr=None, max_harm=None):
    """Band-limited saw by additive synthesis."""
    sr = SR if sr is None else sr
    tt = t(duration, sr)
    nyq = sr / 2
    count = int(nyq / freq) if max_harm is None else min(max_harm, int(nyq / freq))
    out = np.zeros_like(tt)
    for k in range(1, max(count, 1) + 1):
        out += np.sin(2 * np.pi * freq * k * tt) / k
    return out * (2 / np.pi)


def modal(freqs, decays, amps, duration, sr=None):
    """Sum of exponentially damped sinusoids (impacts, bells, metal)."""
    sr = SR if sr is None else sr
    tt = t(duration, sr)
    out = np.zeros_like(tt)
    for f, d, a in zip(freqs, decays, amps):
        if f < sr / 2:
            out += a * np.exp(-tt / d) * np.sin(2 * np.pi * f * tt + RNG.uniform(0, 2 * np.pi))
    return out


# ---------------------------------------------------------------------------------------
# Space

def make_ir(duration=2.5, decay=1.2, predelay=0.012, damping=6000, early=8, stereo=True, sr=None, brightness=0.6):
    """Algorithmic room impulse response: early reflections + filtered exponential tail."""
    sr = SR if sr is None else sr
    n = int(duration * sr)
    tt = np.arange(n) / sr
    chans = []
    for c in range(2 if stereo else 1):
        tail = RNG.standard_normal(n) * np.exp(-tt * 6.9 / decay)
        # Darker as it decays: blend between bright and damped versions.
        damp = lp(tail, damping * 0.35)
        bright = lp(tail, damping)
        mix = np.exp(-tt * 3.0 / decay)
        tail = bright * mix * brightness + damp * (1 - mix * brightness)
        ir = np.zeros(n)
        start = int(predelay * sr)
        ir[start:] += tail[: n - start] * 0.6
        for _ in range(early):
            delay = RNG.uniform(0.004, 0.06)
            amp = RNG.uniform(0.2, 0.7) * np.exp(-delay * 20)
            ir[int(delay * sr)] += amp * RNG.choice([-1, 1])
        ir[0] += 0.0
        chans.append(ir / (np.sqrt(np.sum(ir ** 2)) + 1e-9))
    return np.stack(chans) if stereo else chans[0]


def reverb(x, ir, wet=0.3, dry=1.0):
    """Convolve mono/stereo signal with mono/stereo IR."""
    if x.ndim == 1 and ir.ndim == 2:
        x = np.stack([x, x])
    if x.ndim == 2 and ir.ndim == 1:
        ir = np.stack([ir, ir])
    if x.ndim == 1:
        w = signal.fftconvolve(x, ir)[: len(x)]
    else:
        w = np.stack([signal.fftconvolve(x[c], ir[c])[: x.shape[1]] for c in range(2)])
    return dry * x + wet * w


def reverb_tail(x, ir, wet=0.3, dry=1.0):
    """Like reverb() but keeps the full tail (output is longer)."""
    if x.ndim == 1 and ir.ndim == 2:
        x = np.stack([x, x])
    if x.ndim == 2 and ir.ndim == 1:
        ir = np.stack([ir, ir])
    if x.ndim == 1:
        w = signal.fftconvolve(x, ir)
        out = w * wet
        out[: len(x)] += dry * x
        return out
    w = np.stack([signal.fftconvolve(x[c], ir[c]) for c in range(2)])
    out = w * wet
    out[:, : x.shape[1]] += dry * x
    return out


def muffle_through_floor(x, sr=None):
    """What a sound sounds like through a wooden floor / ceiling."""
    sr = SR if sr is None else sr
    y = lp(x, 380, order=4)
    y = peak(y, 120, q=1.2, gain_db=5)
    return y


def radio(x, sr=None, band=(320, 3200), drive=2.2, wow=0.0025, flutter=0.0009, hiss=0.015, crackle=True):
    """AM-radio style processing: band-limit, saturation, wow/flutter, hiss and crackle."""
    sr = SR if sr is None else sr
    n = len(x)
    tt = np.arange(n) / sr
    # Wow & flutter via resampled time base.
    warp = tt + wow * np.sin(2 * np.pi * 0.55 * tt) / (2 * np.pi * 0.55) + flutter * np.sin(2 * np.pi * 6.2 * tt) / (2 * np.pi * 6.2)
    x = np.interp(warp * sr, np.arange(n), x)
    y = bp(x, band[0], band[1], order=3)
    y = np.tanh(y * drive) / np.tanh(drive)
    y = peak(y, 1400, q=0.8, gain_db=4)
    y += hiss * bp(white(n / sr), 1500, 6000)
    if crackle:
        clicks = np.zeros(n)
        count = int(n / sr * 6)
        for _ in range(count):
            i = RNG.integers(0, n - 200)
            clicks[i:i + 60] += RNG.uniform(-1, 1) * np.exp(-np.arange(60) / 8)
        y += 0.15 * hp(clicks, 1500)
    # Slow signal fading, like a weak station.
    y *= 0.82 + 0.18 * np.sin(2 * np.pi * 0.07 * tt + 1.3) * np.sin(2 * np.pi * 0.023 * tt)
    return y


# ---------------------------------------------------------------------------------------
# Output

def normalize(x, peak_db=-1.0):
    m = np.max(np.abs(x)) + 1e-9
    return x / m * 10 ** (peak_db / 20)


def rms_db(x):
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)


def loudness(x, target_rms_db=-20.0, peak_db=-1.0):
    """Gain to a target RMS, then soft-limit peaks."""
    g = 10 ** ((target_rms_db - rms_db(x)) / 20)
    y = x * g
    ceiling = 10 ** (peak_db / 20)
    return np.tanh(y / ceiling) * ceiling


def dc_block(x):
    return hp(x, 18, order=1)


def make_loop(x, crossfade, sr=None):
    """Seamless loop: crossfade the tail into the head (equal power)."""
    sr = SR if sr is None else sr
    n = int(crossfade * sr)
    head = x[..., :n]
    tail = x[..., -n:]
    ramp = np.linspace(0, np.pi / 2, n)
    mixed = tail * np.cos(ramp) + head * np.sin(ramp)
    body = x[..., n:-n]
    return np.concatenate([mixed, body], axis=-1)


MANIFEST = []


def save(path, x, sr=None, category='SFX', loop=False, spatial=True, radius=1500.0, caption='', volume=1.0):
    """Write 16-bit WAV and record import metadata for the editor pipeline."""
    sr = SR if sr is None else sr
    os.makedirs(os.path.dirname(path), exist_ok=True)
    x = np.asarray(x, dtype=np.float64)
    if x.ndim == 2:
        x = x.T
    x = np.clip(x, -1, 1)
    import soundfile as sf
    sf.write(path, x, sr, subtype='PCM_16')
    MANIFEST.append({
        'file': path.replace('\\', '/'),
        'category': category,
        'loop': loop,
        'spatial': spatial,
        'radius': radius,
        'caption': caption,
        'volume': volume,
        'seconds': round(len(x) / sr, 3),
    })


def write_manifest(path):
    existing = []
    if os.path.exists(path):
        with open(path) as f:
            existing = json.load(f)
    by_file = {e['file']: e for e in existing}
    for e in MANIFEST:
        by_file[e['file']] = e
    with open(path, 'w') as f:
        json.dump(sorted(by_file.values(), key=lambda e: e['file']), f, indent=1)
