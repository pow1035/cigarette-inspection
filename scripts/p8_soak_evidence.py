#!/usr/bin/env python3
"""Collect SDK-free P8 soak evidence for an arbitrary command.

The harness starts a fresh process for every round, assigns an empty and unique
output directory, samples resource evidence, and atomically writes a manifest.
It deliberately does not collect or infer GPU information.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import math
import os
import platform
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = "p8-soak-evidence-v1"
PROFILE_NAME = "ci-contract-v1"
PROFILE_DEFAULTS = {
    "rounds": 2,
    "restarts": 2,
    "timeoutSeconds": 30.0,
    "sampleIntervalSeconds": 0.10,
    "terminationGraceSeconds": 1.0,
    "minimumSuccessRate": 1.0,
    "maximumTimeouts": 0,
    "maximumCrashes": 0,
    "minimumDiskFreeBytes": 64 * 1024 * 1024,
    "maximumOutputBytes": 64 * 1024 * 1024,
    "rssWarmupRuns": 1,
    "maximumRssGrowthBytes": 64 * 1024 * 1024,
}


class InvocationError(ValueError):
    """Raised when execution cannot safely begin."""


def _utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).isoformat().replace("+00:00", "Z")


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            "w",
            encoding="utf-8",
            dir=path.parent,
            prefix=f".{path.name}.",
            suffix=".tmp",
            delete=False,
        ) as stream:
            temporary = stream.name
            json.dump(value, stream, ensure_ascii=False, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
        temporary = None
    finally:
        if temporary is not None:
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass


def _positive_int(text: str) -> int:
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return value


def _nonnegative_int(text: str) -> int:
    value = int(text)
    if value < 0:
        raise argparse.ArgumentTypeError("value must be nonnegative")
    return value


def _positive_float(text: str) -> float:
    value = float(text)
    if not math.isfinite(value) or value <= 0.0:
        raise argparse.ArgumentTypeError("value must be finite and positive")
    return value


def _absolute(path: Path) -> Path:
    return Path(os.path.abspath(os.fspath(path)))


def _new_evidence_root(path: Path) -> Path:
    root = _absolute(path)
    if root.exists() or root.is_symlink():
        raise InvocationError(f"evidence root must not already exist: {root}")
    parent = root.parent
    if not parent.exists() or not parent.is_dir():
        raise InvocationError(f"evidence root parent must be a directory: {parent}")
    return root


def _directory_size(paths: Iterable[Path]) -> int:
    total = 0
    for root in paths:
        try:
            info = root.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode):
            total += info.st_size
            continue
        if not stat.S_ISDIR(info.st_mode):
            continue
        for directory, directory_names, file_names in os.walk(root, followlinks=False):
            base = Path(directory)
            for name in directory_names:
                item = base / name
                try:
                    item_info = item.lstat()
                except FileNotFoundError:
                    continue
                if stat.S_ISLNK(item_info.st_mode):
                    total += item_info.st_size
            for name in file_names:
                item = base / name
                try:
                    item_info = item.lstat()
                except FileNotFoundError:
                    continue
                if stat.S_ISREG(item_info.st_mode) or stat.S_ISLNK(item_info.st_mode):
                    total += item_info.st_size
    return total


def _linux_rss_bytes(pid: int) -> int | None:
    status_path = Path("/proc") / str(pid) / "status"
    try:
        for line in status_path.read_text(encoding="ascii", errors="replace").splitlines():
            if line.startswith("VmRSS:"):
                fields = line.split()
                if len(fields) >= 2:
                    return int(fields[1]) * 1024
    except (OSError, ValueError):
        return None
    return None


def _windows_rss_bytes(pid: int) -> int | None:
    if os.name != "nt":
        return None
    try:
        import ctypes
        from ctypes import wintypes

        class ProcessMemoryCounters(ctypes.Structure):
            _fields_ = [
                ("cb", wintypes.DWORD),
                ("PageFaultCount", wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
            ]

        process_query_information = 0x0400
        process_vm_read = 0x0010
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        psapi = ctypes.WinDLL("psapi", use_last_error=True)
        handle = kernel32.OpenProcess(
            process_query_information | process_vm_read, False, pid)
        if not handle:
            return None
        try:
            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            if not psapi.GetProcessMemoryInfo(
                handle, ctypes.byref(counters), counters.cb
            ):
                return None
            return int(counters.WorkingSetSize)
        finally:
            kernel32.CloseHandle(handle)
    except (AttributeError, OSError, TypeError, ValueError):
        return None


def _ps_rss_bytes(pid: int) -> int | None:
    if os.name == "nt":
        return None
    try:
        completed = subprocess.run(
            ["ps", "-o", "rss=", "-p", str(pid)],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="ascii",
            errors="replace",
            timeout=1.0,
            check=False,
        )
        if completed.returncode == 0 and completed.stdout.strip():
            return int(completed.stdout.strip().splitlines()[0]) * 1024
    except (OSError, subprocess.TimeoutExpired, ValueError):
        return None
    return None


def _parent_process_rss(pid: int) -> tuple[int | None, str]:
    value = _linux_rss_bytes(pid)
    if value is not None:
        return value, "proc-status"
    value = _windows_rss_bytes(pid)
    if value is not None:
        return value, "windows-process-memory"
    value = _ps_rss_bytes(pid)
    if value is not None:
        return value, "ps-rss"
    return None, "not-collected"


def _resource_child_peak_bytes() -> int | None:
    try:
        import resource

        value = int(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
        if value <= 0:
            return None
        return value if platform.system() == "Darwin" else value * 1024
    except (ImportError, AttributeError, OSError, ValueError):
        return None


def _process_exists(pid: int) -> bool:
    if pid <= 0:
        return False
    if os.name == "nt":
        try:
            import ctypes

            process_query_limited_information = 0x1000
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            kernel32.OpenProcess.restype = ctypes.c_void_p
            kernel32.OpenProcess.argtypes = [
                ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
            kernel32.GetExitCodeProcess.argtypes = [
                ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
            kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
            handle = kernel32.OpenProcess(
                process_query_limited_information, False, pid)
            if not handle:
                return ctypes.get_last_error() == 5  # Access denied means unconfirmed/alive.
            exit_code = ctypes.c_ulong()
            active = (
                kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))
                and exit_code.value == 259
            )
            kernel32.CloseHandle(handle)
            return bool(active)
        except (AttributeError, OSError):
            return False
    proc_stat = Path("/proc") / str(pid) / "stat"
    try:
        fields = proc_stat.read_text(
            encoding="ascii", errors="replace").split()
        if len(fields) >= 3:
            return fields[2] != "Z"
    except OSError:
        pass
    try:
        completed = subprocess.run(
            ["ps", "-o", "stat=", "-p", str(pid)],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="ascii",
            errors="replace",
            timeout=1.0,
            check=False,
        )
        if completed.returncode != 0 or not completed.stdout.strip():
            return False
        return not completed.stdout.strip().startswith("Z")
    except (OSError, subprocess.TimeoutExpired):
        pass
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _windows_process_tree_pids(root_pid: int) -> list[int] | None:
    if os.name != "nt":
        return None
    try:
        import ctypes
        from ctypes import wintypes

        class ProcessEntry32W(ctypes.Structure):
            _fields_ = [
                ("dwSize", wintypes.DWORD),
                ("cntUsage", wintypes.DWORD),
                ("th32ProcessID", wintypes.DWORD),
                ("th32DefaultHeapID", ctypes.c_size_t),
                ("th32ModuleID", wintypes.DWORD),
                ("cntThreads", wintypes.DWORD),
                ("th32ParentProcessID", wintypes.DWORD),
                ("pcPriClassBase", wintypes.LONG),
                ("dwFlags", wintypes.DWORD),
                ("szExeFile", wintypes.WCHAR * 260),
            ]

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.CreateToolhelp32Snapshot.restype = ctypes.c_void_p
        kernel32.CreateToolhelp32Snapshot.argtypes = [
            wintypes.DWORD, wintypes.DWORD]
        kernel32.Process32FirstW.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ProcessEntry32W)]
        kernel32.Process32NextW.argtypes = [
            ctypes.c_void_p, ctypes.POINTER(ProcessEntry32W)]
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        snapshot = kernel32.CreateToolhelp32Snapshot(0x00000002, 0)
        invalid_handle = ctypes.c_void_p(-1).value
        if snapshot == invalid_handle:
            return None
        parent_by_pid: dict[int, int] = {}
        try:
            entry = ProcessEntry32W()
            entry.dwSize = ctypes.sizeof(entry)
            ctypes.set_last_error(0)
            if not kernel32.Process32FirstW(snapshot, ctypes.byref(entry)):
                return None
            while True:
                parent_by_pid[int(entry.th32ProcessID)] = int(
                    entry.th32ParentProcessID)
                ctypes.set_last_error(0)
                if kernel32.Process32NextW(snapshot, ctypes.byref(entry)):
                    continue
                if ctypes.get_last_error() != 18:  # ERROR_NO_MORE_FILES
                    return None
                break
        finally:
            kernel32.CloseHandle(snapshot)
        discovered = {root_pid}
        changed = True
        while changed:
            changed = False
            for pid, parent_pid in parent_by_pid.items():
                if parent_pid in discovered and pid not in discovered:
                    discovered.add(pid)
                    changed = True
        return sorted(discovered)
    except (AttributeError, OSError, ValueError):
        return None


def _wait_windows_pids_gone(process_ids: set[int], seconds: float) -> list[int]:
    deadline = time.monotonic() + seconds
    while True:
        residual = sorted(pid for pid in process_ids if _process_exists(pid))
        if not residual or time.monotonic() >= deadline:
            return residual
        time.sleep(min(0.025, max(0.0, deadline - time.monotonic())))


def _live_process_group_pids(group_id: int) -> list[int] | None:
    if os.name == "nt":
        return [group_id] if _process_exists(group_id) else []
    try:
        completed = subprocess.run(
            ["ps", "-axo", "pid=,pgid=,stat="],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="ascii",
            errors="replace",
            timeout=1.0,
            check=False,
        )
        if completed.returncode != 0:
            return None
        live: list[int] = []
        for line in completed.stdout.splitlines():
            fields = line.split()
            if len(fields) < 3:
                continue
            try:
                pid = int(fields[0])
                process_group = int(fields[1])
            except ValueError:
                continue
            if process_group == group_id and not fields[2].startswith("Z"):
                live.append(pid)
        return live
    except (OSError, subprocess.TimeoutExpired):
        return None


def _process_group_exists(group_id: int) -> bool:
    if os.name == "nt":
        return _process_exists(group_id)
    live_pids = _live_process_group_pids(group_id)
    if live_pids is not None:
        return bool(live_pids)
    try:
        os.killpg(group_id, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def _wait_until_gone(group_id: int, seconds: float) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if not _process_group_exists(group_id):
            return True
        time.sleep(min(0.025, max(0.0, deadline - time.monotonic())))
    return not _process_group_exists(group_id)


def _terminate_process_group(
    process: subprocess.Popen[Any],
    group_id: int,
    grace_seconds: float,
    tracked_process_ids: set[int] | None = None,
) -> dict[str, Any]:
    tracked = set(tracked_process_ids or ())
    tracked.add(process.pid)
    result: dict[str, Any] = {
        "attempted": True,
        "method": "taskkill-tree" if os.name == "nt" else "posix-process-group",
        "gracefulSignalSent": False,
        "forceSignalSent": False,
        "noResidualProcessConfirmed": False,
        "residualProcessIds": [],
        "trackedProcessIds": sorted(tracked),
        "treeEnumerationSucceeded": os.name != "nt",
    }
    if os.name == "nt":
        descendants = _windows_process_tree_pids(process.pid)
        result["treeEnumerationSucceeded"] = descendants is not None
        if descendants is not None:
            tracked.update(descendants)
        targets = [process.pid] + sorted(tracked - {process.pid})
        for target_pid in targets:
            if not _process_exists(target_pid):
                continue
            try:
                completed = subprocess.run(
                    ["taskkill", "/PID", str(target_pid), "/T", "/F"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    timeout=max(1.0, grace_seconds),
                    check=False,
                )
                result["forceSignalSent"] = (
                    result["forceSignalSent"] or completed.returncode == 0)
            except (OSError, subprocess.TimeoutExpired):
                if target_pid == process.pid:
                    try:
                        process.kill()
                        result["forceSignalSent"] = True
                    except OSError:
                        pass
        try:
            process.wait(timeout=max(0.1, grace_seconds))
        except (subprocess.TimeoutExpired, OSError):
            pass
        residual = _wait_windows_pids_gone(tracked, grace_seconds)
        result["trackedProcessIds"] = sorted(tracked)
        result["residualProcessIds"] = residual
        result["noResidualProcessConfirmed"] = (
            bool(result["treeEnumerationSucceeded"]) and not residual)
        return result

    try:
        os.killpg(group_id, signal.SIGTERM)
        result["gracefulSignalSent"] = True
    except (ProcessLookupError, PermissionError):
        pass
    if not _wait_until_gone(group_id, grace_seconds):
        try:
            os.killpg(group_id, signal.SIGKILL)
            result["forceSignalSent"] = True
        except (ProcessLookupError, PermissionError):
            pass
    try:
        process.wait(timeout=max(0.1, grace_seconds))
    except (subprocess.TimeoutExpired, OSError):
        pass
    result["noResidualProcessConfirmed"] = _wait_until_gone(group_id, grace_seconds)
    residual_pids = _live_process_group_pids(group_id)
    if residual_pids is not None:
        result["residualProcessIds"] = residual_pids
        result["noResidualProcessConfirmed"] = not residual_pids
    return result


def _expand_command(
    template: list[str],
    output_dir: Path,
    evidence_root: Path,
    restart: int,
    round_number: int,
    iteration: int,
) -> list[str]:
    replacements = {
        "{output_dir}": str(output_dir),
        "{evidence_root}": str(evidence_root),
        "{restart}": str(restart),
        "{round}": str(round_number),
        "{iteration}": str(iteration),
    }
    expanded: list[str] = []
    for argument in template:
        value = argument
        for token, replacement in replacements.items():
            value = value.replace(token, replacement)
        expanded.append(value)
    return expanded


def _relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def _file_reference(path: Path, root: Path) -> dict[str, Any]:
    return {
        "path": _relative(path, root),
        "size": path.stat().st_size,
        "sha256": _sha256_file(path),
    }


def _sample(
    pid: int,
    output_dir: Path,
    stdout_path: Path,
    stderr_path: Path,
    evidence_root: Path,
    started_monotonic: float,
) -> dict[str, Any]:
    rss, rss_source = _parent_process_rss(pid)
    try:
        disk_free = shutil.disk_usage(evidence_root).free
    except OSError:
        disk_free = None
    return {
        "sampledAt": _utc_now(),
        "elapsedSeconds": round(time.monotonic() - started_monotonic, 6),
        "parentProcessRssBytes": rss,
        "parentProcessRssSource": rss_source,
        "outputBytes": _directory_size((output_dir, stdout_path, stderr_path)),
        "diskFreeBytes": disk_free,
        "gpu": "not-collected",
    }


def _run_once(
    *,
    evidence_root: Path,
    command_template: list[str],
    cwd: Path,
    restart: int,
    round_number: int,
    iteration: int,
    timeout_seconds: float,
    sample_interval_seconds: float,
    grace_seconds: float,
    maximum_output_bytes: int,
    minimum_disk_free_bytes: int,
) -> dict[str, Any]:
    run_dir = evidence_root / "runs" / f"restart-{restart:03d}" / f"round-{round_number:03d}"
    run_dir.mkdir(parents=True, exist_ok=False)
    output_dir = run_dir / "output"
    if output_dir.exists() or output_dir.is_symlink():
        raise RuntimeError(f"run output directory is not fresh: {output_dir}")
    output_dir.mkdir()
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"
    samples_path = run_dir / "samples.json"
    result_path = run_dir / "result.json"
    command_path = run_dir / "command.json"
    argv = _expand_command(
        command_template, output_dir, evidence_root,
        restart, round_number, iteration)
    environment = os.environ.copy()
    environment.update({
        "P8_SOAK_OUTPUT_DIR": str(output_dir),
        "P8_SOAK_EVIDENCE_ROOT": str(evidence_root),
        "P8_SOAK_RESTART": str(restart),
        "P8_SOAK_ROUND": str(round_number),
        "P8_SOAK_ITERATION": str(iteration),
        "P8_SOAK_PROFILE": PROFILE_NAME,
    })
    _atomic_write_json(command_path, {
        "argv": argv,
        "cwd": str(cwd),
        "shell": False,
        "outputDirectory": str(output_dir),
        "environment": {
            key: environment[key] for key in (
                "P8_SOAK_OUTPUT_DIR",
                "P8_SOAK_EVIDENCE_ROOT",
                "P8_SOAK_RESTART",
                "P8_SOAK_ROUND",
                "P8_SOAK_ITERATION",
                "P8_SOAK_PROFILE",
            )
        },
    })

    started_at = _utc_now()
    started_monotonic = time.monotonic()
    ended_at = started_at
    exit_code: int | None = None
    timed_out = False
    launch_error = ""
    termination_reason = ""
    termination: dict[str, Any] = {
        "attempted": False,
        "method": "not-required",
        "gracefulSignalSent": False,
        "forceSignalSent": False,
        "noResidualProcessConfirmed": True,
        "residualProcessIds": [],
        "trackedProcessIds": [],
        "treeEnumerationSucceeded": os.name != "nt",
    }
    samples: list[dict[str, Any]] = []
    process: subprocess.Popen[Any] | None = None
    group_id: int | None = None
    tracked_process_ids: set[int] = set()
    child_peak_before = _resource_child_peak_bytes()

    with stdout_path.open("wb") as stdout_stream, stderr_path.open("wb") as stderr_stream:
        try:
            popen_options: dict[str, Any] = {
                "cwd": cwd,
                "env": environment,
                "stdin": subprocess.DEVNULL,
                "stdout": stdout_stream,
                "stderr": stderr_stream,
                "shell": False,
            }
            if os.name == "nt":
                popen_options["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP
            else:
                popen_options["start_new_session"] = True
            process = subprocess.Popen(argv, **popen_options)
            group_id = process.pid
            tracked_process_ids.add(process.pid)
            deadline = started_monotonic + timeout_seconds
            while True:
                if os.name == "nt":
                    descendants = _windows_process_tree_pids(process.pid)
                    if descendants is not None:
                        tracked_process_ids.update(descendants)
                current = _sample(
                    process.pid, output_dir, stdout_path, stderr_path,
                    evidence_root, started_monotonic)
                samples.append(current)
                observed_output = int(current["outputBytes"])
                observed_disk = current["diskFreeBytes"]
                if process.poll() is not None:
                    break
                if time.monotonic() >= deadline:
                    timed_out = True
                    termination_reason = "timeout"
                    termination = _terminate_process_group(
                        process, group_id, grace_seconds, tracked_process_ids)
                    break
                if observed_output > maximum_output_bytes:
                    termination_reason = "output-budget"
                    termination = _terminate_process_group(
                        process, group_id, grace_seconds, tracked_process_ids)
                    break
                if observed_disk is not None and observed_disk < minimum_disk_free_bytes:
                    termination_reason = "disk-floor"
                    termination = _terminate_process_group(
                        process, group_id, grace_seconds, tracked_process_ids)
                    break
                time.sleep(sample_interval_seconds)
            try:
                exit_code = process.wait(timeout=max(0.1, grace_seconds))
            except subprocess.TimeoutExpired:
                if group_id is not None:
                    termination_reason = termination_reason or "residual-process"
                    termination = _terminate_process_group(
                        process, group_id, grace_seconds, tracked_process_ids)
                exit_code = process.poll()
        except OSError as error:
            launch_error = f"{type(error).__name__}: {error}"
        finally:
            stdout_stream.flush()
            stderr_stream.flush()
            ended_at = _utc_now()

    if process is not None and group_id is not None:
        if os.name == "nt":
            descendants = _windows_process_tree_pids(process.pid)
            if descendants is not None:
                tracked_process_ids.update(descendants)
                termination["treeEnumerationSucceeded"] = True
                termination["trackedProcessIds"] = sorted(tracked_process_ids)
            residual_exists = bool(
                _wait_windows_pids_gone(tracked_process_ids, 0.0))
            if descendants is None:
                residual_exists = True
        else:
            residual_exists = _process_group_exists(group_id)
    else:
        residual_exists = False
    if process is not None and group_id is not None and residual_exists:
        residual_cleanup = _terminate_process_group(
            process, group_id, grace_seconds, tracked_process_ids)
        if not termination["attempted"]:
            termination = residual_cleanup
        else:
            termination["noResidualProcessConfirmed"] = (
                bool(termination["noResidualProcessConfirmed"])
                and bool(residual_cleanup["noResidualProcessConfirmed"])
            )
        termination_reason = termination_reason or "residual-process"

    if not samples:
        samples.append({
            "sampledAt": ended_at,
            "elapsedSeconds": round(time.monotonic() - started_monotonic, 6),
            "parentProcessRssBytes": None,
            "parentProcessRssSource": "not-collected",
            "outputBytes": _directory_size((output_dir, stdout_path, stderr_path)),
            "diskFreeBytes": shutil.disk_usage(evidence_root).free,
            "gpu": "not-collected",
        })
    if all(item["parentProcessRssBytes"] is None for item in samples):
        child_peak_after = _resource_child_peak_bytes()
        fallback_peak = child_peak_after
        if (fallback_peak is not None and child_peak_before is not None
                and fallback_peak < child_peak_before):
            fallback_peak = child_peak_before
        if fallback_peak is not None:
            samples[-1]["parentProcessRssBytes"] = fallback_peak
            samples[-1]["parentProcessRssSource"] = "resource-child-peak"

    final_output_bytes = _directory_size((output_dir, stdout_path, stderr_path))
    maximum_observed_output = max(
        [final_output_bytes]
        + [int(item["outputBytes"]) for item in samples]
    )
    output_budget_exceeded = maximum_observed_output > maximum_output_bytes
    disk_values = [
        int(item["diskFreeBytes"]) for item in samples
        if item["diskFreeBytes"] is not None
    ]
    minimum_observed_disk = min(disk_values) if disk_values else None
    rss_values = [
        int(item["parentProcessRssBytes"]) for item in samples
        if item["parentProcessRssBytes"] is not None
    ]
    peak_rss = max(rss_values) if rss_values else None
    crashed = (
        exit_code not in (None, 0)
        and not timed_out
        and termination_reason not in {"output-budget", "disk-floor", "residual-process"}
    )
    succeeded = (
        not launch_error
        and exit_code == 0
        and not timed_out
        and not output_budget_exceeded
        and termination_reason == ""
        and termination["noResidualProcessConfirmed"]
        and output_dir.is_dir()
    )

    _atomic_write_json(samples_path, {
        "schemaVersion": "p8-soak-samples-v1",
        "restart": restart,
        "round": round_number,
        "iteration": iteration,
        "samples": samples,
    })
    result: dict[str, Any] = {
        "restart": restart,
        "round": round_number,
        "iteration": iteration,
        "argv": argv,
        "cwd": str(cwd),
        "startedAt": started_at,
        "endedAt": ended_at,
        "durationSeconds": round(time.monotonic() - started_monotonic, 6),
        "exitCode": exit_code,
        "timedOut": timed_out,
        "crashed": crashed,
        "succeeded": succeeded,
        "launchError": launch_error,
        "terminationReason": termination_reason,
        "processGroupTermination": termination,
        "outputDirectory": _relative(output_dir, evidence_root),
        "outputDirectoryFresh": True,
        "stdout": _file_reference(stdout_path, evidence_root),
        "stderr": _file_reference(stderr_path, evidence_root),
        "samples": _relative(samples_path, evidence_root),
        "sampleCount": len(samples),
        "finalOutputBytes": final_output_bytes,
        "maximumObservedOutputBytes": maximum_observed_output,
        "outputBudgetExceeded": output_budget_exceeded,
        "minimumObservedDiskFreeBytes": minimum_observed_disk,
        "peakParentProcessRssBytes": peak_rss,
        "gpu": "not-collected",
    }
    _atomic_write_json(result_path, result)
    result["commandRecord"] = _relative(command_path, evidence_root)
    result["resultRecord"] = _relative(result_path, evidence_root)
    return result


def _evidence_files(root: Path) -> tuple[list[dict[str, Any]], list[str]]:
    records: list[dict[str, Any]] = []
    unsafe: list[str] = []
    manifest_path = root / "soak-manifest.json"
    for directory, directory_names, file_names in os.walk(root, followlinks=False):
        base = Path(directory)
        retained_directories: list[str] = []
        for name in directory_names:
            path = base / name
            if path.is_symlink():
                unsafe.append(f"symlink directory excluded: {_relative(path, root)}")
            else:
                retained_directories.append(name)
        directory_names[:] = retained_directories
        for name in file_names:
            path = base / name
            if path == manifest_path or path.name.endswith(".tmp"):
                continue
            try:
                info = path.lstat()
            except FileNotFoundError:
                unsafe.append(f"evidence file disappeared: {_relative(path, root)}")
                continue
            if stat.S_ISLNK(info.st_mode):
                unsafe.append(f"symlink file excluded: {_relative(path, root)}")
                continue
            if not stat.S_ISREG(info.st_mode):
                unsafe.append(f"non-regular evidence excluded: {_relative(path, root)}")
                continue
            records.append({
                "path": _relative(path, root),
                "size": info.st_size,
                "sha256": _sha256_file(path),
            })
    records.sort(key=lambda item: item["path"])
    return records, unsafe


def _gate(
    name: str,
    passed: bool,
    observed: Any,
    requirement: str,
) -> dict[str, Any]:
    return {
        "name": name,
        "status": "passed" if passed else "failed",
        "observed": observed,
        "requirement": requirement,
    }


def _evaluate(
    runs: list[dict[str, Any]],
    thresholds: dict[str, Any],
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    planned = int(thresholds["plannedRuns"])
    successful = sum(bool(item["succeeded"]) for item in runs)
    timeouts = sum(bool(item["timedOut"]) for item in runs)
    crashes = sum(bool(item["crashed"]) for item in runs)
    success_rate = successful / planned if planned else 0.0
    total_output = sum(int(item["finalOutputBytes"]) for item in runs)
    disk_values = [
        int(item["minimumObservedDiskFreeBytes"]) for item in runs
        if item["minimumObservedDiskFreeBytes"] is not None
    ]
    minimum_disk = min(disk_values) if disk_values else None
    peaks = [
        int(item["peakParentProcessRssBytes"]) for item in runs
        if item["peakParentProcessRssBytes"] is not None
    ]
    warmup = int(thresholds["rssWarmupRuns"])
    post_warmup = peaks[warmup:] if len(peaks) > warmup else peaks[-1:]
    rss_growth: int | None
    if post_warmup:
        rss_growth = max(0, max(post_warmup) - post_warmup[0])
    else:
        rss_growth = None
    summary = {
        "plannedRuns": planned,
        "completedRuns": len(runs),
        "successfulRuns": successful,
        "successRate": success_rate,
        "successRatePercent": success_rate * 100.0,
        "timeouts": timeouts,
        "crashes": crashes,
        "totalOutputBytes": total_output,
        "minimumObservedDiskFreeBytes": minimum_disk,
        "rssSamplesCollected": len(peaks),
        "rssWarmupRuns": warmup,
        "rssPostWarmupGrowthBytes": rss_growth,
    }
    checks = [
        _gate(
            "all-runs-completed",
            len(runs) == planned,
            len(runs),
            f"exactly {planned} runs",
        ),
        _gate(
            "success-rate",
            success_rate >= float(thresholds["minimumSuccessRate"]),
            success_rate,
            f">= {thresholds['minimumSuccessRate']}",
        ),
        _gate(
            "timeouts",
            timeouts <= int(thresholds["maximumTimeouts"]),
            timeouts,
            f"<= {thresholds['maximumTimeouts']}",
        ),
        _gate(
            "crashes",
            crashes <= int(thresholds["maximumCrashes"]),
            crashes,
            f"<= {thresholds['maximumCrashes']}",
        ),
        _gate(
            "disk-free-floor",
            minimum_disk is not None
            and minimum_disk >= int(thresholds["minimumDiskFreeBytes"]),
            minimum_disk,
            f">= {thresholds['minimumDiskFreeBytes']} bytes",
        ),
        _gate(
            "output-budget",
            total_output <= int(thresholds["maximumOutputBytes"])
            and not any(item["outputBudgetExceeded"] for item in runs),
            total_output,
            f"<= {thresholds['maximumOutputBytes']} bytes",
        ),
        _gate(
            "rss-collected",
            bool(peaks),
            len(peaks),
            "at least one parent-process RSS peak",
        ),
        _gate(
            "rss-post-warmup-growth",
            rss_growth is not None
            and rss_growth <= int(thresholds["maximumRssGrowthBytes"]),
            rss_growth,
            f"<= {thresholds['maximumRssGrowthBytes']} bytes "
            f"after {warmup} warmup run(s)",
        ),
        _gate(
            "process-groups-clean",
            all(item["processGroupTermination"]["noResidualProcessConfirmed"]
                for item in runs),
            sum(
                not item["processGroupTermination"]["noResidualProcessConfirmed"]
                for item in runs
            ),
            "0 unconfirmed residual process groups",
        ),
    ]
    return summary, checks


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run SDK-free P8 soak rounds and emit hash-bound evidence.")
    parser.add_argument("--evidence-root", required=True, type=Path)
    parser.add_argument("--profile", choices=(PROFILE_NAME,), default=PROFILE_NAME)
    parser.add_argument("--cwd", type=Path, default=Path.cwd())
    parser.add_argument("--rounds", type=_positive_int)
    parser.add_argument("--restarts", type=_positive_int)
    parser.add_argument("--timeout-seconds", type=_positive_float)
    parser.add_argument("--sample-interval-seconds", type=_positive_float)
    parser.add_argument("--termination-grace-seconds", type=_positive_float)
    parser.add_argument("--min-disk-free-bytes", type=_nonnegative_int)
    parser.add_argument("--max-output-bytes", type=_positive_int)
    parser.add_argument("--rss-warmup-runs", type=_nonnegative_int)
    parser.add_argument("--max-rss-growth-bytes", type=_nonnegative_int)
    parser.add_argument(
        "--command",
        dest="command_option",
        nargs=argparse.REMAINDER,
        help="Command argv; must be the final harness option.",
    )
    parser.add_argument(
        "command",
        nargs=argparse.REMAINDER,
        help="Command argv after --. Supports {output_dir} and run tokens.",
    )
    return parser


def _thresholds(arguments: argparse.Namespace) -> dict[str, Any]:
    rounds = arguments.rounds or int(PROFILE_DEFAULTS["rounds"])
    restarts = arguments.restarts or int(PROFILE_DEFAULTS["restarts"])
    return {
        "minimumSuccessRate": PROFILE_DEFAULTS["minimumSuccessRate"],
        "maximumTimeouts": PROFILE_DEFAULTS["maximumTimeouts"],
        "maximumCrashes": PROFILE_DEFAULTS["maximumCrashes"],
        "minimumDiskFreeBytes": (
            arguments.min_disk_free_bytes
            if arguments.min_disk_free_bytes is not None
            else PROFILE_DEFAULTS["minimumDiskFreeBytes"]
        ),
        "maximumOutputBytes": (
            arguments.max_output_bytes
            if arguments.max_output_bytes is not None
            else PROFILE_DEFAULTS["maximumOutputBytes"]
        ),
        "rssWarmupRuns": (
            arguments.rss_warmup_runs
            if arguments.rss_warmup_runs is not None
            else PROFILE_DEFAULTS["rssWarmupRuns"]
        ),
        "maximumRssGrowthBytes": (
            arguments.max_rss_growth_bytes
            if arguments.max_rss_growth_bytes is not None
            else PROFILE_DEFAULTS["maximumRssGrowthBytes"]
        ),
        "timeoutSeconds": (
            arguments.timeout_seconds
            if arguments.timeout_seconds is not None
            else PROFILE_DEFAULTS["timeoutSeconds"]
        ),
        "sampleIntervalSeconds": (
            arguments.sample_interval_seconds
            if arguments.sample_interval_seconds is not None
            else PROFILE_DEFAULTS["sampleIntervalSeconds"]
        ),
        "terminationGraceSeconds": (
            arguments.termination_grace_seconds
            if arguments.termination_grace_seconds is not None
            else PROFILE_DEFAULTS["terminationGraceSeconds"]
        ),
        "roundsPerRestart": rounds,
        "restarts": restarts,
        "plannedRuns": rounds * restarts,
    }


def main(argv: list[str] | None = None) -> int:
    parser = _build_parser()
    arguments = parser.parse_args(argv)
    command_option = arguments.command_option or []
    positional_command = arguments.command or []
    if command_option and positional_command:
        parser.error("use either --command or --, not both")
    command = command_option or positional_command
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        parser.error("a command must be supplied after -- or --command")

    try:
        evidence_root = _new_evidence_root(arguments.evidence_root)
        cwd = _absolute(arguments.cwd)
        if not cwd.is_dir():
            raise InvocationError(f"working directory must exist: {cwd}")
    except InvocationError as error:
        print(f"p8 soak invocation error: {error}", file=sys.stderr)
        return 2

    thresholds = _thresholds(arguments)
    evidence_root.mkdir()
    started_at = _utc_now()
    runs: list[dict[str, Any]] = []
    failures: list[dict[str, Any]] = []
    iteration = 0
    try:
        for restart in range(1, int(thresholds["restarts"]) + 1):
            for round_number in range(1, int(thresholds["roundsPerRestart"]) + 1):
                iteration += 1
                result = _run_once(
                    evidence_root=evidence_root,
                    command_template=command,
                    cwd=cwd,
                    restart=restart,
                    round_number=round_number,
                    iteration=iteration,
                    timeout_seconds=float(thresholds["timeoutSeconds"]),
                    sample_interval_seconds=float(
                        thresholds["sampleIntervalSeconds"]),
                    grace_seconds=float(thresholds["terminationGraceSeconds"]),
                    maximum_output_bytes=int(thresholds["maximumOutputBytes"]),
                    minimum_disk_free_bytes=int(
                        thresholds["minimumDiskFreeBytes"]),
                )
                runs.append(result)
    except Exception as error:  # Preserve partial evidence for orchestration faults.
        failures.append({
            "kind": "orchestration-error",
            "detail": f"{type(error).__name__}: {error}",
        })

    summary, checks = _evaluate(runs, thresholds)
    evidence_files, unsafe_evidence = _evidence_files(evidence_root)
    checks.append(_gate(
        "regular-evidence-files",
        not unsafe_evidence,
        unsafe_evidence,
        "no symlink or non-regular evidence files",
    ))
    for check in checks:
        if check["status"] == "failed":
            failures.append({
                "kind": "gate-failure",
                "gate": check["name"],
                "observed": check["observed"],
                "requirement": check["requirement"],
            })
    overall_passed = not failures
    manifest = {
        "schemaVersion": SCHEMA_VERSION,
        "profile": {
            "name": arguments.profile,
            "durationClass": "short",
            "claimScope": "sdk-free-local-contract-only",
            "thresholds": thresholds,
        },
        "startedAt": started_at,
        "endedAt": _utc_now(),
        "overallResult": "passed-local-tooling" if overall_passed else "failed",
        "sdkFree": True,
        "productAcceptanceClaimed": False,
        "windowsRuntimeVerified": False,
        "gpu": "not-collected",
        "gpuClaimed": False,
        "commandTemplate": command,
        "cwd": str(cwd),
        "summary": summary,
        "checks": checks,
        "runs": runs,
        "failures": failures,
        "evidenceFiles": evidence_files,
        "manifestExcludedFromSelfHash": True,
    }
    _atomic_write_json(evidence_root / "soak-manifest.json", manifest)
    if overall_passed:
        print(f"P8 SDK-free soak passed: {evidence_root}")
        return 0
    print(f"P8 SDK-free soak failed: {evidence_root}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
