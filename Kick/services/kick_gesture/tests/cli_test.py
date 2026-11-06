#!/usr/bin/env python3
# Copyright (c) 2024 MUSIC Tribe
#
# This file is part of Kick.
#
# Kick is free software: you can redistribute it and/or modify it under the
# terms of the GNU Affero General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Kick is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with Kick. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Command line tests for kick-gesture and kick-gesture-synth.

Usage: cli_test.py KICK_GESTURE KICK_GESTURE_SYNTH VALIDATOR SCHEMA SHIPPED_GESTURE_JSON
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest

GESTURE, SYNTH, VALIDATOR, SCHEMA, SHIPPED = sys.argv[1:6]
del sys.argv[1:6]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


class Cli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.demo = os.path.join(cls.tmp.name, "demo.jsonl")
        r = run([SYNTH, "demo"])
        assert r.returncode == 0, r.stderr
        with open(cls.demo, "w") as f:
            f.write(r.stdout)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_version(self):
        r = run([GESTURE, "--version"])
        self.assertEqual(r.returncode, 0)
        self.assertTrue(r.stdout.startswith("kick-gesture "))

    def test_replay_events_follow_the_schema(self):
        r = run([GESTURE, "--replay", self.demo])
        self.assertEqual(r.returncode, 0, r.stderr)
        events = [json.loads(l) for l in r.stdout.splitlines()]
        types = {e["type"] for e in events}
        for want in ("palm_hold", "swipe_right", "swipe_left", "fist", "pinch_start", "pinch_end"):
            self.assertIn(want, types)
        v = run([sys.executable, "-I", VALIDATOR, SCHEMA], input=r.stdout)
        self.assertEqual(v.returncode, 0, v.stdout)

    def test_every_pinch_start_has_one_pinch_end(self):
        r = run([GESTURE, "--replay", self.demo])
        depth = 0
        for line in r.stdout.splitlines():
            t = json.loads(line)["type"]
            if t == "pinch_start":
                self.assertEqual(depth, 0)
                depth = 1
            elif t == "pinch_end":
                self.assertEqual(depth, 1)
                depth = 0
            elif t == "pinch_change":
                self.assertEqual(depth, 1)
        self.assertEqual(depth, 0)

    def test_replay_is_deterministic(self):
        a = run([GESTURE, "--replay", self.demo]).stdout
        b = run([GESTURE, "--replay", self.demo]).stdout
        self.assertEqual(a, b)

    def test_replay_to_file_sink(self):
        out = os.path.join(self.tmp.name, "events.jsonl")
        r = run([GESTURE, "--replay", self.demo, "--events-file", out])
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(out) as f:
            self.assertGreater(len(f.read().splitlines()), 0)

    def test_missing_replay_file(self):
        r = run([GESTURE, "--replay", os.path.join(self.tmp.name, "nope.jsonl")])
        self.assertEqual(r.returncode, 2)
        self.assertIn("cannot read", r.stderr)

    def test_corrupt_replay_file(self):
        p = os.path.join(self.tmp.name, "bad.jsonl")
        with open(p, "w") as f:
            f.write('{"x":\n')
        r = run([GESTURE, "--replay", p])
        self.assertEqual(r.returncode, 1)
        self.assertIn("line 1", r.stderr)

    def test_unknown_option(self):
        r = run([GESTURE, "--bogus"])
        self.assertEqual(r.returncode, 2)
        r = run([GESTURE, "--print-config", "--pinch.on", "0.9"])
        self.assertEqual(r.returncode, 2)
        self.assertIn("unknown setting", r.stderr)

    def test_live_mode_needs_a_sink(self):
        r = run([GESTURE])
        self.assertEqual(r.returncode, 2)
        self.assertIn("no event sink", r.stderr)

    def test_print_config_matches_shipped_file(self):
        r = run([GESTURE, "--print-config"])
        self.assertEqual(r.returncode, 0)
        with open(SHIPPED) as f:
            self.assertEqual(json.loads(r.stdout), json.load(f))

    def test_config_file_overrides_a_threshold(self):
        p = os.path.join(self.tmp.name, "g.json")
        with open(p, "w") as f:
            json.dump({"version": 1, "pinch": {"on": 0.25}}, f)
        r = run([GESTURE, "--print-config", "--gesture-config", p])
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout)["pinch"]["on"], 0.25)

    def test_invalid_threshold_is_rejected(self):
        p = os.path.join(self.tmp.name, "g2.json")
        with open(p, "w") as f:
            json.dump({"version": 1, "pinch": {"on": 0.9, "off": 0.3}}, f)  # on above off has no hysteresis
        r = run([GESTURE, "--print-config", "--gesture-config", p])
        self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()
