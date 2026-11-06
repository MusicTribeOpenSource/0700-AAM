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
"""Unit tests for gesture_eval.py on hand-written events, no binary needed."""
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gesture_eval as ge  # noqa: E402


def ev(t, ms):
    return {"type": t, "hand": "left", "confidence": 0.9, "t_ms": ms}


class ScoreClip(unittest.TestCase):
    def test_event_inside_window_hits_once(self):
        labels = [{"gesture": "fist", "start_ms": 1000, "end_ms": 1500}]
        lh, eh = ge.score_clip([ev("fist", 1200), ev("fist", 1300)], labels, 400)
        self.assertEqual(lh, [True])
        self.assertEqual(eh, [True, False])  # the second event matches nothing

    def test_tolerance_edges(self):
        labels = [{"gesture": "fist", "start_ms": 1000, "end_ms": 1500}]
        self.assertEqual(ge.score_clip([ev("fist", 600)], labels, 400)[0], [True])
        self.assertEqual(ge.score_clip([ev("fist", 599)], labels, 400)[0], [False])
        self.assertEqual(ge.score_clip([ev("fist", 1900)], labels, 400)[0], [True])
        self.assertEqual(ge.score_clip([ev("fist", 1901)], labels, 400)[0], [False])

    def test_wrong_type_does_not_match(self):
        labels = [{"gesture": "swipe_left", "start_ms": 0, "end_ms": 100}]
        lh, eh = ge.score_clip([ev("swipe_right", 50)], labels, 400)
        self.assertEqual((lh, eh), ([False], [False]))

    def test_pinch_change_is_not_scored(self):
        lh, eh = ge.score_clip([ev("pinch_change", 5)], [], 400)
        self.assertEqual(eh, [True])


class Evaluate(unittest.TestCase):
    def run_eval(self, clips, events_by_file):
        with tempfile.TemporaryDirectory() as d:
            for c in clips:
                if "duration_ms" not in c:
                    c["duration_ms"] = 60000
            stats, ms, fp = ge.evaluate({"clips": clips}, d, lambda clip, path: events_by_file[clip["file"]], 400)
        return stats, ms, fp

    def test_precision_recall_and_negative_rate(self):
        clips = [
            {"file": "a", "labels": [{"gesture": "fist", "start_ms": 0, "end_ms": 100}, {"gesture": "fist", "start_ms": 5000, "end_ms": 5100}]},
            {"file": "neg", "labels": [], "duration_ms": 120000},
        ]
        events = {"a": [ev("fist", 50), ev("swipe_left", 3000)], "neg": [ev("palm_hold", 100), ev("fist", 200)]}
        stats, ms, fp = self.run_eval(clips, events)
        self.assertEqual(stats["fist"]["labels"], 2)
        self.assertEqual(stats["fist"]["tp"], 1)
        self.assertEqual(stats["fist"]["events"], 2)  # one in a, one in neg
        self.assertEqual(stats["fist"]["false"], 1)
        self.assertEqual(stats["swipe_left"]["false"], 1)
        self.assertEqual(ms, 120000)
        self.assertEqual(fp, 2)

    def test_unscorable_label_is_rejected(self):
        with self.assertRaises(SystemExit):
            self.run_eval([{"file": "a", "labels": [{"gesture": "pinch_change", "start_ms": 0, "end_ms": 1}]}], {"a": []})

    def test_backwards_label_is_rejected(self):
        with self.assertRaises(SystemExit):
            self.run_eval([{"file": "a", "labels": [{"gesture": "fist", "start_ms": 10, "end_ms": 1}]}], {"a": []})


class Cli(unittest.TestCase):
    def write(self, d, name, text):
        p = os.path.join(d, name)
        with open(p, "w") as f:
            f.write(text)
        return p

    def setup_set(self, d, positive_events, negative_events):
        labels = {"version": 1, "clips": [
            {"file": "pos.jsonl", "labels": [{"gesture": "fist", "start_ms": 1000, "end_ms": 1500}]},
            {"file": "neg.jsonl", "labels": [], "duration_ms": 600000}]}
        self.write(d, "labels.json", json.dumps(labels))
        os.mkdir(os.path.join(d, "ev"))
        self.write(d, "ev/pos.events.jsonl", "".join(json.dumps(e) + "\n" for e in positive_events))
        self.write(d, "ev/neg.events.jsonl", "".join(json.dumps(e) + "\n" for e in negative_events))
        return os.path.join(d, "labels.json"), os.path.join(d, "ev")

    def test_passes_and_fails_on_the_floors(self):
        with tempfile.TemporaryDirectory() as d:
            lab, evd = self.setup_set(d, [ev("fist", 1200)], [])
            self.assertEqual(ge.main([lab, "--events-dir", evd]), 0)
        with tempfile.TemporaryDirectory() as d:
            lab, evd = self.setup_set(d, [], [])
            self.assertEqual(ge.main([lab, "--events-dir", evd]), 1)  # recall 0
        with tempfile.TemporaryDirectory() as d:
            lab, evd = self.setup_set(d, [ev("fist", 1200)], [ev("fist", 5), ev("fist", 9)])
            self.assertEqual(ge.main([lab, "--events-dir", evd]), 1)  # 2 false triggers in 10 minutes
            self.assertEqual(ge.main([lab, "--events-dir", evd, "--max-fp-per-10min", "2"]), 0)

    def test_status_lines_are_ignored(self):
        with tempfile.TemporaryDirectory() as d:
            lab, evd = self.setup_set(d, [ev("fist", 1200), {"type": "status", "state": "running"}], [])
            self.assertEqual(ge.main([lab, "--events-dir", evd]), 0)

    def test_no_negative_footage_fails_the_floor(self):
        with tempfile.TemporaryDirectory() as d:
            labels = {"version": 1, "clips": [{"file": "pos.jsonl", "labels": [{"gesture": "fist", "start_ms": 0, "end_ms": 10}]}]}
            self.write(d, "labels.json", json.dumps(labels))
            os.mkdir(os.path.join(d, "ev"))
            self.write(d, "ev/pos.events.jsonl", json.dumps(ev("fist", 5)) + "\n")
            self.assertEqual(ge.main([os.path.join(d, "labels.json"), "--events-dir", os.path.join(d, "ev")]), 1)

    def test_json_output(self):
        with tempfile.TemporaryDirectory() as d:
            lab, evd = self.setup_set(d, [ev("fist", 1200)], [])
            out = os.path.join(d, "r.json")
            ge.main([lab, "--events-dir", evd, "--json", out])
            r = json.load(open(out))
            self.assertTrue(r["pass"])
            self.assertEqual(r["gestures"]["fist"]["recall"], 1.0)
            self.assertAlmostEqual(r["negative_minutes"], 10.0)


if __name__ == "__main__":
    unittest.main()
