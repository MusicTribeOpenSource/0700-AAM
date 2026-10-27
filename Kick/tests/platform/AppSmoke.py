#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Launch the actual app with isolated preferences and disabled board devices."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("app")
parser.add_argument("resources")
parser.add_argument("fixture")
parser.add_argument("--startup-only", action="store_true")
args = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="kick-app-smoke-") as directory:
    root = Path(directory)
    snapshot = root / "screen.png"
    env = dict(os.environ, KICK_DATA_ROOT=str(root), KICK_RESOURCE_ROOT=args.resources,
               KICK_NO_AUDIO_DEVICE="1", KICK_NO_SERIAL_DEVICE="1", KICK_NO_HARDWARE="1",
               KICK_SMOKE_SNAPSHOT=str(snapshot), KICK_SMOKE_CLAP_PATH="" if args.startup_only else args.fixture)
    with (root / "app.log").open("w+") as log:
        child = subprocess.Popen([args.app, "--smoke-test"], env=env, stdout=log,
                                 stderr=subprocess.STDOUT, start_new_session=True)
        try:
            code = child.wait(timeout=45)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            raise SystemExit("FAIL: KICK app exceeded 45-second watchdog")
        log.seek(0)
        output = log.read()
    print(output)
    if code != 0:
        raise SystemExit(f"FAIL: KICK app exited {code}")
    if "JUCE Assertion" in output or "Leaked objects" in output:
        raise SystemExit("FAIL: KICK reported a lifetime/assertion error")
    if not args.startup_only and "PASS: full KICK app" not in output:
        raise SystemExit("FAIL: project lifecycle checks did not complete")
    if not snapshot.is_file() or snapshot.stat().st_size < 1024 or snapshot.read_bytes()[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("FAIL: UI snapshot missing or invalid")
    print("PASS: real app rendered and shut down cleanly" if args.startup_only else
          "PASS: real app rendered, persisted CLAP state and shut down cleanly")
