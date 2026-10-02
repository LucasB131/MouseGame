"""Sound effect recipes. Each returns a mono float array; generate_audio.py normalizes and saves them."""
import numpy as np
from synth import *


def pad_sum(parts):
    n = max(len(p[1]) + p[0] for p in parts)
    out = np.zeros(n)
    for start, sig in parts:
        out[start:start + len(sig)] += sig
    return out


def add(*xs):
    n = max(len(x) for x in xs)
    return sum(np.pad(x, (0, n - len(x))) for x in xs)


def tone(f, dur, kind="square", tau=None, duty=0.5, gain=1.0):
    n = secs(dur)
    f = np.asarray(f, dtype=float)
    f = glide(f[0], f[1], n) if f.ndim and f.size == 2 else f
    wave_ = {"square": lambda: square(f, n, duty), "sine": lambda: sine(f, n), "tri": lambda: tri(f, n), "saw": lambda: saw(f, n)}[kind]()
    env = decay(n, tau if tau else dur / 3)
    return declick(wave_ * env * gain, 0.002, 0.01)


# ------------------------------------------------------------------ footsteps (tiny mouse feet)
def step(variant):
    n = secs(0.06)
    click = bp(noise(n), 1800 + 600 * variant, 5200) * decay(n, 0.012)
    body = sine(glide(520 - 60 * variant, 260, n), n) * decay(n, 0.008) * 0.5
    return declick(click + body, 0.0005, 0.004)


# ------------------------------------------------------------------ cheese and holes
def cheese():
    return pad_sum([(0, tone(880, 0.07, "square", 0.04, 0.25, 0.7)), (secs(0.06), tone(1318, 0.12, "square", 0.06, 0.25, 0.7))])


def golden_cheese():
    notes = [1046.5, 1318.5, 1568.0, 2093.0, 2637.0]
    parts = [(secs(0.07 * i), add(tone(f, 0.45, "sine", 0.18, gain=0.7), tone(f * 2, 0.3, "sine", 0.1, gain=0.25))) for i, f in enumerate(notes)]
    return pad_sum(parts)


def store():
    pop = tone(glide(380, 760, secs(0.08)), 0.08, "sine", 0.05)
    ching = add(tone(2093, 0.3, "sine", 0.1, gain=0.5), tone(3136, 0.3, "sine", 0.09, gain=0.35))
    return pad_sum([(0, pop), (secs(0.07), ching)])


def hole_enter():
    n = secs(0.28)
    s = sine(glide(700, 140, n, 2.0), n) * decay(n, 0.12)
    sw = bp(noise(n), 300, 2500) * np.linspace(0.6, 0.0, n) ** 2 * 0.5
    return declick(s + sw, 0.002, 0.03)


def hole_exit():
    n = secs(0.22)
    s = sine(glide(160, 760, n, 2.0), n) * adsr(n, 0.01, 0.05, 0.6, 0.12)
    pop = tone(900, 0.05, "sine", 0.02, gain=0.5)
    return pad_sum([(0, s), (n - secs(0.04), pop)])


def hole_teleport():
    n = secs(0.5)
    t = np.arange(n) / SR
    f = 380 + 260 * np.sin(2 * np.pi * 9 * t) + glide(0, 500, n)
    s = sine(f, n) * adsr(n, 0.02, 0.1, 0.7, 0.25) * (0.7 + 0.3 * np.sin(2 * np.pi * 18 * t))
    return declick(s, 0.003, 0.02)


def pepper():
    n = secs(0.5)
    crunch = bp(noise(n), 900, 4200) * (decay(n, 0.04) + 0.8 * np.roll(decay(n, 0.04), secs(0.09)))
    crunch[:secs(0.09)] = crunch[:secs(0.09)]
    rise = square(vibrato(glide(380, 1300, n), n, 12, 0.03, 0), n, 0.3) * adsr(n, 0.01, 0.1, 0.5, 0.2) * 0.35
    return declick(crunch * 0.8 + rise, 0.002, 0.03)


