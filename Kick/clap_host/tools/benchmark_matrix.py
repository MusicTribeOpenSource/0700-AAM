#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Collect bounded CLAP benchmark runs, preserving crashes/timeouts as evidence."""
import argparse
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import signal
import subprocess
import tempfile
import time


def stop(process):
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    elif process.poll() is None:
        process.kill()
    process.wait()


def run_case(command, timeout, report_path):
    """No shell or PIPE accumulation; also stop descendants on POSIX."""
    report_path = Path(report_path)
    report_path.unlink(missing_ok=True)
    started = time.monotonic()
    status = "completed"
    with tempfile.TemporaryFile() as log:
        try:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                       start_new_session=(os.name == "posix"))
        except OSError as error:
            return {"status": "launch-failed", "error": str(error), "command": command}
        try:
            while process.poll() is None:
                if time.monotonic() - started > timeout:
                    status = "timeout"
                    stop(process)
                    break
                if log.tell() > 16 * 1024 * 1024:
                    status = "output-limit"
                    stop(process)
                    break
                time.sleep(0.01)
        finally:
            # Also release the owned group on Ctrl-C or a supervisor exception.
            # Normally exited workers may have left plugin child processes.
            if os.name == "posix" or process.poll() is None:
                stop(process)
        log.seek(0)
        text = log.read(65536).decode("utf-8", errors="replace")
    result = {"status": status, "exit_code": process.returncode,
              "wall_seconds": time.monotonic() - started, "command": command, "log": text}
    if status == "completed" and process.returncode != 0:
        result["status"] = "crashed" if process.returncode < 0 else "failed"
    if report_path.is_file() and report_path.stat().st_size <= 4 * 1024 * 1024:
        try:
            report = json.loads(report_path.read_text(encoding="utf-8"))
            if not isinstance(report, dict) or report.get("schema") != 1 or not isinstance(report.get("completed"), bool):
                raise ValueError("Unrecognized benchmark report")
            result["report"] = report
            if result["status"] == "completed" and not report["completed"]:
                result["status"] = "incomplete"
        except (OSError, ValueError) as error:
            result["report_error"] = str(error)
    if result["status"] == "completed" and "report" not in result:
        result["status"] = "invalid-report"
    return result


def positive_list(value):
    try:
        numbers = [int(item) for item in value.split(",")]
        if not numbers or any(item <= 0 for item in numbers):
            raise ValueError()
        return numbers
    except ValueError:
        raise argparse.ArgumentTypeError("Use comma-separated positive integers")


def file_hash(path):
    path = Path(path)
    if not path.is_file():
        return None  # macOS .clap bundles are directories
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def atomic_json(path, value):
    path = Path(path)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, delete=False) as stream:
        temporary = Path(stream.name)
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bench", required=True, help="Path to kick-clap-bench or kick-clap-device-bench")
    parser.add_argument("--plugin", required=True)
    parser.add_argument("--id", required=True)
    parser.add_argument("--frames", type=positive_list, default=[64, 128, 256])
    parser.add_argument("--instances", type=positive_list, default=[1, 4, 8])
    parser.add_argument("--notes", type=int, default=8)
    parser.add_argument("--seconds", type=float, default=30)
    parser.add_argument("--warmup", type=float, default=3)
    parser.add_argument("--timeout", type=float, help="Per-case wall deadline; default duration + warmup + 30 seconds")
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--parameter", type=int)
    parser.add_argument("--automation-hz", type=float, default=30)
    parser.add_argument("--snapshot-ms", type=int, default=0)
    parser.add_argument("--audio", action="store_true")
    parser.add_argument("--driver")
    parser.add_argument("--device")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if not all(math.isfinite(v) for v in (args.seconds, args.warmup, args.automation_hz)) or args.seconds <= 0 or args.warmup < 0:
        parser.error("Durations and rates must be finite; duration must be positive")
    timeout = args.timeout if args.timeout is not None else args.seconds + args.warmup + 30
    if not math.isfinite(timeout) or timeout <= 0:
        parser.error("Timeout must be finite and positive")
    if len(args.frames) * len(args.instances) > 256:
        parser.error("At most 256 cases per matrix")
    destination = Path(args.output).resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    run = {"schema": 1, "recorded_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
           "machine": {"system": platform.system(), "release": platform.release(),
                       "architecture": platform.machine(), "processor": platform.processor(), "logical_cpus": os.cpu_count()},
           "benchmark_sha256": file_hash(args.bench), "plugin_sha256": file_hash(args.plugin), "cases": []}
    with tempfile.TemporaryDirectory(prefix="kick-clap-matrix-") as temporary:
        for frames in args.frames:
            for instances in args.instances:
                report = Path(temporary) / "case.json"
                command = [str(Path(args.bench).resolve()), "--plugin", str(Path(args.plugin).resolve()), "--id", args.id,
                           "--rate", str(args.rate), "--frames", str(frames), "--instances", str(instances),
                           "--notes", str(args.notes), "--seconds", str(args.seconds), "--warmup", str(args.warmup),
                           "--automation-hz", str(args.automation_hz), "--snapshot-ms", str(args.snapshot_ms), "--output", str(report)]
                if args.parameter is not None:
                    command += ["--parameter", str(args.parameter)]
                if args.audio:
                    command += ["--audio"]
                if args.driver:
                    command += ["--driver", args.driver]
                if args.device:
                    command += ["--device", args.device]
                result = run_case(command, timeout, report)
                result.update({"frames": frames, "instances": instances})
                run["cases"].append(result)
                atomic_json(destination, run)  # completed cases survive a later interruption
                print(f"frames={frames} instances={instances}: {result['status']}", flush=True)
    return 0 if all(case["status"] == "completed" for case in run["cases"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
