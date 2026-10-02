"""The four original music loops of Sneak. Every note below is written out by hand and rendered by instruments.py.

Each track is 16 bars and loops seamlessly (notes and effect tails wrap around the loop point).
    music_menu   - "Tiptoe"       C major, 112 BPM: bouncy, pizzicato, playful
    music_house  - "Past Bedtime" D minor,  96 BPM: sparse, sneaky, tense
    music_egypt  - "Sand and Whiskers" D phrygian dominant, 104 BPM: reed melody over hand drums
    music_boss   - "Sir Pounce"   A minor, 138 BPM: driving, with a harpsichord and brass
"""
import numpy as np

from instruments import *  # noqa: F401,F403
from synth import secs


def root_midi(name, lo):
    """Lowest note at or above MIDI 'lo' that has the chord's root pitch class."""
    minor = name.endswith("m")
    pc = mid((name[:-1] if minor else name) + "0") % 12
    return lo + ((pc - lo) % 12)


def merged(chords):
    """[(start_beat, length_beats, chord_name)] with repeated neighbours joined (for pads)."""
    out = []
    for bar, name in enumerate(chords):
        if out and out[-1][2] == name:
            out[-1][1] += 4
        else:
            out.append([bar * 4, 4, name])
    return out


def span(events, lo_beat, hi_beat, up=0, vel=1.0):
    return [(b, d, m + up, vel) for b, d, m, *_ in events if lo_beat <= b < hi_beat]


# ======================================================================== menu
MENU_CHORDS = "C Am F G  C Am F G  Dm G C Am  F G C C".split()
MENU_MELODY = """
E5:.5 G5:.5 E5:.5 C5:.5 D5:.5 E5:1.5 |
C5:.5 E5:.5 A5:1 G5:.5 E5:.5 C5:1 |
A5:.5 G5:.5 F5:.5 A5:.5 C6:1 A5:1 |
G5:1.5 F5:.5 D5:1 r:1 |
E5:.5 G5:.5 C6:.5 G5:.5 E5:.5 G5:1.5 |
E5:.5 A5:.5 C6:.5 A5:.5 E5:1 r:1 |
F5:.5 A5:.5 C6:.5 D6:.5 C6:1 A5:1 |
B5:1 G5:1 D5:1 r:1 |
D5:.5 F5:.5 A5:.5 F5:.5 D6:1 A5:1 |
B5:.5 D6:.5 B5:.5 G5:.5 F5:1 D5:1 |
E5:.5 G5:.5 C6:1 B5:.5 C6:.5 E6:1 |
C6:1 A5:.5 E5:.5 A5:2 |
A5:.5 C6:.5 A5:.5 F5:.5 G5:.5 A5:1.5 |
B5:.5 G5:.5 D5:.5 G5:.5 B5:1 D6:1 |
C6:.5 B5:.5 G5:.5 E5:.5 C5:2 |
r:3 G4:.5 C5:.5
"""


def menu():
    m = Mixer(112, 16)
    mel = parse(MENU_MELODY)
    swing = 0.035

    m.notes(marimba, mel, gain=0.8, pan=0.08, rev=0.22, swing=swing, vel_var=0.06)
    # The second time around each phrase gets a music-box doubling an octave up.
    m.notes(celeste, span(mel, 16, 32, 12, 0.55) + span(mel, 48, 64, 12, 0.55), gain=0.5, pan=-0.25, rev=0.4, ech=0.25, swing=swing)

    bass_ev, stabs, pads = [], [], []
    for bar, name in enumerate(MENU_CHORDS):
        b = bar * 4
        r = root_midi(name, 40)
        for off, interval, vel in ((0, 0, 1.0), (1, 7, 0.7), (2, 0, 0.9), (3, 7, 0.7)):
            bass_ev.append((b + off, 0.4, r + interval, vel))
        c = chord(name, 4)
        for off in (1.5, 3.5):
            stabs += [(b + off, 0.22, n_, 0.55) for n_ in c]
    for start, length, name in merged(MENU_CHORDS):
        pads += [(start, length, n_, 0.6) for n_ in chord(name, 3)]
    m.notes(bass, bass_ev, gain=0.85, swing=swing)
    m.notes(pluck, stabs, gain=0.17, pan=-0.35, rev=0.2, swing=swing)
    m.notes(pad, pads, gain=0.16, rev=0.3, pan=0.0)

    beats = np.arange(64)
    m.hits(softkick, [(b, 0.8 if b % 4 == 0 else 0.55) for b in beats if b % 2 == 0], gain=0.7)
    m.hits(rim, [(b, 0.5) for b in beats if b % 2 == 1], gain=0.45, pan=0.2)
    m.hits(shaker, [(b * 0.5, 0.55 if b % 2 == 0 else 0.3) for b in range(128)], gain=0.4, pan=-0.3, swing=swing)
    m.hits(wood, [(b + 3.0 + 0.25 * i, 0.45 + 0.1 * i) for b in (28, 60) for i in range(4)], gain=0.5, pan=0.3, high=True)
    return m.render(rev_size=0.9, echo_beats=0.75, echo_fb=0.3)


