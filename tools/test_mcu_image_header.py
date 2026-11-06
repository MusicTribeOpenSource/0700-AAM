# SPDX-License-Identifier: AGPL-3.0-or-later
"""Tests for mcu-image-header.py. Run: python3 -m unittest tools/test_mcu_image_header.py"""
from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

_SPEC = importlib.util.spec_from_file_location(
    "mcu_image_header", Path(__file__).with_name("mcu-image-header.py")
)
assert _SPEC and _SPEC.loader
tool = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(tool)


def payload(n: int) -> bytes:
    return bytes((i * 7 + 3) & 0xFF for i in range(n))


class StampTests(unittest.TestCase):
    def test_bin_round_trips_through_hex(self) -> None:
        body = payload(300)
        image = tool.stamp(None, body, (1, 2, 3), 9, None)
        hex_text = tool.write_hex(image, 16)
        back = tool.flatten(tool.read_hex(hex_text))
        self.assertEqual(image, back)
        info = tool.check(back)
        self.assertEqual("1.2.3", info["version"])
        self.assertEqual(9, info["revision"])
        self.assertEqual(64 + 300, info["image_bytes"])

    def test_hex_input_keeps_the_payload_at_offset_64(self) -> None:
        flat = b"\xff" * 64 + payload(100)
        image = tool.stamp(flat, None, (2, 0, 0), 0, None)
        self.assertEqual(flat[64:], image[64:])
        self.assertNotEqual(flat[:64], image[:64])

    def test_reserved_header_area_must_be_empty(self) -> None:
        flat = b"\x12" + b"\xff" * 63 + payload(10)
        with self.assertRaises(tool.ImageError):
            tool.stamp(flat, None, (1, 0, 0), 0, None)

    def test_header_layout_matches_the_documented_offsets(self) -> None:
        image = tool.stamp(None, payload(40), (4, 5, 6), 0x0A0B, None)
        self.assertEqual(bytes([0x78, 0x56, 0x34, 0x12]), image[0:4])
        self.assertEqual(64, int.from_bytes(image[4:6], "little"))
        self.assertEqual(1, int.from_bytes(image[6:8], "little"))
        self.assertEqual(104, int.from_bytes(image[8:12], "little"))
        self.assertEqual(tool.crc32(image[64:]), int.from_bytes(image[12:16], "little"))
        self.assertEqual((4, 5, 6, 0), tuple(image[16:20]))
        self.assertEqual(0x0A0B, int.from_bytes(image[20:22], "little"))
        self.assertEqual(bytes(32), image[32:64], "signature field stays zero")

    def test_crc_matches_the_standard_check_value(self) -> None:
        self.assertEqual(0xCBF43926, tool.crc32(b"123456789"))

    def test_slot_size_is_enforced(self) -> None:
        with self.assertRaises(tool.ImageError):
            tool.stamp(None, payload(300), (1, 0, 0), 0, 200)

    def test_one_flipped_payload_bit_fails_verification(self) -> None:
        image = bytearray(tool.stamp(None, payload(100), (1, 0, 0), 0, None))
        image[100] ^= 0x01
        with self.assertRaises(tool.ImageError):
            tool.check(bytes(image))

    def test_truncated_image_fails_verification(self) -> None:
        image = tool.stamp(None, payload(100), (1, 0, 0), 0, None)
        with self.assertRaises(tool.ImageError):
            tool.check(image[:-1])


class HexTests(unittest.TestCase):
    def test_records_stay_within_what_the_host_buffer_holds(self) -> None:
        image = tool.stamp(None, payload(500), (1, 0, 0), 0, None)
        for size in (1, 16, tool.MAX_RECORD_BYTES):
            for line in tool.write_hex(image, size).splitlines():
                if line[7:9] == "00":
                    self.assertLessEqual(int(line[1:3], 16), size)
        with self.assertRaises(tool.ImageError):
            tool.write_hex(image, tool.MAX_RECORD_BYTES + 1)

    def test_only_the_record_types_the_host_handles_are_written(self) -> None:
        image = tool.stamp(None, payload(500), (1, 0, 0), 0, None)
        types = {line[7:9] for line in tool.write_hex(image, 16).splitlines()}
        self.assertEqual({"00", "01", "04"}, types)

    def test_starts_with_an_address_record_and_ends_with_eof(self) -> None:
        lines = tool.write_hex(
            tool.stamp(None, payload(50), (1, 0, 0), 0, None), 16
        ).splitlines()
        self.assertEqual(":020000040000FA", lines[0])
        self.assertEqual(":00000001FF", lines[-1])

    def test_an_image_over_64_kib_gets_a_new_address_record_and_no_wrapped_record(
        self,
    ) -> None:
        image = tool.stamp(None, payload(70000), (1, 0, 0), 0, None)
        text = tool.write_hex(image, 29)
        self.assertEqual(image, tool.flatten(tool.read_hex(text)))
        self.assertIn(":020000040001F9", text.splitlines())

    def test_start_address_records_are_dropped_not_forwarded(self) -> None:
        text = ":0400000500001234B1\n:00000001FF\n"
        with self.assertRaises(tool.ImageError):
            tool.flatten(tool.read_hex(text))  # nothing but a start address: no data
        text = ":0400000500001234B1\n:0100000055AA\n:00000001FF\n"
        self.assertEqual(b"\x55", tool.flatten(tool.read_hex(text)))

    def test_bad_checksum_and_unsupported_types_are_rejected(self) -> None:
        with self.assertRaises(tool.ImageError):
            tool.read_hex(":0100000055AB\n:00000001FF\n")
        with self.assertRaises(tool.ImageError):
            tool.read_hex(":020000020000FC\n:00000001FF\n")  # extended segment address
        with self.assertRaises(tool.ImageError):
            tool.read_hex(":0100000055AA\n")  # no EOF


class CommandTests(unittest.TestCase):
    def test_stamp_verify_info_through_main(self) -> None:
        with tempfile.TemporaryDirectory() as d:
            src, out = Path(d, "fw.bin"), Path(d, "fw.hex")
            src.write_bytes(payload(200))
            self.assertEqual(
                0,
                tool.main(
                    ["stamp", "--bin", str(src), "-o", str(out), "--version", "1.4"]
                ),
            )
            self.assertEqual(0, tool.main(["verify", str(out)]))
            self.assertEqual(0, tool.main(["info", str(out)]))
            # Change one data byte in a record without fixing its checksum.
            lines = out.read_text().splitlines()
            first = lines[3][9:11]
            lines[3] = lines[3][:9] + ("00" if first != "00" else "01") + lines[3][11:]
            out.write_text("\n".join(lines) + "\n")
            self.assertEqual(1, tool.main(["verify", str(out)]))

    def test_version_parsing(self) -> None:
        self.assertEqual((1, 2, 0), tool.parse_version("1.2"))
        self.assertEqual((1, 2, 3), tool.parse_version("1.2.3"))
        for bad in ("1", "1.x", "1.2.3.4", "256.0"):
            with self.assertRaises(Exception):
                tool.parse_version(bad)


if __name__ == "__main__":
    unittest.main()
