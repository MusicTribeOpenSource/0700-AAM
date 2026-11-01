#!/usr/bin/env python3
"""Writes a starter drum kit of synthesized one-shots for the Kick emulator.

Kick's public release ships no samples. These are generated from scratch (no third
party audio), 16-bit mono WAV at 48 kHz, written to <root>/data/sounds where the
unit keeps its sample pool, so they show up in Kick's sample browser.

Usage: make_samples.py [root]    (root defaults to $KICK_EMU_ROOT or ~/KickEmu)
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 48000


def write(path: str, samples: list[float]) -> None:
    peak = max(1e-9, max(abs(s) for s in samples))
    gain = 0.89 / peak  # about -1 dBFS
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * gain)) * 32767)) for s in samples)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(frames)


def env(i: int, n: int, decay: float) -> float:
    return math.exp(-decay * i / n)


def kick(n: int = int(0.45 * RATE)) -> list[float]:
    out, phase = [], 0.0
    for i in range(n):
        freq = 45 + 120 * math.exp(-i / (0.03 * RATE))
        phase += 2 * math.pi * freq / RATE
        out.append(math.sin(phase) * env(i, n, 6.0))
    return out


def snare(n: int = int(0.30 * RATE)) -> list[float]:
    rng = random.Random(1)
    return [
        (0.55 * math.sin(2 * math.pi * 190 * i / RATE) * env(i, n, 14) + 0.8 * (rng.random() * 2 - 1) * env(i, n, 9))
        for i in range(n)
    ]


def hat(open_: bool) -> list[float]:
    rng = random.Random(2 if open_ else 3)
    n = int((0.35 if open_ else 0.08) * RATE)
    prev, out = 0.0, []
    for i in range(n):
        x = rng.random() * 2 - 1
        out.append((x - prev) * env(i, n, 5 if open_ else 12))  # first difference = high-pass
        prev = x
    return out


def clap(n: int = int(0.28 * RATE)) -> list[float]:
    rng = random.Random(4)
    bursts = [0, int(0.012 * RATE), int(0.024 * RATE)]
    out = [0.0] * n
    for b in bursts:
        for i in range(b, min(n, b + int(0.02 * RATE))):
            out[i] += (rng.random() * 2 - 1) * env(i - b, int(0.02 * RATE), 4)
    for i in range(bursts[-1], n):
        out[i] += (rng.random() * 2 - 1) * 0.6 * env(i - bursts[-1], n, 9)
    return out


def tom(freq: float, n: int = int(0.35 * RATE)) -> list[float]:
    out, phase = [], 0.0
    for i in range(n):
        phase += 2 * math.pi * (freq + 40 * math.exp(-i / (0.05 * RATE))) / RATE
        out.append(math.sin(phase) * env(i, n, 7))
    return out


def bass(freq: float, n: int = int(0.6 * RATE)) -> list[float]:
    return [
        (math.sin(2 * math.pi * freq * i / RATE) + 0.3 * math.sin(4 * math.pi * freq * i / RATE)) * env(i, n, 4)
        for i in range(n)
    ]


def rim(n: int = int(0.06 * RATE)) -> list[float]:
    return [math.sin(2 * math.pi * 1700 * i / RATE) * env(i, n, 10) for i in range(n)]


def pluck(freq: float, n: int = int(0.5 * RATE)) -> list[float]:
    return [
        sum(math.sin(2 * math.pi * freq * k * i / RATE) / k for k in (1, 2, 3, 4)) * env(i, n, 6)
        for i in range(n)
    ]


KIT = {
    "01 Kick": kick,
    "02 Snare": snare,
    "03 Closed Hat": lambda: hat(False),
    "04 Open Hat": lambda: hat(True),
    "05 Clap": clap,
    "06 Rim": rim,
    "07 Tom Low": lambda: tom(90),
    "08 Tom Mid": lambda: tom(140),
    "09 Tom High": lambda: tom(210),
    "10 Bass C1": lambda: bass(32.7),
    "11 Bass E1": lambda: bass(41.2),
    "12 Bass G1": lambda: bass(49.0),
    "13 Pluck C3": lambda: pluck(130.8),
    "14 Pluck E3": lambda: pluck(164.8),
    "15 Pluck G3": lambda: pluck(196.0),
    "16 Pluck C4": lambda: pluck(261.6),
}


def main() -> int:
    root = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("KICK_EMU_ROOT") or os.path.expanduser("~/KickEmu")
    out_dir = os.path.join(root, "data", "sounds", "Emu Kit")
    os.makedirs(out_dir, exist_ok=True)
    for name, fn in KIT.items():
        write(os.path.join(out_dir, f"{name}.wav"), fn())
    print(f"wrote {len(KIT)} samples to {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
