#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Stamp, check and inspect the header of a KICK control-surface MCU image.

The host updater (kick_FirmwareUpdate.cpp) sends an Intel HEX file to the MCU record
by record and has no image-level checksum of its own. This tool adds one: it writes a
64-byte header at the start of the image holding the length, a CRC-32 of the payload,
the version and the FLASH_VALID marker. The MCU checks all of that before it will boot
the image (Firmware/kick-mcu/include/kick_mcu/system/image.h has the layout, and this
file has to agree with it byte for byte).

    mcu-image-header.py stamp --hex fw.hex -o fw-stamped.hex --version 1.2.0
    mcu-image-header.py stamp --bin fw.bin -o fw-stamped.hex --version 1.2.0
    mcu-image-header.py verify fw-stamped.hex
    mcu-image-header.py info fw-stamped.hex

Addresses in the hex are offsets from the start of the image, not flash addresses. The
MCU adds the base of the slot it is writing to. With --hex the first 64 bytes are
reserved for the header (leave them out of the link, or fill them with 0xFF or 0x00);
with --bin the tool puts the header in front of the raw payload.

The output hex holds only the record types the host handles: 00 data, 04 extended linear
address and 01 end of file. The host treats any other type as the end of the file, so
start-address records (03, 05) in the input are dropped.

