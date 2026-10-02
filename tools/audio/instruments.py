"""Synthesized instruments, drums and a small mixer for the music tracks. Everything returns mono numpy arrays."""
import numpy as np
from synth import SR, secs, sine, tri, noise, glide, lp, hp, bp, decay, adsr, declick, vibrato, place, _phase

TAU = 2 * np.pi


def _t(n):
    return np.arange(n) / SR


# ------------------------------------------------------------------ pitch helpers
NOTE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def mid(name):
    """'C#5' -> MIDI note number."""
    pc = NOTE[name[0]]
    i = 1
    while i < len(name) and name[i] in "#b":
        pc += 1 if name[i] == "#" else -1
        i += 1
    return 12 * (int(name[i:]) + 1) + pc


def hz(m):
    return 440.0 * 2 ** ((m - 69) / 12)


def chord(name, octave):
    """'Am' / 'F' / 'Bb' -> three MIDI notes (root position) with the root in the given octave."""
    minor = name.endswith("m")
    root = name[:-1] if minor else name
    r = mid(root + str(octave))
    return [r, r + (3 if minor else 4), r + 7]


def parse(text, bar_beats=4.0):
    """'E5:.5 G5:1 r:2 | ...' -> [(beat, duration, midi)]; checks that every bar adds up."""
    events, t = [], 0.0
    for bar_no, bar in enumerate(text.split("|"), 1):
        start = t
        for tok in bar.split():
            n, d = tok.split(":")
            d = float(d)
            if n != "r":
                events.append((t, d, mid(n)))
            t += d
        assert abs(t - start - bar_beats) < 1e-6, f"bar {bar_no} has {t - start} beats: {bar}"
    return events


# ------------------------------------------------------------------ band-limited waves (no aliasing grit on leads)
def _blsaw(ph, K):
    out = np.zeros_like(ph)
    for k in range(1, K + 1):
        out += np.sin(k * ph) / k
    return out * (2 / np.pi)


def blsaw(f, n, maxhz=9000.0):
    f = np.broadcast_to(np.asarray(f, dtype=float), (n,))
    return _blsaw(TAU * _phase(f, n), int(np.clip(maxhz / np.mean(f), 1, 48)))


def blpulse(f, n, duty=0.3, maxhz=9000.0):
    f = np.broadcast_to(np.asarray(f, dtype=float), (n,))
    ph = TAU * _phase(f, n)
    K = int(np.clip(maxhz / np.mean(f), 1, 48))
    return (_blsaw(ph, K) - _blsaw(ph - TAU * duty, K)) * 0.5


# ------------------------------------------------------------------ pitched instruments: (freq, seconds, velocity) -> mono
def pluck(f, dur, vel=1.0):
    n = secs(dur + 0.3)
    t = _t(n)
    base = float(np.clip(dur * 1.3, 0.12, 0.5))
    out = np.zeros(n)
    for k in range(1, 11):
        if f * k > 14000:
            break
        out += (1.0 / k ** 1.15) * np.sin(TAU * f * k * t) * np.exp(-t / (base / k ** 0.6))
    return declick(out * 0.9 * vel, 0.001, 0.02)


def marimba(f, dur, vel=1.0):
    """Woody xylophone-ish tone with a little tick at the start."""
    n = secs(max(dur, 0.12) + 0.25)
    t = _t(n)
    out = np.zeros(n)
    for ratio, amp, tau in ((1.0, 1.0, 0.30), (3.92, 0.40, 0.08), (9.2, 0.12, 0.035)):
        if f * ratio < 15000:
            out += amp * np.sin(TAU * f * ratio * t) * np.exp(-t / tau)
    out += 0.12 * bp(noise(n), 2500, 5000) * decay(n, 0.006)
    return declick(out * 0.85 * vel, 0.0008, 0.02)


def celeste(f, dur, vel=1.0):
    """Glassy music-box bell."""
    n = secs(max(dur, 0.2) + 0.9)
    t = _t(n)
    out = np.zeros(n)
    for ratio, amp, tau in ((1.0, 1.0, 0.9), (2.01, 0.45, 0.5), (3.0, 0.28, 0.3), (4.2, 0.16, 0.2), (5.4, 0.08, 0.12)):
        if f * ratio < 15000:
            out += amp * np.sin(TAU * f * ratio * t) * np.exp(-t / tau)
    return declick(out * 0.6 * vel, 0.001, 0.05)


