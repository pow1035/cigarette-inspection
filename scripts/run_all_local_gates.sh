#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mode="full"
case "${1:-}" in
  "")
    ;;
  --core)
    mode="core"
    ;;
  --full)
    ;;
  -h|--help)
    cat <<'EOF'
Usage: ./scripts/run_all_local_gates.sh [--core|--full]

  --core  Documents, static invariants, exact Python suites, Python syntax,
          strict C++17 regressions, and standalone-header builds.
  --full  Core gates plus C++14 compatibility, simulation repetition, and
          ASan/UBSan runs including the continuous-soak runtime. This is the default.
EOF
    exit 0
    ;;
  *)
    echo "ERROR unknown argument: $1" >&2
    exit 2
    ;;
esac

for tool in python3 g++ rg xmllint git; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR required local-gate tool is unavailable: $tool" >&2
    exit 2
  fi
done

gate_tmp_parent="${TMPDIR:-/tmp}"
gate_build_dir="$(mktemp -d "$gate_tmp_parent/cigvision-local-gates.XXXXXX")"
cleanup_gate_build() {
  if [[ -n "${gate_build_dir:-}" &&
        -d "$gate_build_dir" &&
        "$(basename "$gate_build_dir")" == cigvision-local-gates.* ]]; then
    rm -rf -- "$gate_build_dir"
  fi
}
trap cleanup_gate_build EXIT

run_python_suite() {
  local suite_dir="$1"
  local expected_count="$2"
  python3 - "$suite_dir" "$expected_count" <<'PY'
import sys
import unittest

suite_dir = sys.argv[1]
expected = int(sys.argv[2])
suite = unittest.defaultTestLoader.discover(suite_dir, pattern="test_*.py")
observed = suite.countTestCases()
print(f"DISCOVER {suite_dir}: {observed} test(s)")
if observed != expected:
    raise SystemExit(
        f"ERROR {suite_dir}: expected exactly {expected} tests, discovered {observed}"
    )
result = unittest.TextTestRunner(verbosity=1).run(suite)
if not result.wasSuccessful():
    raise SystemExit(1)
PY
}

echo "== Documentation and static invariants =="
./scripts/validate_project_docs.sh
./scripts/validate_p1_static.sh

echo "== Exact Python regressions =="
PYTHONDONTWRITEBYTECODE=1 run_python_suite tests/p5 100
PYTHONDONTWRITEBYTECODE=1 run_python_suite tests/p6 17
PYTHONDONTWRITEBYTECODE=1 run_python_suite tests/p8 81

echo "== Python syntax without bytecode artifacts =="
python3 - <<'PY'
from pathlib import Path
import subprocess

completed = subprocess.run(
    ["rg", "--files", "scripts", "tests", "-g", "*.py"],
    check=True,
    text=True,
    stdout=subprocess.PIPE,
)
paths = sorted(Path(line) for line in completed.stdout.splitlines() if line)
if not paths:
    raise SystemExit("ERROR no Python sources discovered")
for path in paths:
    compile(path.read_text(encoding="utf-8-sig"), str(path), "exec")
print(f"PASS Python syntax: {len(paths)} file(s)")
PY

source_include="01_上位机_QT_新版_CigVision/源码"
cxx17_flags=(
  -std=c++17
  -Wall
  -Wextra
  -Wpedantic
  -Werror
  -pthread
  "-I$source_include"
)

echo "== Strict C++17 SDK-free regressions =="
g++ "${cxx17_flags[@]}" \
  tests/CigVision.Contracts/ContractTests.cpp \
  -o "$gate_build_dir/contracts-cxx17"
g++ "${cxx17_flags[@]}" \
  tests/CigVision.Offline/OfflineTests.cpp \
  -o "$gate_build_dir/offline-cxx17"
g++ "${cxx17_flags[@]}" \
  tests/CigVision.Simulation/SimulationTests.cpp \
  -o "$gate_build_dir/simulation-cxx17"
g++ "${cxx17_flags[@]}" \
  tests/CigVision.ProductState/ProductStateTests.cpp \
  -o "$gate_build_dir/product-state-cxx17"
"$gate_build_dir/contracts-cxx17"
"$gate_build_dir/offline-cxx17"
"$gate_build_dir/simulation-cxx17"
"$gate_build_dir/product-state-cxx17"
./scripts/run_p8_local_continuous_soak.sh \
  --profile contract-test-v1 \
  --evidence-root "$gate_build_dir/local-soak-cxx17-evidence"
python3 scripts/p8_continuous_soak.py verify \
  --evidence-root "$gate_build_dir/local-soak-cxx17-evidence"

echo "== Standalone SDK-free header builds =="
header_index=0
while IFS= read -r header; do
  header_index=$((header_index + 1))
  printf '#include "%s"\nint main() { return 0; }\n' "$header" |
    g++ "${cxx17_flags[@]}" -x c++ - \
      -o "$gate_build_dir/header-$header_index"
