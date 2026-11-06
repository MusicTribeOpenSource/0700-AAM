#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Builds emu/tests/traces/leds.trace for the LED replay comparison.

The emulated host only sends boot-time pad and slider frames (leds-live.trace, recorded from the
unmodified host). The LED paths the replay has to cover (button LEDs, blink, multi-colour pads,
step pads, strip) are not exercised in the emulator, so this script appends host-to-MCU frames
built from the payload layouts in Kick/modules/contracts/dcl/dcl_contracts_helper.cpp.

Usage: make-led-trace.py <live-trace> <out-trace>
"""
import sys

POLY = 0x31
INIT = 0xFF
LED, PAD, SLIDER = 2, 3, 7
STATE_OFF, STATE_ON, STATE_SAMPLE_ASSIGNED, STATE_SAMPLE_NOT_ASSIGNED, STATE_ON_BLINK, STATE_OFF_BLINK = range(6)
TRANS = 15


def crc8(data: bytes) -> int:
    crc = INIT
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ POLY) & 0xFF if crc & 0x80 else (crc << 1) & 0xFF
    return crc


def frame(ftype: int, cmd: int, payload: list[int]) -> bytes:
    body = bytes([ord("M"), ord("T"), ftype, 0, cmd, len(payload) + 1] + payload)
    return body + bytes([crc8(body)])


def main() -> int:
    live, out = sys.argv[1], sys.argv[2]
    with open(live, encoding="utf-8") as f:
        lines = f.read().splitlines()
    frames: list[bytes] = [
        # Button LEDs: bank 0 then bank 3 (radio group), play on, stop on (radio), a lone button.
        frame(LED, 0, [0, STATE_ON, 5]),
        frame(LED, 0, [3, STATE_ON, 5]),
        frame(LED, 0, [44, STATE_ON, 12]),
        frame(LED, 0, [45, STATE_ON, 12]),
        frame(LED, 0, [20, STATE_ON, 1]),
        frame(LED, 0, [20, STATE_OFF, 1]),
        frame(LED, 0, [30, STATE_ON, 7]),
        # Pads.
        frame(PAD, 2, [STATE_ON, 3]),
        frame(PAD, 1, [4, STATE_ON, 12]),
        frame(PAD, 1, [9, STATE_ON_BLINK, 8]),
        frame(PAD, 3, [STATE_ON] + [(i * 5) % 18 for i in range(16)]),
        frame(PAD, 1, [2, STATE_OFF, TRANS]),
        frame(PAD, 6, [(i * 7) % 18 for i in range(16)] + [STATE_ON if i % 3 else STATE_OFF for i in range(16)]),
        frame(PAD, 8, [6, STATE_ON_BLINK, 4, 4]),
        frame(PAD, 9, [11, STATE_ON, 9, 100]),
        frame(PAD, 4, [16, 20, 50, 1]),
        frame(PAD, 5, [1]),
        # Strip.
        frame(SLIDER, 1, [40]),
        frame(SLIDER, 2, [75]),
    ]
    t = 2000.0
    with open(out, "w", encoding="utf-8") as f:
        for line in lines:
            f.write(line + "\n")
        for fr in frames:
            t += 10.0
            f.write(f"{t:.3f} RX {' '.join(f'{b:02x}' for b in fr)}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
