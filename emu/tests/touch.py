#!/usr/bin/env python3
"""Touchscreen regression test: drives Kick through its control socket while the Surface holds its own connection.

Run: python3 emu/tests/touch.py [--root DIR]   (after emu/build.sh; exit code 0 = pass)

It opens Demo 01, creates a drum program through the CREATE PROGRAM dialog, opens the file browser, picks the
factory library, double-taps the sample folder and loads it. Each step fails if a tap lands on the wrong component,
which is what happens when dialogs open outside Kick's window, when table rows ignore taps, or when double taps
are not recognised. A second control connection checks the socket serves clients side by side.
"""

import argparse
import os
import pathlib
import signal
import socket
import subprocess
import sys
import tempfile
import time

REPO = pathlib.Path(__file__).resolve().parents[2]
PORT = int(os.environ.get("KICK_EMU_CONTROL_PORT", "50901"))


class Kick:
    def __init__(self):
        self.sock = socket.create_connection(("127.0.0.1", PORT), timeout=10)
        self.reader = self.sock.makefile("r")

    def send(self, line: str) -> str:
        self.sock.sendall((line + "\n").encode())
        return self.reader.readline().strip()

    def tap(self, x: int, y: int, expect: str, settle: float = 1.0) -> None:
        reply = self.send(f"tap {x} {y}")
        if expect not in reply:
            raise AssertionError(f"tap {x} {y}: expected a {expect}, got '{reply}'")
        time.sleep(settle)


def wait_for_kick(deadline: float) -> None:
    while time.time() < deadline:
        try:
            if Kick().send("ping") == "ok":
                return
        except OSError:
            pass
        time.sleep(0.5)
    raise AssertionError("Kick's control socket never answered")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=tempfile.mkdtemp(prefix="kickemu-touch-"))
    args = parser.parse_args()

    # The Surface only has to stay up and keep its own control connection; Kick is driven from here.
    script = pathlib.Path(args.root) / "touch.script"
    script.parent.mkdir(parents=True, exist_ok=True)
    script.write_text("expect connected\nwait 120000\nquit 0\n")
    run = subprocess.Popen([str(REPO / "emu/run.sh"), "--root", args.root, "--script", str(script)],
                           start_new_session=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log = pathlib.Path(args.root) / "kick.log"
    try:
        wait_for_kick(time.time() + 60)
        time.sleep(8)  # boot finishes after the socket opens
        kick = Kick()
        other = Kick()
        assert other.send("ping") == "ok", "a second control connection was not served"

        kick.tap(110, 150, "RecentItems", 3)                     # Demo 01
        kick.tap(350, 108, "KickToggleButton")                   # Program tab
        kick.tap(992, 108, "KickImageButton", 1.5)               # + opens CREATE PROGRAM
        kick.tap(334, 229, "ButtonAttachment", 0.7)              # DRUM, inside the dialog
        kick.tap(725, 518, "KickImageButton", 2)                 # Apply
        kick.tap(396, 196, "KickImageButton", 2)                 # Sample Assign on the new program
        kick.tap(726, 43, "KickImageButton", 2)                  # file browser
        kick.tap(128, 271, "KickTableView", 1.5)                 # Factory Sample Library row
        kick.tap(455, 218, "KickLabel", 0.08)                    # double tap the Emu Kit folder
        kick.tap(455, 218, "KickLabel", 1.5)
        kick.tap(652, 507, "KickButton", 3)                      # Load All

        if "Sample assigned to Pad" not in log.read_text(errors="replace"):
            raise AssertionError("Load All did not assign a sample, so the folder never opened")
        print("touch test passed")
        return 0
    except (AssertionError, OSError) as e:
        # A socket timeout means Kick stopped answering, for example because another client holds the socket.
        print(f"touch test FAILED: {e!r}", file=sys.stderr)
        return 1
    finally:
        os.killpg(run.pid, signal.SIGTERM)
        run.wait(timeout=10)


if __name__ == "__main__":
    sys.exit(main())
