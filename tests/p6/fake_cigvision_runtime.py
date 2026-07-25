#!/usr/bin/env python3
"""Executable test double for the P6 target-evidence driver."""

from __future__ import annotations

import hashlib
import json
import os
import sys
from pathlib import Path


KNOWN_OPTIONS = {
    "--offline-batch-manifest",
    "--tensorrt-batch-manifest",
    "--detector-config",
    "--offline-output",
    "--simulation-batch-manifest",
    "--simulation-output",
    "--simulation-reject-delay-micros",
    "--simulation-queue-capacity",
    "--simulation-target-output",
}
MODE = os.environ.get("P6_FAKE_MODE", "valid")


def rejected() -> int:
    return 0 if MODE == "wrong-negative-exit" else 2


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def parse_options(arguments: list[str]) -> dict[str, str] | None:
    values: dict[str, str] = {}
    index = 0
    while index < len(arguments):
        option = arguments[index]
        if option not in KNOWN_OPTIONS:
            index += 1
            continue
        if option in values or index + 1 >= len(arguments) or arguments[index + 1].startswith("--"):
            return None
        values[option] = arguments[index + 1]
        index += 2
    return values


def load_manifest(path: Path) -> tuple[dict[str, object], list[dict[str, object]], Path] | None:
    try:
        document = json.loads(path.read_text(encoding="utf-8-sig"))
        root_value = document["root"]
        samples = document["samples"]
        if not isinstance(root_value, str) or not isinstance(samples, list) or not samples:
            return None
        root = (path.parent / root_value.replace("\\", os.sep)).resolve()
        for index, sample in enumerate(samples):
            if not isinstance(sample, dict):
                return None
            source = (root / str(sample.get("path", "")).replace("\\", os.sep)).resolve()
            if not source.is_file() or digest(source) != str(sample.get("sha256", "")).upper():
                return None
            if sample.get("expected") not in (None, "OK", "NG"):
                return None
            delay = sample.get("delayBeforeMicros", 0)
            cigarette = sample.get("cigaretteNumber", index + 1)
            if isinstance(delay, bool) or not isinstance(delay, int) or delay < 0 or delay > 60_000_000:
                return None
            if isinstance(cigarette, bool) or not isinstance(cigarette, int) or cigarette <= 0:
                return None
        return document, samples, root
    except (OSError, ValueError, KeyError, TypeError):
        return None


