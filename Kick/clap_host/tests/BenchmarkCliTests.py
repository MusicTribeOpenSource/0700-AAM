# SPDX-License-Identifier: AGPL-3.0-or-later
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True

bench, normal, crash, hang, matrix = sys.argv[1:]
base = [bench, "--plugin", normal, "--id", "org.kick.test.synth", "--seconds", ".1", "--warmup", ".02"]
with tempfile.TemporaryDirectory(prefix="kick-clap-cli-tests-") as temporary:
    report_path = Path(temporary) / 'report "with spaces".json'
    result = subprocess.run(base + ["--instances", "2", "--parameter", "7", "--snapshot-ms", "10", "--output", str(report_path)], capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr.decode()
    report = json.loads(report_path.read_text())
    assert report["load_us"] > 0
    assert report["completed"] and report["mode"] == "offline" and report["device"] is None
    assert report["render"]["frames"] == 4800 and report["render"]["peak_amplitude"] > 0
    assert report["control"]["parameter_changes"] > 0 and report["control"]["snapshots"] > 0
    assert report["settings"]["instances"] == 2 and report["control"]["snapshot_bytes"] > 0
    for arguments in (["--frames", "0"], ["--seconds", "nan"], ["--seconds", "inf"], ["--instances", "-1"], ["--notes", "2.5"], ["--audio"], ["--bad", "1"]):
        assert subprocess.run(base + arguments, capture_output=True, timeout=10).returncode != 0, arguments
    result = subprocess.run(base + ["--id", "missing.id", "--output", str(report_path)], capture_output=True, timeout=10)
    assert result.returncode == 1 and not json.loads(report_path.read_text())["completed"]
    result = subprocess.run(base + ["--id", "org.kick.test.save-failure", "--parameter", "7", "--seconds", "1", "--snapshot-ms", "10", "--output", str(report_path)], capture_output=True, timeout=10)
    assert result.returncode == 1 and json.loads(report_path.read_text())["control"]["snapshot_failures"] > 0
    spec = importlib.util.spec_from_file_location("benchmark_matrix", matrix)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for binary, expected in ((crash, "crashed"), (hang, "timeout")):
        command = [bench, "--plugin", binary, "--id", "org.kick.test.synth", "--seconds", ".1", "--warmup", "0", "--output", str(report_path)]
        result = module.run_case(command, 1, report_path)
        assert result["status"] == expected, result
    result = module.run_case([bench, "--help"], 2, report_path)
    assert result["status"] == "invalid-report" and "report" not in result, "stale report must not survive a failed/missing write"
    # Interrupting the supervisor must not leave the plugin process alive.
    original_popen = module.subprocess.Popen
    original_sleep = module.time.sleep
    children = []
    def capture(*args, **kwargs):
        child = original_popen(*args, **kwargs)
        children.append(child)
        return child
    def interrupt(_):
        raise KeyboardInterrupt()
    try:
        module.subprocess.Popen = capture
        module.time.sleep = interrupt
        try:
            module.run_case([sys.executable, "-c", "import time; time.sleep(20)"], 5, report_path)
            raise AssertionError("Expected supervisor interruption")
        except KeyboardInterrupt:
            assert children and children[0].poll() is not None, "interrupted job must be reaped"
    finally:
        module.subprocess.Popen = original_popen
        module.time.sleep = original_sleep
    matrix_output = Path(temporary) / "matrix.json"
    result = subprocess.run([sys.executable, matrix, "--bench", bench, "--plugin", normal, "--id", "org.kick.test.synth", "--frames", "64,128", "--instances", "1,2", "--seconds", ".02", "--warmup", "0", "--output", str(matrix_output)], capture_output=True, timeout=10)
    assert result.returncode == 0, result.stderr.decode()
    collected = json.loads(matrix_output.read_text())
    assert len(collected["cases"]) == 4 and all(case["status"] == "completed" for case in collected["cases"])
    assert len(collected["benchmark_sha256"]) == 64 and collected["machine"]["architecture"]
print("PASS: JSON reports, invalid inputs, snapshot failure, crash/timeout retention, stale-report rejection and buffer/instance matrix")
