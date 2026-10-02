"""Tiny synthesizer toolkit used by generate_audio.py. Everything is computed from scratch with numpy/scipy."""
import numpy as np
from scipy import signal

SR = 44100
rng = np.random.default_rng(7)


def secs(s):
    return int(round(s * SR))


# ------------------------------------------------------------------ oscillators
def _phase(f, n):
    f = np.broadcast_to(np.asarray(f, dtype=float), (n,))
    return np.cumsum(f) / SR  # cycles


def sine(f, n):
    return np.sin(2 * np.pi * _phase(f, n))


def square(f, n, duty=0.5):
    return np.where((_phase(f, n) % 1.0) < duty, 1.0, -1.0)


def saw(f, n):
    return 2.0 * (_phase(f, n) % 1.0) - 1.0


def tri(f, n):
    return 4.0 * np.abs((_phase(f, n) % 1.0) - 0.5) - 1.0


def noise(n):
    return rng.uniform(-1.0, 1.0, n)


def glide(f0, f1, n, curve=1.0):
    """Frequency sweep from f0 to f1 (curve > 1 moves fast first)."""
    u = np.linspace(0.0, 1.0, n) ** (1.0 / curve)
    return f0 + (f1 - f0) * u


# ------------------------------------------------------------------ filters
def _wn(fc):
    return float(np.clip(fc, 20.0, SR * 0.45)) / (SR / 2)


def lp(x, fc, order=2):
    b, a = signal.butter(order, _wn(fc), "low")
    return signal.lfilter(b, a, x)


def hp(x, fc, order=2):
    b, a = signal.butter(order, _wn(fc), "high")
    return signal.lfilter(b, a, x)


def bp(x, lo, hi, order=2):
    b, a = signal.butter(order, [_wn(lo), _wn(hi)], "band")
    return signal.lfilter(b, a, x)


def sweep_bp(x, lo0, hi0, lo1, hi1, chunks=48):
    """Band-pass whose band moves from (lo0,hi0) to (lo1,hi1) over the sound."""
    out = np.zeros_like(x)
    edges = np.linspace(0, len(x), chunks + 1).astype(int)
    zi = None
    for i in range(chunks):
        u = i / max(1, chunks - 1)
        lo = lo0 * (lo1 / lo0) ** u
        hi = hi0 * (hi1 / hi0) ** u
        b, a = signal.butter(2, [_wn(lo), _wn(hi)], "band")
        if zi is None:
            zi = signal.lfilter_zi(b, a) * 0.0
        seg, zi = signal.lfilter(b, a, x[edges[i]:edges[i + 1]], zi=zi)
        out[edges[i]:edges[i + 1]] = seg
    return out


# ------------------------------------------------------------------ envelopes
def decay(n, tau):
    return np.exp(-np.arange(n) / SR / tau)


def adsr(n, a, d, s, r):
    a, d, r = max(1, secs(a)), max(1, secs(d)), max(1, secs(r))
    sustain_n = max(0, n - a - d - r)
    env = np.concatenate([np.linspace(0, 1, a, endpoint=False), np.linspace(1, s, d, endpoint=False),
                          np.full(sustain_n, s), np.linspace(s, 0, r)])
    return np.pad(env, (0, max(0, n - len(env))))[:n]


def declick(x, a=0.002, r=0.006):
    x = x.copy()
    na, nr = min(len(x), secs(a)), min(len(x), secs(r))
    if na:
        x[:na] *= np.linspace(0, 1, na)
    if nr:
        x[-nr:] *= np.linspace(1, 0, nr)
    return x


def vibrato(f, n, rate=5.5, depth=0.008, delay=0.15):
    t = np.arange(n) / SR
    ramp = np.clip((t - delay) / 0.3, 0, 1)
    return f * (1 + depth * ramp * np.sin(2 * np.pi * rate * t))


# ------------------------------------------------------------------ misc
def db(x):
    return 10 ** (x / 20.0)


def peak_normalize(x, peak_db):
    p = np.max(np.abs(x))
    return x if p == 0 else x * (db(peak_db) / p)


def place(dst, src, start):
    """Add src into dst at 'start', wrapping around the end so loops stay seamless."""
    n = dst.shape[-1]
    start %= n
    first = min(len(src), n - start)
    dst[..., start:start + first] += src[:first]
    if first < len(src):
        rest = min(len(src) - first, n)
        dst[..., :rest] += src[first:first + rest]


def reverb(x, mix=0.25, size=1.0, damp=3500):
    """Schroeder reverb on a (2, N) array; processed on a 3x tiled copy so loop tails wrap."""
    n = x.shape[-1]
    out = np.zeros_like(x)
    tiled = np.concatenate([x, x, x], axis=-1)
    for ch in range(2):
        wet = np.zeros(tiled.shape[-1])
        for d in (0.0297, 0.0371, 0.0411, 0.0437):
            D = secs(d * size * (1.0 + 0.04 * ch))
            a = np.zeros(D + 1)
            a[0], a[D] = 1.0, -0.78
            wet += signal.lfilter([1.0], a, tiled[ch])
        for d in (0.005, 0.0017):
            D = secs(d)
            b = np.zeros(D + 1)
            a = np.zeros(D + 1)
            b[0], b[D] = -0.7, 1.0
            a[0], a[D] = 1.0, -0.7
            wet = signal.lfilter(b, a, wet)
        wet = lp(wet, damp, 1) * 0.25
        out[ch] = wet[n:2 * n]
    return x * (1 - mix * 0.5) + out * mix


def echo(x, delay_s, feedback=0.35, mix=0.3):
    """Feedback delay on a (2, N) array, wrapping for loops."""
    n = x.shape[-1]
    D = secs(delay_s)
    tiled = np.concatenate([x, x, x], axis=-1)
    a = np.zeros(D + 1)
    a[0], a[D] = 1.0, -feedback
    wet = np.stack([signal.lfilter([1.0], a, tiled[c]) for c in range(2)])[:, n:2 * n]
    return x + (wet - x) * mix


def write_wav(path, x, rate=SR):
    import wave
    x = np.atleast_2d(x)
    pcm = (np.clip(x, -1, 1) * 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(pcm.shape[0])
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.T.copy().tobytes())


def lp_var(x, fc, chunks=40):
    """Low-pass with a time-varying cutoff (fc is an array the same length as x)."""
    out = np.zeros_like(x)
    edges = np.linspace(0, len(x), chunks + 1).astype(int)
    zi = None
    for i in range(chunks):
        seg = x[edges[i]:edges[i + 1]]
        if len(seg) == 0:
            continue
        b, a = signal.butter(2, _wn(float(np.mean(fc[edges[i]:edges[i + 1]]))), "low")
        if zi is None:
            zi = signal.lfilter_zi(b, a) * 0.0
        out[edges[i]:edges[i + 1]], zi = signal.lfilter(b, a, seg, zi=zi)
    return out