# ======================================================================== house
HOUSE_CHORDS = "Dm Dm Bb A  Dm Gm Bb A  Dm Dm Bb A  Gm Gm A A".split()
HOUSE_MELODY = """
r:1 D5:.5 F5:.5 A5:1.5 r:.5 |
G5:.5 F5:.5 D5:1 r:2 |
r:1 Bb4:.5 D5:.5 F5:1 D5:1 |
C#5:1 E5:1 A5:1.5 r:.5 |
r:1 D5:.5 F5:.5 A5:1 C6:1 |
Bb5:1 A5:.5 G5:.5 D5:2 |
F5:.5 D5:.5 Bb4:1 r:1 D5:.5 F5:.5 |
E5:1.5 C#5:.5 A4:2 |
D5:.5 r:.5 F5:.5 r:.5 A5:.5 r:.5 D6:1 |
C6:1 A5:1 F5:2 |
D6:.5 C6:.5 Bb5:1 A5:1 F5:1 |
E5:1 C#5:1 E5:.5 A5:.5 G5:1 |
Bb5:1.5 A5:.5 G5:1 D5:1 |
G5:.5 A5:.5 Bb5:1 D6:2 |
C#6:1 A5:1 E5:1 C#5:1 |
E5:2 r:2
"""


def house():
    m = Mixer(96, 16)
    mel = parse(HOUSE_MELODY)

    m.notes(celeste, mel, gain=0.7, pan=0.1, rev=0.55, ech=0.35, vel_var=0.05)

    bass_ev, offbeat, pads = [], [], []
    for bar, name in enumerate(HOUSE_CHORDS):
        b = bar * 4
        r = root_midi(name, 38)
        for off, interval, vel in ((0, 0, 1.0), (0.75, 0, 0.55), (1.5, 7, 0.7), (2, 0, 0.9), (2.75, 0, 0.55), (3.5, 12, 0.7)):
            bass_ev.append((b + off, 0.3, r + interval, vel))
        c = chord(name, 4)
        for off in (1.5, 3.5):
            offbeat.append((b + off, 0.2, c[2], 0.5))
    for start, length, name in merged(HOUSE_CHORDS):
        pads += [(start, length, n_, 0.55) for n_ in chord(name, 3)]
    m.notes(bass, bass_ev, gain=0.9)
    m.notes(pluck, offbeat, gain=0.22, pan=-0.4, rev=0.3)
    m.notes(pad, pads, gain=0.2, rev=0.35, bright=800)

    beats = range(64)
    m.hits(wood, [(b, 0.28) for b in beats], gain=0.5, pan=-0.2, high=True)   # a ticking clock
    m.hits(rim, [(b, 0.5) for b in beats if b % 2 == 1], gain=0.45, pan=0.25)
    m.hits(softkick, [(b, 0.7) for b in beats if b % 4 == 0], gain=0.6)
    m.hits(hat, [(b * 0.5 + 0.5, 0.35) for b in range(0, 128, 2)], gain=0.35, pan=0.35)
    return m.render(rev_size=1.15, rev_damp=3000, echo_beats=0.75, echo_fb=0.38)


# ======================================================================== egypt
EGYPT_CHORDS = "D D Eb D  D Gm Eb D  Gm Gm Eb D  Eb Eb D D".split()
EGYPT_MELODY = """
D5:1 Eb5:.5 F#5:.5 G5:1 F#5:.5 Eb5:.5 |
D5:1.5 Eb5:.5 D5:1 r:1 |
Eb5:1 G5:.5 Bb5:.5 A5:1 G5:1 |
F#5:.5 Eb5:.5 D5:1 Eb5:.5 D5:.5 A4:1 |
A5:1 A5:.5 Bb5:.5 A5:.5 G5:.5 F#5:1 |
G5:1 Bb5:1 A5:.5 G5:.5 F#5:1 |
G5:.5 Bb5:.5 D6:1 C6:.5 Bb5:.5 G5:1 |
F#5:1 Eb5:.5 D5:.5 D5:2 |
D6:1 D6:.5 Eb6:.5 D6:.5 C6:.5 Bb5:1 |
A5:.5 Bb5:.5 C6:1 D6:1 Bb5:1 |
Bb5:1 G5:.5 Bb5:.5 Eb6:1 D6:1 |
C6:.5 Bb5:.5 A5:1 F#5:.5 Eb5:.5 D5:1 |
G5:1 F#5:.5 G5:.5 Bb5:1 A5:.5 G5:.5 |
Eb5:.5 G5:.5 Bb5:1 G5:.5 F#5:.5 Eb5:1 |
F#5:1 A5:1 D6:1 A5:.5 F#5:.5 |
D5:3 r:1
"""