def exit_open():
    parts = []
    for i, f in enumerate((784.0, 987.8, 1174.7, 1568.0)):
        parts.append((secs(0.09 * i), add(tone(f, 0.9, "sine", 0.28, gain=0.6), tone(f * 2.76, 0.5, "sine", 0.1, gain=0.12))))
    return pad_sum(parts)


# ------------------------------------------------------------------ cats
def alert():
    a = tone(740, 0.09, "square", 0.08, 0.25, 0.8)
    b = tone(1109, 0.16, "square", 0.1, 0.25, 0.8)
    return pad_sum([(0, a), (secs(0.08), b)])


def windup():
    n = secs(0.42)
    t = np.arange(n) / SR
    f = glide(70, 125, n) * (1 + 0.05 * np.sin(2 * np.pi * 22 * t))
    g = saw(f, n) * (0.6 + 0.4 * np.sin(2 * np.pi * 26 * t))
    g = lp(g, 700) + 0.2 * bp(noise(n), 400, 1200)
    return declick(g * adsr(n, 0.06, 0.05, 0.85, 0.1), 0.004, 0.02)


def pounce():
    n = secs(0.3)
    w = sweep_bp(noise(n), 2500, 5000, 400, 900) * np.concatenate([np.linspace(0, 1, secs(0.06)), np.linspace(1, 0.1, n - secs(0.06))])
    thump = sine(glide(130, 50, secs(0.14)), secs(0.14)) * decay(secs(0.14), 0.06)
    return declick(pad_sum([(0, w * 0.8), (secs(0.12), thump * 0.8)]), 0.002, 0.03)


def stun():
    n = secs(0.14)
    bonk = sine(glide(240, 80, n), n) * decay(n, 0.05) + 0.4 * bp(noise(n), 800, 3000) * decay(n, 0.01)
    parts = [(0, bonk)]
    for i, f in enumerate((1318.5, 1108.7, 932.3, 783.99)):
        parts.append((secs(0.1 + 0.07 * i), tone(f, 0.28, "sine", 0.1, gain=0.35) * (0.7 + 0.3 * np.sin(2 * np.pi * 14 * np.arange(secs(0.28)) / SR))))
    return pad_sum(parts)


def boss_hit():
    n = secs(0.7)
    boom = sine(glide(110, 32, n, 3.0), n) * decay(n, 0.22)
    crash = lp(noise(n), 3200) * decay(n, 0.12) * 0.8
    crack = bp(noise(secs(0.05)), 1500, 6000) * decay(secs(0.05), 0.01)
    parts = [(0, boom + crash), (0, crack)]
    for i, f in enumerate((1046.5, 880.0, 698.5)):
        parts.append((secs(0.18 + 0.08 * i), tone(f, 0.3, "sine", 0.11, gain=0.3)))
    return pad_sum(parts)


def boss_ko():
    n = secs(1.3)
    t = np.arange(n) / SR
    fall = lp(saw(vibrato(glide(420, 55, n, 1.5), n, 6, 0.03, 0), n), 900) * adsr(n, 0.02, 0.1, 0.8, 0.5) * 0.6
    thud_n = secs(0.5)
    thud = sine(glide(90, 28, thud_n), thud_n) * decay(thud_n, 0.18)
    dust = lp(noise(thud_n), 1500) * decay(thud_n, 0.1) * 0.5
    parts = [(0, fall), (secs(0.85), thud + dust)]
    for i, f in enumerate((1318.5, 1046.5, 880.0, 659.3)):
        parts.append((secs(1.0 + 0.09 * i), tone(f, 0.5, "sine", 0.18, gain=0.3)))
    return pad_sum(parts)


# ------------------------------------------------------------------ results
def caught():
    squeak_n = secs(0.12)
    squeak = sine(glide(2200, 3600, squeak_n), squeak_n) * decay(squeak_n, 0.05) * 0.4
    parts = [(0, squeak)]
    start = secs(0.12)
    for f, d in ((466.2, 0.22), (415.3, 0.22), (370.0, 0.22), (277.2, 0.7)):
        n = secs(d)
        t = np.arange(n) / SR
        wah = lp_var(square(vibrato(f, n, 6, 0.012, 0.0), n, 0.4), 500 + 1100 * np.sin(np.pi * np.clip(t / d, 0, 1))) * adsr(n, 0.02, 0.05, 0.8, 0.1)
        parts.append((start, wah * 0.8))
        start += secs(d)
    return pad_sum(parts)