def bass(f, dur, vel=1.0):
    """Round plucked bass."""
    n = secs(dur + 0.1)
    x = 0.85 * sine(f, n) + 0.3 * tri(f, n) + 0.12 * sine(f * 2, n)
    return declick(lp(x, 900, 2) * adsr(n, 0.004, min(0.16, dur), 0.5, 0.07) * vel, 0.002, 0.02)


def sawbass(f, dur, vel=1.0):
    """Buzzy bass for the boss fight."""
    n = secs(dur + 0.06)
    x = lp(blsaw(f, n, 3000), 1100, 2) * 0.75 + 0.7 * sine(f, n)
    return declick(x * adsr(n, 0.004, 0.05, 0.85, 0.05) * vel, 0.002, 0.012)


def pad(f, dur, vel=1.0, bright=1400):
    n = secs(dur + 1.0)
    out = sum(blsaw(f * d, n, 4000) for d in (0.996, 1.0, 1.004)) / 3
    return lp(out, bright, 2) * adsr(n, 0.6, 0.4, 0.85, 1.0) * 0.8 * vel


def reed(f, dur, vel=1.0):
    """Nasal double-reed (think mizmar / zurna) with vibrato and a breath of noise."""
    n = secs(dur + 0.12)
    fv = vibrato(f, n, 5.3, 0.007, 0.12)
    x = 0.6 * blsaw(fv, n, 7000) + 0.55 * blpulse(fv, n, 0.28, 7000)
    x = bp(x, 380, 2900, 2)
    x += 0.05 * bp(noise(n), 1800, 5200)
    return declick(x * adsr(n, 0.03, 0.08, 0.85, 0.1) * 1.3 * vel, 0.003, 0.02)


def oud(f, dur, vel=1.0):
    """Plucked lute: bright attack, quick decay."""
    n = secs(dur + 0.25)
    t = _t(n)
    tau = float(np.clip(dur * 1.1, 0.1, 0.32))
    out = np.zeros(n)
    for k in range(1, 12):
        if f * k > 12000:
            break
        out += (1.0 / k ** 0.8) * np.sin(TAU * f * k * t * (1 + 0.0004 * k * k)) * np.exp(-t / (tau / k ** 0.45))
    out += 0.1 * bp(noise(n), 1500, 4000) * decay(n, 0.01)
    return declick(out * 0.7 * vel, 0.001, 0.02)


def brass(f, dur, vel=1.0):
    n = secs(dur + 0.08)
    x = (blsaw(f * 0.998, n, 6000) + blsaw(f * 1.002, n, 6000)) * 0.5
    swell = np.clip(_t(n) / 0.09, 0, 1) ** 2  # the tone brightens as the note "blows" in
    out = lp(x, 900, 2) * (1 - swell) + lp(x, 3200, 2) * swell
    return declick(out * adsr(n, 0.025, 0.1, 0.85, 0.07) * 0.9 * vel, 0.002, 0.015)


def harpsichord(f, dur, vel=1.0):
    n = secs(dur + 0.2)
    t = _t(n)
    out = np.zeros(n)
    tau = float(np.clip(dur * 1.1, 0.08, 0.3))
    for k in range(1, 14):
        if f * k > 14000:
            break
        out += (1.0 / k ** 0.6) * np.sin(TAU * f * k * t) * np.exp(-t / (tau / k ** 0.35))
    out += 0.15 * hp(noise(n), 3000) * decay(n, 0.004)
    return declick(out * 0.45 * vel, 0.0008, 0.02)


# ------------------------------------------------------------------ drums: (velocity) -> mono
def kick(vel=1.0):
    n = secs(0.32)
    body = sine(glide(135, 46, n, 6.0), n) * decay(n, 0.11)
    click = bp(noise(n), 1500, 5000) * decay(n, 0.004) * 0.25
    return declick((body + click) * vel, 0.0005, 0.01)


