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
"""Scores kick-gesture against a labelled set of landmark recordings.

For every clip the runner replays the landmark file through `kick-gesture --replay`, collects the events, and matches them
to the labels. An event counts for a label when its type is the label's gesture and its time falls between
start_ms - tolerance and end_ms + tolerance. Each label is matched by at most one event. An event that matches nothing is a
false trigger. The result is, per gesture:

    recall      matched labels / labels
    precision   matched events / all events of the type

and for the whole set:

    false triggers per 10 minutes of negative footage (clips with no labels, all events in them are false)

pinch_change is a continuous stream and is not scored.

The acceptance floor from the issue is 95 % recall per gesture and under 1 false trigger per 10 minutes of negative
clips. It is applied with --min-recall and --max-fp-per-10min, and the exit status is 1 when a floor is missed, so the
tool can gate a data set in CI.

Label file (labels.json):

    {"version": 1,
     "clips": [
       {"file": "clips/a.jsonl",
        "labels": [{"gesture": "swipe_right", "start_ms": 10200, "end_ms": 10550}],
        "duration_ms": 12000}          # optional, defaults to last t_ms minus first t_ms of the clip
     ]}

A clip's "file" is relative to the label file. Times are the t_ms values of the landmark record, the same clock the events use.

Usage:
    gesture_eval.py labels.json --binary build/kick-gesture [--config gesture.json]
                    [--tolerance-ms 400] [--min-recall 0.95] [--max-fp-per-10min 1.0] [--json out.json]
    gesture_eval.py labels.json --events-dir DIR     # events already produced, one <clip stem>.events.jsonl per clip
"""
import argparse
import json
import os
import subprocess
import sys

SCORED = ["swipe_left", "swipe_right", "palm_hold", "fist", "pinch_start", "pinch_end"]


def read_events(path):
    events = []
    with open(path) as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            try:
                e = json.loads(line)
            except ValueError as err:
                raise SystemExit(f"{path}:{n}: not JSON: {err}")
            if e.get("type") == "status":
                continue
            events.append(e)
    return events


def clip_duration_ms(landmarks_path):
    first = last = None
    with open(landmarks_path) as f:
        for line in f:
            line = line.strip()
            if not line or '"t_ms"' not in line:
                continue
            t = json.loads(line).get("t_ms")
            if first is None:
                first = t
            last = t
    return 0 if first is None else last - first


def run_replay(binary, config, landmarks_path):
    cmd = [binary, "--replay", landmarks_path]
    if config:
        cmd += ["--gesture-config", config]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"{' '.join(cmd)} failed ({r.returncode}): {r.stderr.strip()}")
    events = []
    for line in r.stdout.splitlines():
        if line.strip():
            e = json.loads(line)
            if e.get("type") != "status":
                events.append(e)
    return events


def score_clip(events, labels, tolerance_ms):
    """Returns (matched_label_flags, matched_event_flags). Greedy, in time order, one event per label."""
    label_hit = [False] * len(labels)
    event_hit = [False] * len(events)
    for ei, e in enumerate(events):
        if e["type"] not in SCORED:
            event_hit[ei] = True  # not scored: neither a hit nor a false trigger
            continue
        for li, lab in enumerate(labels):
            if label_hit[li] or lab["gesture"] != e["type"]:
                continue
            if lab["start_ms"] - tolerance_ms <= e["t_ms"] <= lab["end_ms"] + tolerance_ms:
                label_hit[li] = True
                event_hit[ei] = True
                break
    return label_hit, event_hit