def egypt():
    m = Mixer(104, 16)
    mel = parse(EGYPT_MELODY)
    m.notes(reed, mel, gain=0.75, pan=0.1, rev=0.3, ech=0.18, vel_var=0.04)

    bass_ev, oud_ev, pads = [], [], []
    for bar, name in enumerate(EGYPT_CHORDS):
        b = bar * 4
        r = root_midi(name, 38)
        bass_ev += [(b, 1.3, r, 1.0), (b + 1.5, 0.4, r, 0.7), (b + 2, 1.3, r, 0.9), (b + 3.5, 0.4, r + 7, 0.7)]
        c = chord(name, 3)
        notes_ = [c[0], c[2], c[1], c[0] + 12, c[2], c[1], c[0] + 12, c[1]] if bar % 4 != 3 else [c[0], c[2], c[1], c[2], c[0], c[2], c[1], c[2]]
        oud_ev += [(b + i * 0.5, 0.4, n_, 1.0 if i % 4 == 0 else 0.6) for i, n_ in enumerate(notes_)]
    for start, length, name in merged(EGYPT_CHORDS):
        pads += [(start, length, n_, 0.6) for n_ in chord(name, 3)]
    m.notes(bass, bass_ev, gain=0.9)
    m.notes(oud, oud_ev, gain=0.34, pan=-0.4, rev=0.2)
    m.notes(pad, pads, gain=0.16, rev=0.4, bright=1800)

    dums, teks, kas, zills = [], [], [], []
    for bar in range(16):
        b = bar * 4
        dums += [(b, 1.0), (b + 2, 0.9)]
        teks += [(b + 0.5, 0.7), (b + 1.5, 0.7), (b + 3, 0.75)]
        kas += [(b + 2.5, 0.35), (b + 3.5, 0.35)]
        zills += [(b + 1.5, 0.5), (b + 3.5, 0.45)]
        if bar >= 8 and bar < 12:
            zills += [(b + 0.5, 0.35), (b + 2.5, 0.35)]
        if bar % 4 == 3:  # little roll leading into the next phrase
            kas = [k for k in kas if not (b + 3.0 <= k[0] < b + 4.0)]
            teks = [t for t in teks if not (b + 3.0 <= t[0] < b + 4.0)]
            teks += [(b + 3.0, 0.7)]
            kas += [(b + 3.25, 0.4), (b + 3.5, 0.55), (b + 3.75, 0.7)]
    m.hits(dum, dums, gain=0.9)
    m.hits(tek, teks, gain=0.6, pan=0.25)
    m.hits(ka, kas, gain=0.55, pan=-0.2)
    m.hits(zill, zills, gain=0.55, pan=0.4, rev=0.2)
    return m.render(rev_size=1.0, rev_damp=3800, echo_beats=0.75, echo_fb=0.28)


# ======================================================================== boss
BOSS_CHORDS = "Am Am F E  Am Dm F E  F G Am C  F G E E".split()
BOSS_MELODY = """
A5:1 r:.5 C6:.5 E6:1 D6:.5 C6:.5 |
B5:.5 C6:.5 B5:.5 A5:.5 E5:2 |
F5:1 A5:.5 C6:.5 F6:1 E6:.5 D6:.5 |
E6:1 D6:.5 C6:.5 B5:1 G#5:1 |
A5:.5 C6:.5 E6:1 A6:1 G6:.5 E6:.5 |
D6:1 F6:1 E6:.5 D6:.5 A5:1 |
A5:.5 C6:.5 F6:1 E6:.5 D6:.5 C6:1 |
B5:1 G#5:1 E5:1 B5:.5 G#5:.5 |
C6:1 F6:1 E6:.5 D6:.5 C6:1 |
D6:1 G6:1 F6:.5 E6:.5 D6:1 |
E6:1.5 D6:.5 C6:1 A5:1 |
G5:.5 C6:.5 E6:1 G6:2 |
A6:1 G6:.5 F6:.5 E6:.5 D6:.5 C6:1 |
G6:1 F6:.5 E6:.5 D6:.5 B5:.5 D6:1 |
E6:1 B5:.5 G#5:.5 E6:1 B5:.5 G#5:.5 |
E6:2 r:1 B5:.5 E6:.5
"""