def win():
    parts = []
    for i, f in enumerate((523.25, 659.25, 783.99, 1046.5)):
        n = secs(0.16)
        parts.append((secs(0.13 * i), tone(f, 0.16, "square", 0.1, 0.25, 0.5) + tone(f, 0.16, "tri", 0.1, gain=0.5)))
    chord_start = secs(0.55)
    for f in (523.25, 659.25, 783.99, 1046.5):
        n = secs(1.1)
        parts.append((chord_start, (square(f, n, 0.25) * 0.25 + tri(f, n) * 0.4) * adsr(n, 0.01, 0.1, 0.6, 0.5)))
    return pad_sum(parts)


def intro():
    parts = []
    for i, f in enumerate((392.0, 523.25, 659.25, 783.99, 1046.5)):
        parts.append((secs(0.1 * i), add(tone(f, 0.35, "tri", 0.15), 0.5 * tone(f * 2, 0.25, "sine", 0.08))))
    return pad_sum(parts)


# ------------------------------------------------------------------ interface
def ui_move():
    return tone(1100, 0.03, "square", 0.012, 0.25, 0.5)


def ui_select():
    return pad_sum([(0, tone(660, 0.06, "square", 0.04, 0.25, 0.7)), (secs(0.05), tone(990, 0.1, "square", 0.05, 0.25, 0.7))])


def ui_back():
    return pad_sum([(0, tone(700, 0.06, "square", 0.04, 0.25, 0.7)), (secs(0.05), tone(470, 0.1, "square", 0.05, 0.25, 0.7))])


def ui_locked():
    n = secs(0.2)
    t = np.arange(n) / SR
    return declick(lp(saw(110, n), 800) * (0.6 + 0.4 * np.sin(2 * np.pi * 30 * t)) * adsr(n, 0.005, 0.03, 0.8, 0.05), 0.002, 0.01)


def shop_buy():
    kaching = bp(noise(secs(0.04)), 2500, 7000) * decay(secs(0.04), 0.012)
    parts = [(0, kaching)]
    for i, f in enumerate((2093.0, 2637.0, 3136.0, 4186.0)):
        parts.append((secs(0.05 + 0.055 * i), tone(f, 0.4, "sine", 0.12, gain=0.5)))
    return pad_sum(parts)


def shop_equip():
    n = secs(0.18)
    w = sweep_bp(noise(n), 800, 2000, 2500, 6000) * np.hanning(n) * 0.6
    click = tone(1500, 0.03, "square", 0.01, 0.25, 0.5)
    return pad_sum([(0, w), (secs(0.12), click)])


def shop_fail():
    return pad_sum([(0, tone(300, 0.1, "square", 0.08, 0.5, 0.6)), (secs(0.1), tone(210, 0.18, "square", 0.1, 0.5, 0.6))])


# name -> (function, args, target peak in dBFS). Peaks set the relative loudness between sounds.
SFX = {
    "step1": (step, (0,), -26), "step2": (step, (1,), -26),
    "cheese": (cheese, (), -9), "golden_cheese": (golden_cheese, (), -7), "store": (store, (), -8),
    "hole_enter": (hole_enter, (), -9), "hole_exit": (hole_exit, (), -9), "hole_teleport": (hole_teleport, (), -9),
    "pepper": (pepper, (), -7), "exit_open": (exit_open, (), -8),
    "alert": (alert, (), -8), "windup": (windup, (), -6), "pounce": (pounce, (), -5), "stun": (stun, (), -8),
    "boss_hit": (boss_hit, (), -3), "boss_ko": (boss_ko, (), -3),
    "caught": (caught, (), -9), "win": (win, (), -5), "intro": (intro, (), -8),
    "ui_move": (ui_move, (), -16), "ui_select": (ui_select, (), -11), "ui_back": (ui_back, (), -12), "ui_locked": (ui_locked, (), -12),
    "shop_buy": (shop_buy, (), -8), "shop_equip": (shop_equip, (), -12), "shop_fail": (shop_fail, (), -12),
}