This is a checksum, not a signature. It catches a corrupted or truncated transfer. It
does not stop someone from building their own image. The header has a 32-byte signature
field, written as zeros and not checked, reserved for the day the MCU part is known and
can verify one.
"""
from __future__ import annotations

import argparse
import logging
import struct
import sys
import zlib
from pathlib import Path

LOG = logging.getLogger("mcu-image-header")

FLASH_VALID = 0x12345678
HEADER_BYTES = 64
HEADER_VERSION = 1
HEADER_CRC_OFFSET = 24
SIGNATURE_OFFSET = 32

# The host reads a record into a 32-byte buffer and sends binaryData[0 .. n + 2], so a
# record longer than 29 data bytes reads past the end of that buffer.
MAX_RECORD_BYTES = 29
DEFAULT_RECORD_BYTES = 16


class ImageError(Exception):
    """The input is not something this tool can turn into, or accept as, an image."""


def crc32(data: bytes) -> int:
    """CRC-32/ISO-HDLC, the same one kick_mcu::system::crc32 computes."""
    return zlib.crc32(data) & 0xFFFFFFFF


def parse_version(text: str) -> tuple[int, int, int]:
    parts = text.split(".")
    if len(parts) not in (2, 3) or not all(p.isdigit() for p in parts):
        raise argparse.ArgumentTypeError(
            f"version must be MAJOR.MINOR[.BUG], got {text!r}"
        )
    numbers = [int(p) for p in parts] + [0] * (3 - len(parts))
    if any(n > 255 for n in numbers):
        raise argparse.ArgumentTypeError("each version field must fit in a byte")
    return numbers[0], numbers[1], numbers[2]


def read_hex(text: str) -> dict[int, int]:
    """Parses Intel HEX into {address: byte}. Rejects what the host updater cannot carry."""
    memory: dict[int, int] = {}
    upper = 0
    seen_eof = False
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line:
            continue
        if not line.startswith(":"):
            raise ImageError(f"line {number}: a record must start with ':'")
        try:
            record = bytes.fromhex(line[1:])
        except ValueError as e:
            raise ImageError(f"line {number}: not hex ({e})") from e
        if len(record) < 5 or len(record) != record[0] + 5:
            raise ImageError(f"line {number}: length byte does not match the record")
        if sum(record) & 0xFF:
            raise ImageError(f"line {number}: record checksum is wrong")
        count, address, rtype = record[0], (record[1] << 8) | record[2], record[3]
        body = record[4 : 4 + count]
        if seen_eof:
            raise ImageError(f"line {number}: data after the end-of-file record")
        if rtype == 0:
            for i, value in enumerate(body):
                memory[(upper << 16) + address + i] = value
        elif rtype == 1:
            seen_eof = True
        elif rtype == 4:
            if count != 2:
                raise ImageError(
                    f"line {number}: extended linear address needs 2 bytes"
                )
            upper = (body[0] << 8) | body[1]
        elif rtype in (3, 5):
            LOG.info(
                "line %d: dropping start-address record (type %02X)", number, rtype
            )
        else:
            raise ImageError(
                f"line {number}: record type {rtype:02X} is not supported "
                "(the host updater stops at it)"
            )
    if not seen_eof:
        raise ImageError("no end-of-file record")
    return memory


def flatten(memory: dict[int, int]) -> bytes:
    """Contiguous bytes from address 0 to the highest byte, gaps filled with 0xFF."""
    if not memory:
        raise ImageError("the hex file holds no data")
    top = max(memory) + 1
    out = bytearray(b"\xff" * top)
    for address, value in memory.items():
        out[address] = value
    return bytes(out)


def build_header(payload: bytes, version: tuple[int, int, int], revision: int) -> bytes:
    header = bytearray(HEADER_BYTES)
    struct.pack_into(
        "<IHHII",
        header,
        0,
        FLASH_VALID,
        HEADER_BYTES,
        HEADER_VERSION,
        HEADER_BYTES + len(payload),
        crc32(payload),
    )
    header[16], header[17], header[18] = version
    struct.pack_into("<H", header, 20, revision)
    struct.pack_into(
        "<I", header, HEADER_CRC_OFFSET, crc32(bytes(header[:HEADER_CRC_OFFSET]))
    )
    return bytes(header)


def stamp(
    flat: bytes | None,
    payload_only: bytes | None,
    version: tuple[int, int, int],
    revision: int,
    slot_bytes: int | None,
) -> bytes:
    """Returns the finished image: header followed by payload."""
    if payload_only is not None:
        payload = payload_only
    else:
        assert flat is not None
        if len(flat) <= HEADER_BYTES:
            raise ImageError(
                f"the image holds {len(flat)} bytes; the first {HEADER_BYTES} are the header"
            )
        reserved = set(flat[:HEADER_BYTES])
        if not reserved <= {0x00, 0xFF}:
            raise ImageError(
                f"the first {HEADER_BYTES} bytes are reserved for the header "
                "but hold other data; leave them out of the link"
            )
        payload = flat[HEADER_BYTES:]
    if not payload:
        raise ImageError("the payload is empty")
    image = build_header(payload, version, revision) + payload
    if slot_bytes is not None and len(image) > slot_bytes:
        raise ImageError(
            f"the image is {len(image)} bytes and the slot holds {slot_bytes}"
        )
    return image


def write_hex(image: bytes, record_bytes: int) -> str:
    if not 1 <= record_bytes <= MAX_RECORD_BYTES:
        raise ImageError(f"record size must be 1 to {MAX_RECORD_BYTES}")

    def record(address: int, rtype: int, body: bytes) -> str:
        raw = bytes([len(body), address >> 8, address & 0xFF, rtype]) + body
        checksum = (-sum(raw)) & 0xFF
        return ":" + (raw + bytes([checksum])).hex().upper()

    lines: list[str] = []
    offset = 0
    current_upper = None
    while offset < len(image):
        upper, low = offset >> 16, offset & 0xFFFF
        if upper != current_upper:
            lines.append(record(0, 4, bytes([upper >> 8, upper & 0xFF])))
            current_upper = upper
        # A record never crosses a 64 KiB boundary: its 16-bit address would wrap.
        n = min(record_bytes, len(image) - offset, 0x10000 - low)
        lines.append(record(low, 0, image[offset : offset + n]))
        offset += n
    lines.append(":00000001FF")
    return "\n".join(lines) + "\n"


def check(image: bytes, slot_bytes: int | None = None) -> dict[str, object]:
    """Validates a finished image the way the MCU does. Raises ImageError on any fault."""
    if len(image) < HEADER_BYTES:
        raise ImageError("shorter than the header")
    header = image[:HEADER_BYTES]
    (
        flash_state,
        header_bytes,
        header_version,
        image_bytes,
        payload_crc,
    ) = struct.unpack_from("<IHHII", header, 0)
    if flash_state != FLASH_VALID:
        raise ImageError(f"marker is {flash_state:#010x}, not FLASH_VALID")
    if header_version != HEADER_VERSION or header_bytes != HEADER_BYTES:
        raise ImageError("unknown header layout")
    (stored_header_crc,) = struct.unpack_from("<I", header, HEADER_CRC_OFFSET)
    if stored_header_crc != crc32(header[:HEADER_CRC_OFFSET]):
        raise ImageError("header CRC does not match")
    if image_bytes != len(image):
        raise ImageError(f"header says {image_bytes} bytes, the image has {len(image)}")
    if slot_bytes is not None and image_bytes > slot_bytes:
        raise ImageError(f"image is {image_bytes} bytes, the slot holds {slot_bytes}")
    if payload_crc != crc32(image[HEADER_BYTES:]):
        raise ImageError("payload CRC does not match")
    (revision,) = struct.unpack_from("<H", header, 20)
    return {
        "version": f"{header[16]}.{header[17]}.{header[18]}",
        "revision": revision,
        "image_bytes": image_bytes,
        "payload_crc32": f"{payload_crc:#010x}",
        "signature_present": any(header[SIGNATURE_OFFSET:]),
    }


def command_stamp(args: argparse.Namespace) -> int:
    if args.hex:
        flat, payload = flatten(read_hex(Path(args.hex).read_text())), None
    else:
        flat, payload = None, Path(args.bin).read_bytes()
    image = stamp(flat, payload, args.version, args.revision, args.slot_bytes)
    Path(args.output).write_text(write_hex(image, args.record_bytes))
    info = check(image, args.slot_bytes)
    print(
        f"wrote {args.output}: version {info['version']}, {info['image_bytes']} bytes, "
        f"payload CRC-32 {info['payload_crc32']}"
    )
    return 0


def command_verify(args: argparse.Namespace) -> int:
    image = flatten(read_hex(Path(args.hex).read_text()))
    check(image, args.slot_bytes)
    print(f"{args.hex}: ok")
    return 0


def command_info(args: argparse.Namespace) -> int:
    image = flatten(read_hex(Path(args.hex).read_text()))
    for key, value in check(image).items():
        print(f"{key}: {value}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)

    p = sub.add_parser(
        "stamp", help="write the header into an image and emit a host-ready hex"
    )
    source = p.add_mutually_exclusive_group(required=True)
    source.add_argument(
        "--hex", help="Intel HEX with the first 64 bytes reserved for the header"
    )
    source.add_argument("--bin", help="raw payload; the header is put in front of it")
    p.add_argument("-o", "--output", required=True)
    p.add_argument(
        "--version", type=parse_version, required=True, help="MAJOR.MINOR[.BUG]"
    )
    p.add_argument(
        "--revision", type=int, default=0, help="software revision number, 0-65535"
    )
    p.add_argument("--record-bytes", type=int, default=DEFAULT_RECORD_BYTES)
    p.add_argument(
        "--slot-bytes",
        type=int,
        help="fail if the image does not fit a slot of this size",
    )
    p.set_defaults(func=command_stamp)

    p = sub.add_parser("verify", help="check a stamped hex the way the MCU will")
    p.add_argument("hex")
    p.add_argument("--slot-bytes", type=int)
    p.set_defaults(func=command_verify)

    p = sub.add_parser("info", help="print the header of a stamped hex")
    p.add_argument("hex")
    p.set_defaults(func=command_info)
    return parser


def main(argv: list[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(name)s: %(message)s")
    args = build_parser().parse_args(argv)
    if getattr(args, "revision", 0) not in range(0x10000):
        LOG.error("revision must be 0-65535")
        return 2
    try:
        return args.func(args)
    except ImageError as e:
        LOG.error("%s", e)
        return 1
    except OSError as e:
        LOG.error("cannot read or write a file: %s", e)
        return 1


if __name__ == "__main__":
    sys.exit(main())