def boss():
    m = Mixer(138, 16)
    mel = parse(BOSS_MELODY)
    m.notes(harpsichord, span(mel, 0, 32), gain=1.0, pan=0.1, rev=0.2, ech=0.15)
    m.notes(brass, span(mel, 32, 64), gain=0.8, pan=0.05, rev=0.2, ech=0.18)
    m.notes(brass, span(mel, 32, 64, -12, 0.5), gain=0.42, pan=-0.15, rev=0.2)

    bass_ev, arp, stabs, pads = [], [], [], []
    for bar, name in enumerate(BOSS_CHORDS):
        b = bar * 4
        r = root_midi(name, 40)
        for i, (interval, vel) in enumerate(((0, 1.0), (0, 0.7), (12, 0.9), (0, 0.7), (0, 1.0), (0, 0.7), (12, 0.9), (0, 0.7))):
            bass_ev.append((b + i * 0.5, 0.42, r + interval, vel))
        c = chord(name, 4)
        shape = [c[0], c[1], c[2], c[0] + 12]
        for i, idx in enumerate([0, 1, 2, 3, 2, 1, 0, 1] * 2):
            arp.append((b + i * 0.25, 0.2, shape[idx], 1.0 if i % 4 == 0 else 0.55))
        if bar >= 8:
            stabs += [(b, 0.6, n_, 0.9) for n_ in c] + [(b + 1.5, 0.35, n_, 0.7) for n_ in c] + [(b + 2.5, 0.35, n_, 0.7) for n_ in c]
    for start, length, name in merged(BOSS_CHORDS):
        if start >= 32:
            pads += [(start, length, n_, 0.7) for n_ in chord(name, 3)]
    m.notes(sawbass, bass_ev, gain=0.6)
    m.notes(harpsichord, [(b, d, n_, v) for b, d, n_, v in arp if b < 32], gain=0.5, pan=-0.35, rev=0.15)
    m.notes(harpsichord, [(b, d, n_, v * 0.7) for b, d, n_, v in arp if b >= 32], gain=0.4, pan=-0.35, rev=0.15)
    m.notes(brass, stabs, gain=0.3, pan=0.35, rev=0.2)
    m.notes(pad, pads, gain=0.2, rev=0.3, bright=2200)

    kicks, snares, hats, opens, toms = [], [], [], [], []
    for bar in range(16):
        b = bar * 4
        kicks += [(b, 1.0), (b + 1, 0.8), (b + 2, 0.95), (b + 3, 0.8)]
        if bar % 2 == 1:
            kicks.append((b + 2.5, 0.6))
        snares += [(b + 1, 1.0), (b + 3, 1.0)]
        hats += [(b + i * 0.5, 0.8 if i % 2 else 0.5) for i in range(8)]
        hats += [(b + i * 0.5 + 0.25, 0.22) for i in range(8) if i % 2 == 1]
        opens.append((b + 3.5, 0.5))
        if bar % 4 == 3:  # fill: drop the usual last-beat hats and roll the toms
            toms += [(b + 3.0 + 0.25 * i, f_) for i, f_ in enumerate((210, 170, 140, 105))]
            snares = [s for s in snares if s[0] != b + 3]
            kicks = [k for k in kicks if k[0] not in (b + 3, b + 2.5)]
            hats = [h for h in hats if h[0] < b + 3]
            opens = [o for o in opens if o[0] != b + 3.5]
    m.hits(kick, kicks, gain=0.8)
    m.hits(snare, snares, gain=0.8, pan=0.05, rev=0.12)
    m.hits(hat, hats, gain=0.32, pan=0.3)
    m.hits(hat, opens, gain=0.3, pan=0.3, open_=True)
    for i, (beat, f_) in enumerate(toms):
        m.hits(lambda v, f_=f_: tom(f_, v), [(beat, 0.9)], gain=0.75, pan=-0.3 + 0.12 * (i % 4), rev=0.15)
    m.hits(crash, [(0, 0.9), (32, 0.9)], gain=0.4, pan=0.2, rev=0.2)
    return m.render(rev_size=0.85, rev_damp=4500, echo_beats=0.75, echo_fb=0.28)


def _finish(fn, rms_db=-16.0, drive=1.2):
    return lambda: master(fn(), rms_db, -1.0, drive)


TRACKS = {
    "music_menu": _finish(menu),
    "music_house": _finish(house),
    "music_egypt": _finish(egypt),
    "music_boss": _finish(boss),
}