def softkick(vel=1.0):
    n = secs(0.25)
    return declick(sine(glide(95, 50, n, 5.0), n) * decay(n, 0.08) * vel * 0.9, 0.001, 0.01)


def snare(vel=1.0):
    n = secs(0.25)
    body = sine(glide(210, 170, n), n) * decay(n, 0.05) * 0.5
    hiss = bp(noise(n), 1800, 8000) * decay(n, 0.09)
    return declick((body + hiss * 0.85) * vel, 0.0005, 0.02)


def hat(vel=1.0, open_=False):
    n = secs(0.25 if open_ else 0.07)
    return declick(hp(noise(n), 7500, 2) * decay(n, 0.1 if open_ else 0.02) * vel * 0.6, 0.0005, 0.01)


def rim(vel=1.0):
    n = secs(0.06)
    x = sine(1750, n) * decay(n, 0.006) + 0.5 * bp(noise(n), 2500, 6500) * decay(n, 0.008)
    return declick(x * vel * 0.8, 0.0003, 0.005)


def wood(vel=1.0, high=False):
    f = 1250 if high else 880
    n = secs(0.1)
    x = sine(f, n) * decay(n, 0.022) + 0.4 * sine(f * 2.45, n) * decay(n, 0.01)
    return declick(x * vel * 0.8, 0.0003, 0.01)


def shaker(vel=1.0):
    n = secs(0.09)
    env = np.minimum(_t(n) / 0.012, 1.0) * decay(n, 0.03)
    return declick(bp(noise(n), 4500, 10000) * env * vel * 0.7, 0.002, 0.01)


def dum(vel=1.0):
    """Doumbek bass stroke."""
    n = secs(0.3)
    x = sine(glide(190, 105, n, 3.0), n) * decay(n, 0.11) + 0.2 * lp(noise(n), 500) * decay(n, 0.02)
    return declick(x * vel, 0.001, 0.02)


def tek(vel=1.0):
    """Doumbek edge stroke."""
    n = secs(0.12)
    x = bp(noise(n), 2500, 7000) * decay(n, 0.03) + 0.35 * sine(850, n) * decay(n, 0.02)
    return declick(x * vel * 0.9, 0.0005, 0.01)


def ka(vel=1.0):
    n = secs(0.08)
    x = bp(noise(n), 3500, 8500) * decay(n, 0.015) + 0.2 * sine(1100, n) * decay(n, 0.012)
    return declick(x * vel * 0.7, 0.0004, 0.008)


def zill(vel=1.0):
    """Finger cymbals."""
    n = secs(0.7)
    t = _t(n)
    out = sum(a * np.sin(TAU * f * t) * np.exp(-t / tau) for f, a, tau in
              ((3150, 1.0, 0.35), (4480, 0.7, 0.28), (6270, 0.5, 0.22), (8410, 0.3, 0.15)))
    out += 0.3 * hp(noise(n), 6000) * decay(n, 0.01)
    return declick(out * vel * 0.3, 0.0005, 0.05)


def tom(f, vel=1.0):
    n = secs(0.35)
    x = sine(glide(f * 1.5, f, n, 5.0), n) * decay(n, 0.13) + 0.2 * bp(noise(n), 800, 3000) * decay(n, 0.01)
    return declick(x * vel, 0.001, 0.02)


def crash(vel=1.0):
    n = secs(1.6)
    return declick(hp(noise(n), 4500, 2) * decay(n, 0.5) * vel * 0.5, 0.001, 0.2)


# ------------------------------------------------------------------ effects (block-recursive, so long delays stay fast)
def _recur(x, D, g):
    """y[n] = x[n] + g * y[n - D]"""
    y = x.copy()
    for s in range(D, len(x), D):
        e = min(s + D, len(x))
        y[s:e] += g * y[s - D:e - D]
    return y


