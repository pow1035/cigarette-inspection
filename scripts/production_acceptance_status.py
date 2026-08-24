#!/usr/bin/env python3
"""Summarize production-acceptance readiness without changing controlled inputs.

The command deliberately distinguishes local engineering gates from external
acceptance evidence.  It is safe to run in a fresh clone and returns exit 2
when required external inputs are absent, exit 3 for invalid evidence, and
exit 0 only when every declared gate is explicitly verified.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "production-acceptance-status-v1"
READINESS_SCRIPT = "scripts/p5_input_readiness.py"
LOCAL_GATE_SCRIPT = "scripts/run_all_local_gates.sh"


def _utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat()


def _git_value(repo_root: Path, *args: str) -> str | None:
    try:
        result = subprocess.run(
            ["git", *args], cwd=repo_root, capture_output=True, text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    value = result.stdout.strip()
    return value or None


def _run_readiness(repo_root: Path) -> tuple[int, dict[str, Any], str]:
    with tempfile.TemporaryDirectory(prefix="p5-readiness-") as temp_dir:
        output = Path(temp_dir) / "readiness.json"
        command = [
            sys.executable,
            str(repo_root / READINESS_SCRIPT),
            "--require-reviewed",
            "--require-fallback",
            "--output",
            str(output),
        ]
        result = subprocess.run(
            command, cwd=repo_root, capture_output=True, text=True,
        )
        try:
            report = json.loads(output.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise RuntimeError(
                f"readiness did not produce valid JSON: {exc}"
            ) from exc
    if not isinstance(report, dict):
        raise RuntimeError("readiness report root must be an object")
    return result.returncode, report, result.stdout.strip()


def _run_local_gate(repo_root: Path, mode: str) -> tuple[int, str]:
    command = ["bash", LOCAL_GATE_SCRIPT, f"--{mode}"]
    result = subprocess.run(command, cwd=repo_root, capture_output=True, text=True)
    output = (result.stdout + result.stderr).strip()
    return result.returncode, output[-4000:]


def build_status(repo_root: Path, *, run_local_gates: str | None = None) -> tuple[int, dict[str, Any]]:
    readiness_code, readiness, readiness_stdout = _run_readiness(repo_root)
    if readiness_code == 0 and readiness.get("ready") is True:
        readiness_status = "pass"
    elif readiness_code == 2:
        readiness_status = "blocked-external-inputs"
    elif readiness_code == 3:
        readiness_status = "blocked-invalid-inputs"
    else:
        readiness_status = "tool-error"

    gates: list[dict[str, Any]] = [{
        "id": "p5-controlled-inputs",
        "status": readiness_status,
        "evidence": READINESS_SCRIPT,
        "detail": {
            "missing_required": readiness.get("missing_required", []),
            "invalid_or_mismatched_required": readiness.get(
                "invalid_or_mismatched_required", []),
        },
    }]

    if run_local_gates is None:
        gates.append({
            "id": "local-engineering-gates",
            "status": "not-run",
            "evidence": LOCAL_GATE_SCRIPT,
            "detail": "pass --run-local-gates core or --run-local-gates full to execute",
        })
    else:
        code, output = _run_local_gate(repo_root, run_local_gates)
        gates.append({
            "id": "local-engineering-gates",
            "status": "pass" if code == 0 else "failed",
            "evidence": LOCAL_GATE_SCRIPT,
            "detail": {"mode": run_local_gates, "exit_code": code, "tail": output},
        })

    gates.extend([
        {
            "id": "formal-tensorrt-evaluation",
            "status": "blocked-unverified",
            "evidence": "Windows target execution record + TensorRT evaluation report",
            "detail": "requires reviewed truth, compatible engine/runtime, target GPU, and approved thresholds",
        },
        {
            "id": "windows-qt-gpu-deployment",
            "status": "blocked-unverified",
            "evidence": "Windows host report, deployment/rollback receipt, GPU soak evidence",
            "detail": "Linux or SDK-free evidence cannot close this gate",
        },
        {
            "id": "现场设备与安全联锁",
            "status": "blocked-unverified",
            "evidence": "现场相机/DAQNavi/IO/剔除/急停三方见证记录",
            "detail": "simulation evidence is explicitly excluded",
        },
        {
            "id": "three-party-production-signoff",
            "status": "blocked-unverified",
            "evidence": "business, site, and engineering signatures",
            "detail": "final gate remains closed until all blocking evidence is present",
        },
    ])

    blockers: list[dict[str, Any]] = []
    if readiness_status != "pass":
        blockers.append({
            "priority": "P0",
            "id": "restore-controlled-p5-artifacts",
            "owner": "数据负责人/业务批准人",
            "action": "通过受控渠道恢复 reviewed-truth 与 fallback artifact，并核对外部 evidence-manifest SHA-256",
            "depends_on": [],
        })
    blockers.extend([
        {
            "priority": "P0",
            "id": "run-formal-tensorrt",
            "owner": "算法/工程",
            "action": "冻结真值后在目标推理栈执行可复现 TensorRT 评估，记录逐类指标与阈值版本",
            "depends_on": ["restore-controlled-p5-artifacts"],
        },
        {
            "priority": "P0",
            "id": "execute-windows-target",
            "owner": "部署/工程",
            "action": "在目标 Windows/GPU 完成依赖、部署回滚、GPU 性能和 soak 证据",
            "depends_on": ["run-formal-tensorrt"],
        },
        {
            "priority": "P0",
            "id": "witness-real-io-safety",
            "owner": "现场负责人/安全负责人",
            "action": "完成真实相机、DAQNavi、IO/剔除、急停和失败安全三方见证",
            "depends_on": ["execute-windows-target"],
        },
        {
            "priority": "P0",
            "id": "sign-production-acceptance",
            "owner": "业务/现场/工程负责人",
            "action": "审查证据矩阵并完成三方签字；未验证项不得标记完成",
            "depends_on": ["witness-real-io-safety"],
        },
    ])

    status = "blocked" if blockers else "ready"
    report = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": _utc_now(),
        "repository": str(repo_root),
        "git": {
            "head": _git_value(repo_root, "rev-parse", "HEAD"),
            "branch": _git_value(repo_root, "branch", "--show-current"),
        },
        "status": status,
        "production_acceptance_claimed": False,
        "readiness_exit_code": readiness_code,
        "readiness_stdout": readiness_stdout,
        "gates": gates,
        "blockers": blockers,
        "next_actions": blockers[:2],
        "notes": [
            "Local gates and SDK-free evidence do not prove Windows, GPU, Qt, TensorRT, real IO, or safety acceptance.",
            "This report is advisory until the evidence matrix and three-party signatures are complete.",
        ],
    }
    if readiness_code not in (0, 2, 3):
        return 4, report
    if run_local_gates is not None and gates[1]["status"] != "pass":
        return 4, report
    return (0 if status == "ready" else 2), report


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--run-local-gates", choices=("core", "full"))
    args = parser.parse_args(argv)
    repo_root = args.repo_root.resolve()
    try:
        code, report = build_status(repo_root, run_local_gates=args.run_local_gates)
    except (OSError, RuntimeError, ValueError) as exc:
        print(json.dumps({"schema_version": SCHEMA_VERSION, "status": "tool-error", "error": str(exc)}))
        return 4
    payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8", newline="\n")
    print(payload, end="")
    return code


if __name__ == "__main__":
    raise SystemExit(main())
