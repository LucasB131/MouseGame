#!/usr/bin/env python3
"""Generates every sound in Sneak. Nothing is sampled or downloaded: all of it is synthesized from scratch.

    python tools/audio/generate_audio.py [output_dir]      (default: assets/audio)

Needs numpy, scipy and ffmpeg (with libvorbis) on the PATH; ffmpeg is only used to encode the music to OGG.
Sound effects are written as 16-bit mono WAV files, the music as stereo OGG loops.
"""
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from synth import SR, declick, peak_normalize, write_wav  # noqa: E402
from sfx import SFX  # noqa: E402


def build_sfx(out_dir):
    for name, (fn, args, peak_db) in SFX.items():
        x = declick(peak_normalize(fn(*args), peak_db), 0.001, 0.01)
        write_wav(os.path.join(out_dir, name + ".wav"), x)
        print(f"  sfx   {name}")


def build_music(out_dir):
    try:
        from music import TRACKS
    except ImportError:
        return
    for name, render in TRACKS.items():
        wav = os.path.join(out_dir, name + ".tmp.wav")
        ogg = os.path.join(out_dir, name + ".ogg")
        write_wav(wav, render())
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-c:a", "libvorbis", "-q:a", "4", ogg], check=True)
        os.remove(wav)
        print(f"  music {name}")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "assets", "audio")
    os.makedirs(out, exist_ok=True)
    build_sfx(out)
    build_music(out)
    print("done")