done <<'EOF'
core/BatchCommandLine.h
core/RealtimeSimulation.h
core/RealtimeLoadSimulation.h
core/ProductRuntimeState.h
core/Sha256.h
core/ProductParameterProfile.h
adapters/tensorrt/TensorRtDetector.h
EOF
if [[ "$header_index" -ne 7 ]]; then
  echo "ERROR expected 7 standalone header builds, got $header_index" >&2
  exit 1
fi

if [[ "$mode" == "full" ]]; then
  # The profile remains locked at a 0.12 s minimum. Run the project contract
  # for 1.0 s so 100 Hz Linux CPU accounting cannot quantize work to 0.00 s.
  contract_test_duration_seconds="1.0"
  cxx14_flags=(
    -std=c++14
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -pthread
    "-I$source_include"
  )

  echo "== C++14 compatibility =="
  g++ "${cxx14_flags[@]}" \
    tests/CigVision.Simulation/SimulationTests.cpp \
    -o "$gate_build_dir/simulation-cxx14"
  g++ "${cxx14_flags[@]}" \
    tests/CigVision.ProductState/ProductStateTests.cpp \
    -o "$gate_build_dir/product-state-cxx14"
  "$gate_build_dir/simulation-cxx14"
  "$gate_build_dir/product-state-cxx14"
  python3 scripts/p8_continuous_soak.py run \
    --evidence-root "$gate_build_dir/local-soak-cxx14-evidence" \
    --profile contract-test-v1 \
    --cwd "$repo_root" \
    --compiler "$(command -v g++)" \
    --standard c++14 \
    -- \
    --output-dir '{output_dir}' \
    --duration-seconds "$contract_test_duration_seconds" \
    --frames-per-session 512 \
    --restart '{restart}' \
    --round '{round}' \
    --iteration '{iteration}'
  python3 scripts/p8_continuous_soak.py verify \
    --evidence-root "$gate_build_dir/local-soak-cxx14-evidence"

  echo "== Repeated optimized simulation =="
  g++ "${cxx17_flags[@]}" -O2 \
    tests/CigVision.Simulation/SimulationTests.cpp \
    -o "$gate_build_dir/simulation-repeat"
  for iteration in $(seq 1 20); do
    "$gate_build_dir/simulation-repeat" \
      >"$gate_build_dir/simulation-repeat-$iteration.log"
  done
  echo "PASS optimized simulation repeat: 20/20"

  echo "== ASan/UBSan simulation, product state, and continuous-soak runtime =="
  sanitizer_flags=(
    "${cxx17_flags[@]}"
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
  )
  g++ "${sanitizer_flags[@]}" \
    tests/CigVision.Simulation/SimulationTests.cpp \
    -o "$gate_build_dir/simulation-sanitized"
  g++ "${sanitizer_flags[@]}" \
    tests/CigVision.ProductState/ProductStateTests.cpp \
    -o "$gate_build_dir/product-state-sanitized"
  g++ "${sanitizer_flags[@]}" \
    tests/CigVision.LocalSoak/LocalSoakRuntime.cpp \
    -o "$gate_build_dir/local-soak-sanitized"
  mkdir "$gate_build_dir/local-soak-sanitized-output"
  if [[ "$(uname -s)" == "Darwin" ]]; then
    echo "INFO Apple ASan does not support LeakSanitizer; detect_leaks=0"
    ASAN_OPTIONS=detect_leaks=0 "$gate_build_dir/simulation-sanitized"
    ASAN_OPTIONS=detect_leaks=0 "$gate_build_dir/product-state-sanitized"
    ASAN_OPTIONS=detect_leaks=0 "$gate_build_dir/local-soak-sanitized" \
      --output-dir "$gate_build_dir/local-soak-sanitized-output" \
      --duration-seconds 0.12 --frames-per-session 128 \
      --restart 1 --round 1 --iteration 1
  else
    ASAN_OPTIONS=detect_leaks=1 "$gate_build_dir/simulation-sanitized"
    ASAN_OPTIONS=detect_leaks=1 "$gate_build_dir/product-state-sanitized"
    ASAN_OPTIONS=detect_leaks=1 "$gate_build_dir/local-soak-sanitized" \
      --output-dir "$gate_build_dir/local-soak-sanitized-output" \
      --duration-seconds 0.12 --frames-per-session 128 \
      --restart 1 --round 1 --iteration 1
  fi
  python3 - "$gate_build_dir/local-soak-sanitized-output/local-soak-summary.json" <<'PY'
import json
import sys
from pathlib import Path

document = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
if not (
    document.get("schemaVersion") == "cigvision-local-soak-runtime-v1"
    and document.get("completed") is True
    and document.get("invariantsPassed") is True
    and document.get("sdkFree") is True
    and document.get("realIoEnabled") is False
    and document.get("realRejectEnabled") is False
    and document.get("productAcceptanceClaimed") is False
    and document.get("durationSeconds", 0) >= 0.12
    and document.get("framesProcessed", 0) > 0
):
    raise SystemExit("ERROR sanitized local-soak runtime summary mismatch")
print("PASS sanitized local-soak runtime summary")
PY
fi

git diff --check
echo "PASS all local gates ($mode)"