def evaluate(spec, base_dir, get_events, tolerance_ms):
    stats = {g: {"labels": 0, "tp": 0, "events": 0, "false": 0} for g in SCORED}
    negative_ms = 0
    negative_false = 0
    for clip in spec["clips"]:
        path = os.path.join(base_dir, clip["file"])
        labels = clip.get("labels", [])
        for lab in labels:
            if lab["gesture"] not in SCORED:
                raise SystemExit(f"{clip['file']}: gesture {lab['gesture']!r} cannot be scored (scored: {', '.join(SCORED)})")
            if lab["end_ms"] < lab["start_ms"]:
                raise SystemExit(f"{clip['file']}: label ends before it starts: {lab}")
        events = get_events(clip, path)
        label_hit, event_hit = score_clip(events, labels, tolerance_ms)
        for lab, hit in zip(labels, label_hit):
            stats[lab["gesture"]]["labels"] += 1
            stats[lab["gesture"]]["tp"] += 1 if hit else 0
        for e, hit in zip(events, event_hit):
            if e["type"] in SCORED:
                stats[e["type"]]["events"] += 1
                stats[e["type"]]["false"] += 0 if hit else 1
        if not labels:
            negative_ms += clip.get("duration_ms", clip_duration_ms(path) if os.path.exists(path) else 0)
            negative_false += sum(1 for e in events if e["type"] in SCORED)
    return stats, negative_ms, negative_false


def main(argv):
    ap = argparse.ArgumentParser(description="Per-gesture precision and recall, and false triggers per 10 minutes of negatives.")
    ap.add_argument("labels")
    ap.add_argument("--binary", help="path of kick-gesture, used to replay each clip")
    ap.add_argument("--config", help="gesture.json to replay with")
    ap.add_argument("--events-dir", help="use events already produced: <clip stem>.events.jsonl in this directory")
    ap.add_argument("--tolerance-ms", type=int, default=400)
    ap.add_argument("--min-recall", type=float, default=0.95)
    ap.add_argument("--max-fp-per-10min", type=float, default=1.0)
    ap.add_argument("--json", help="write the numbers here")
    args = ap.parse_args(argv)
    if bool(args.binary) == bool(args.events_dir):
        ap.error("give exactly one of --binary and --events-dir")

    with open(args.labels) as f:
        spec = json.load(f)
    if spec.get("version") != 1:
        raise SystemExit("labels file: version must be 1")
    base = os.path.dirname(os.path.abspath(args.labels))

    if args.binary:
        get_events = lambda clip, path: run_replay(args.binary, args.config, path)
    else:
        get_events = lambda clip, path: read_events(
            os.path.join(args.events_dir, os.path.splitext(os.path.basename(clip["file"]))[0] + ".events.jsonl"))

    stats, neg_ms, neg_false = evaluate(spec, base, get_events, args.tolerance_ms)

    print(f"{'gesture':<12} {'labels':>6} {'hit':>5} {'events':>7} {'false':>6} {'recall':>8} {'precision':>10}")
    failures = []
    result = {"gestures": {}, "tolerance_ms": args.tolerance_ms}
    for g in SCORED:
        s = stats[g]
        recall = s["tp"] / s["labels"] if s["labels"] else None
        precision = (s["events"] - s["false"]) / s["events"] if s["events"] else None
        result["gestures"][g] = {**s, "recall": recall, "precision": precision}
        r = "-" if recall is None else f"{recall:.3f}"
        p = "-" if precision is None else f"{precision:.3f}"
        print(f"{g:<12} {s['labels']:>6} {s['tp']:>5} {s['events']:>7} {s['false']:>6} {r:>8} {p:>10}")
        if recall is not None and recall < args.min_recall:
            failures.append(f"{g}: recall {recall:.3f} is below {args.min_recall}")
    minutes = neg_ms / 60000.0
    fp_rate = (neg_false / minutes * 10.0) if minutes > 0 else None
    result["negative_minutes"] = minutes
    result["negative_false_triggers"] = neg_false
    result["false_triggers_per_10min"] = fp_rate
    if fp_rate is None:
        print("\nno negative footage: false-trigger rate not measured")
        failures.append("no negative clips, so the false-trigger floor cannot be checked")
    else:
        print(f"\nnegative footage: {minutes:.1f} min, {neg_false} false triggers, {fp_rate:.2f} per 10 min")
        if fp_rate > args.max_fp_per_10min:
            failures.append(f"false triggers {fp_rate:.2f} per 10 min is above {args.max_fp_per_10min}")
    result["pass"] = not failures
    result["failures"] = failures
    if args.json:
        with open(args.json, "w") as f:
            json.dump(result, f, indent=1)
    if failures:
        print("\nFAIL")
        for line in failures:
            print("  " + line)
        return 1
    print("\nPASS")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