def main() -> int:
    values = parse_options(sys.argv[1:])
    if values is None:
        return rejected()
    modes = sum(option in values for option in (
        "--offline-batch-manifest", "--tensorrt-batch-manifest",
        "--simulation-batch-manifest"))
    if modes != 1 or "--simulation-batch-manifest" not in values:
        return rejected()
    if ("--simulation-output" not in values or "--offline-output" in values or
            "--detector-config" in values):
        return rejected()
    try:
        delay = int(values.get("--simulation-reject-delay-micros", "0"))
        capacity = int(values.get("--simulation-queue-capacity", "4"))
    except ValueError:
        return rejected()
    target = values.get("--simulation-target-output", "simulation-reject").strip()
    if (delay < 0 or delay > 60_000_000 or capacity < 1 or capacity > 65_536 or
            not target or len(target.encode("utf-8")) > 256):
        return rejected()

    manifest_path = Path(values["--simulation-batch-manifest"]).resolve()
    loaded = load_manifest(manifest_path)
    if loaded is None:
        return rejected()
    document, samples, sample_root = loaded
    if MODE == "positive-exit-one":
        return 1

    output = Path(values["--simulation-output"])
    output.mkdir(parents=True, exist_ok=True)
    input_samples: list[dict[str, object]] = []
    traces: list[dict[str, object]] = []
    decisions: list[str] = []
    for index, sample in enumerate(samples, start=1):
        source = (sample_root / str(sample["path"]).replace("\\", os.sep)).resolve()
        station = str(sample.get("stationId", "offline")).strip()
        camera = str(sample.get("cameraId", source.name)).strip()
        cigarette = int(sample.get("cigaretteNumber", index))
        replay_delay = int(sample.get("delayBeforeMicros", 0))
        expected = sample.get("expected")
        decision = str(expected if expected is not None else ("NG" if index % 2 == 0 else "OK"))
        decisions.append(decision)
        input_item: dict[str, object] = {
            "sourcePath": str(source),
            "sha256": digest(source).lower(),
            "stationId": station,
            "cameraId": camera,
            "cigaretteNumber": cigarette,
            "delayBeforeMicros": replay_delay,
        }
        if expected is not None:
            input_item["expected"] = expected
        input_samples.append(input_item)

        frame = {
            "frameId": index,
            "decision": decision,
            "elapsedMicros": 10,
            "parameterVersion": "simulation-fixture-v1",
            "errorCode": "",
            "errorMessage": "",
            "stationId": station,
            "cameraId": camera,
            "sourceFile": camera,
            "cigaretteNumber": cigarette,
            "capturedAtMicros": 1_000 + index,
            "width": 2,
            "height": 2,
            "defects": [] if decision == "OK" else [{
                "classId": 0,
                "className": "fixture-ng",
                "confidence": 1.0,
                "detectorVersion": "deterministic-fixture-v1",
                "box": {"x": 0, "y": 0, "width": 1, "height": 1},
            }],
        }
        if MODE == "frame-decision-array" and index == 1:
            frame["decision"] = [decision]
        write_json(output / f"frame-{index:08d}.json", frame)
        png_prefix = b"bad-png" if MODE == "invalid-png" else b"\x89PNG\r\n\x1a\n"
        (output / f"frame-{index:08d}.png").write_bytes(
            png_prefix + b"fake-png-" + bytes([index]))

        observed = 10_000 + index * 1_000
        ng = decision == "NG"
        command = None if not ng else {
            "frameId": index,
            "cigaretteNumber": cigarette,
            "targetOutput": target,
            "scheduledAtMicros": observed + delay,
            "mode": "Simulation",
        }
        status = "SIMULATED" if ng else "SKIPPED"
        traces.append({
            "frameId": index,
            "stationId": station,
            "cameraId": camera,
            "cigaretteNumber": cigarette,
            "observedAtMicros": observed,
            "decision": decision,
            "status": status,
            "simulation": True,
            "command": command,
            "execution": {
                "frameId": index,
                "status": status,
                "completedAtMicros": observed + delay if ng else observed,
                "errorCode": "",
                "errorMessage": "",
            },
            "errorCode": "",
            "errorMessage": "",
        })

    write_json(output / "input-manifest.json", {"samples": input_samples})
    if MODE == "input-camera-mismatch":
        value = json.loads((output / "input-manifest.json").read_text(encoding="utf-8"))
        value["samples"][0]["cameraId"] = "tampered-camera"
        write_json(output / "input-manifest.json", value)

    ok_count = decisions.count("OK")
    ng_count = decisions.count("NG")
    processed = len(samples) - (1 if MODE == "summary-mismatch" else 0)
    write_json(output / "summary.json", {
        "state": 1,
        "statistics": {
            "received": len(samples),
            "processed": processed,
            "ok": ok_count,
            "ng": ng_count,
            "error": 0,
            "sourceErrors": 0,
            "detectorErrors": 0,
            "observerErrors": 0,
            "saveFailures": 0,
            "dropped": 0,
        },
        "defectsByClass": {},
        "issues": [],
    })

    trace = {
        "schemaVersion": "cigvision-simulation-trace-v1",
        "mode": "simulation",
        "simulation": True,
        "realIoEnabled": MODE == "real-io-trace",
        "manifestPath": str(manifest_path),
        "runState": 1,
        "traceCount": len(traces),
        "traceComplete": True,
        "traceValidationError": "",
        "configuration": {
            "detector": "deterministic-fixture-v1",
            "rejectMode": "Simulation",
            "simulation": True,
            "rejectDelayMicros": delay,
            "queueCapacity": capacity,
            "targetOutput": target,
            "overflowPolicy": "RejectNewest",
        },
        "statistics": {
            "received": len(samples),
            "processed": len(samples),
            "ok": ok_count,
            "ng": ng_count,
            "error": 0,
            "dropped": 0,
            "observed": len(traces),
            "ngCandidates": ng_count,
            "commands": ng_count,
            "simulated": ng_count,
            "skipped": ok_count,
            "failed": 0,
        },
        "traces": traces,
    }
    write_json(output / "simulation-trace.json", trace)
    if MODE == "missing-frame-json":
        (output / f"frame-{len(samples):08d}.json").unlink()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
