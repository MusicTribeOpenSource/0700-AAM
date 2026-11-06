#!/usr/bin/env python3
"""Fails when the Surface's copy of the wire protocol drifts from Kick's.

Run: python3 emu/tests/protocol_drift.py   (exit code 0 = in sync)

emu/surface/protocol.h mirrors enums and ids from Kick/modules/contracts/dcl/dcl_contracts.h and
ComProtocol/PktMgr/PktMgr.hpp so the Surface builds without the Kick tree. Both sides are plain C++ enums, so this
reads them as text, assigns each member its value (an explicit `= n`, otherwise the previous value plus one) and
compares the pairs below. A new button or command upstream shows up here instead of as a wrong LED on the Surface.
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
SURFACE = REPO / "emu/surface/protocol.h"
CONTRACTS = REPO / "Kick/modules/contracts/dcl/dcl_contracts.h"
PKTMGR = REPO / "ComProtocol/PktMgr/PktMgr.hpp"

# Surface enum -> (Kick file, Kick enum). Members pair up by position, so a reordering counts as drift.
ENUM_PAIRS = {
    "PacketType": (PKTMGR, "enumPacketType"),
    "SysCmd": (CONTRACTS, "SysCmd"),
    "LedCmd": (CONTRACTS, "LedCmd"),
    "PadCmd": (CONTRACTS, "PadCmd"),
    "SliderCmd": (CONTRACTS, "SliderCmd"),
    "PadButtonState": (CONTRACTS, "PadButtonState"),
}

# Surface constant -> (Kick enum, member)
CONSTANT_PAIRS = {
    "numButtons": ("BtnLedSequence", "SIZE"),
    "numPads": ("PadSequence", "EN_ALL_PADS"),
    "numKnobs": ("KnobSequence", "EN_KNOB_16"),
    "btnShift": ("BtnLedSequence", "EN_SHIFT"),
    "btnPlay": ("BtnLedSequence", "EN_PLAY"),
    "btnStop": ("BtnLedSequence", "EN_STOP"),
    "btnEnter": ("BtnLedSequence", "EN_KNOB_ENTER"),
    "btnKnobCcw": ("BtnLedSequence", "EN_KNOB_CCW"),
    "btnKnobCw": ("BtnLedSequence", "EN_KNOB_CW"),
    "btnBack": ("BtnLedSequence", "EN_BACK"),
    "colourTrans": ("PadColourIds", "PadColourTrans"),
    "knobCenter": ("KnobSequence", "EN_KNOB_CENTER"),
    "knobCmdRotary": ("KnobCmd", "EN_KNOB_ROTARY"),
    "knobCmdQlink": ("KnobCmd", "EN_KNOB_QLINK"),
}


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def parse_enums(text: str) -> dict[str, list[tuple[str, int]]]:
    """Returns {enum name: [(member, value), ...]} for every `enum Name { ... }` in the text."""
    enums = {}
    for match in re.finditer(r"\benum\s+(\w+)\s*\{(.*?)\}", strip_comments(text), flags=re.S):
        members, value = [], -1
        for item in match.group(2).split(","):
            item = item.strip()
            if not item:
                continue
            name, _, explicit = (part.strip() for part in item.partition("="))
            value = int(explicit, 0) if explicit else value + 1
            members.append((name, value))
        enums[match.group(1)] = members
    return enums


def parse_constants(text: str) -> dict[str, int]:
    return {m.group(1): int(m.group(2)) for m in re.finditer(r"constexpr\s+int\s+(\w+)\s*=\s*(-?\d+)", strip_comments(text))}


def main() -> int:
    surface_enums = parse_enums(SURFACE.read_text())
    constants = parse_constants(SURFACE.read_text())
    kick_enums = {}
    for path in (CONTRACTS, PKTMGR):
        kick_enums.update(parse_enums(path.read_text()))

    problems = []
    for ours, (path, theirs) in ENUM_PAIRS.items():
        mine, upstream = surface_enums.get(ours), kick_enums.get(theirs)
        if mine is None or upstream is None:
            problems.append(f"{ours}: enum {'in protocol.h' if mine is None else theirs} not found")
            continue
        if [v for _, v in mine] != [v for _, v in upstream][: len(mine)]:
            problems.append(f"{ours} differs from {theirs}: {mine} vs {upstream}")
        elif len(mine) != len(upstream):
            problems.append(f"{ours} has {len(mine)} members, {theirs} has {len(upstream)}")

    for ours, (enum, member) in CONSTANT_PAIRS.items():
        values = dict(kick_enums.get(enum, []))
        if ours not in constants or member not in values:
            problems.append(f"{ours}: {'missing in protocol.h' if ours not in constants else f'{enum}::{member} not found'}")
        elif constants[ours] != values[member]:
            problems.append(f"{ours} is {constants[ours]}, {enum}::{member} is {values[member]}")

    if problems:
        print("protocol drift:", *problems, sep="\n  ", file=sys.stderr)
        return 1
    print(f"protocol in sync ({len(ENUM_PAIRS)} enums, {len(CONSTANT_PAIRS)} constants)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