def reverb_wet(x, size=1.0, damp=3500):
    """Wet-only Schroeder reverb of a (2, N) bus, computed on a 3x tiled copy so the loop tail wraps around."""
    n = x.shape[-1]
    tiled = np.concatenate([x, x, x], axis=-1)
    out = np.zeros_like(x)
    for ch in range(2):
        wet = np.zeros(tiled.shape[-1])
        for d in (0.0297, 0.0371, 0.0411, 0.0437):
            wet += _recur(tiled[ch], secs(d * size * (1.0 + 0.04 * ch)), 0.78)
        for d in (0.005, 0.0017):
            D = secs(d)
            t = -0.7 * wet
            t[D:] += wet[:-D]
            wet = _recur(t, D, 0.7)
        out[ch] = lp(wet, damp, 1)[n:2 * n] * 0.25
    return out


def echo_wet(x, delay_s, feedback):
    """Only the repeats (not the original) of a feedback delay on a (2, N) bus, wrapping for loops."""
    n = x.shape[-1]
    tiled = np.concatenate([x, x, x], axis=-1)
    D = secs(delay_s)
    return np.stack([_recur(tiled[c], D, feedback)[n:2 * n] - x[c] for c in range(2)])


# ------------------------------------------------------------------ mixer
class Mixer:
    """Places notes on a looping stereo timeline. Sends feed a shared reverb and echo; placement wraps for a seamless loop."""

    def __init__(self, bpm, bars):
        self.bpm = bpm
        self.spb = 60.0 / bpm
        self.n = secs(bars * 4 * self.spb)
        self.dry = np.zeros((2, self.n))
        self.verb = np.zeros((2, self.n))
        self.echo = np.zeros((2, self.n))
        self.rng = np.random.default_rng(3)

    def _put(self, x, beat, gain, pan, rev, ech):
        start = secs(beat * self.spb)
        l = np.cos((pan + 1) * np.pi / 4)
        r = np.sin((pan + 1) * np.pi / 4)
        for bus, amount in ((self.dry, 1.0), (self.verb, rev), (self.echo, ech)):
            if amount <= 0:
                continue
            place(bus[0], x * (gain * amount * l), start)
            place(bus[1], x * (gain * amount * r), start)

    def notes(self, inst, events, gain=1.0, pan=0.0, rev=0.0, ech=0.0, swing=0.0, vel_var=0.0, shift=0.0, **inst_kw):
        """events: (beat, duration_in_beats, midi_note[, velocity])."""
        for ev in events:
            beat, dur, m = ev[:3]
            vel = ev[3] if len(ev) > 3 else 1.0
            if swing and abs((beat % 1.0) - 0.5) < 1e-6:
                beat += swing
            vel *= 1.0 + vel_var * (self.rng.random() * 2 - 1)
            self._put(inst(hz(m), dur * self.spb, vel, **inst_kw), beat + shift, gain, pan, rev, ech)

    def hits(self, inst, beats, gain=1.0, pan=0.0, rev=0.0, ech=0.0, swing=0.0, **kw):
        """beats: (beat, velocity) pairs. Extra keyword arguments go to the drum function."""
        for beat, vel in beats:
            if swing and abs((beat % 1.0) - 0.5) < 1e-6:
                beat += swing
            self._put(inst(vel, **kw), beat, gain, pan, rev, ech)

    def render(self, rev_size=1.0, rev_damp=3500, echo_beats=0.75, echo_fb=0.3):
        return self.dry + reverb_wet(self.verb, rev_size, rev_damp) + echo_wet(self.echo, echo_beats * self.spb, echo_fb)


def loop_filter(fn, x):
    """Apply a filter on a 3x tiled copy and keep the middle, so the loop point stays seamless."""
    n = x.shape[-1]
    t = np.concatenate([x, x, x], axis=-1)
    return np.stack([fn(c) for c in t])[:, n:2 * n]


def master(mix, rms_db=-16.0, peak_db=-1.0, drive=1.2):
    """Remove rumble, glue with a soft clipper, then set the loudness (average level), never exceeding the peak ceiling."""
    x = loop_filter(lambda c: hp(c, 28, 1), mix)
    x = np.tanh(x * drive / max(np.max(np.abs(x)), 1e-9) * 1.4) / np.tanh(drive * 1.4)
    rms = np.sqrt(np.mean(x ** 2))
    scale = min(10 ** (rms_db / 20) / rms, 10 ** (peak_db / 20) / np.max(np.abs(x)))
    return x * scale
