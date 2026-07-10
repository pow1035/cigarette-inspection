#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

required=(
  AGENTS.md
  docs/requirements.md
  docs/architecture.md
  docs/task-plan.md
  docs/acceptance-criteria.md
  docs/decisions.md
  docs/known-issues.md
  docs/code-audit.md
  docs/progress-log.md
  docs/qa-checklist.md
  docs/evidence-matrix.md
  docs/review-packet.md
  docs/review-results.md
  docs/golden-principles.md
  docs/observability.md
  docs/windows-build-baseline.md
)

for file in "${required[@]}"; do
  if [[ ! -s "$file" ]]; then
    echo "FAIL missing or empty: $file" >&2
    exit 1
  fi
done

if [[ "$(rg -c '<!-- CURRENT_PHASE:' docs/task-plan.md)" -ne 1 ]]; then
  echo "FAIL task plan must contain exactly one CURRENT_PHASE marker" >&2
  exit 1
fi

rg -q 'codex-long-task-architecture' AGENTS.md
rg -q 'AC-00-01' docs/acceptance-criteria.md
rg -q 'AC-00-07' docs/evidence-matrix.md
rg -q 'D-001' docs/decisions.md
rg -q 'KI-001' docs/known-issues.md

git diff --check

for script in scripts/check_windows_build_env.ps1 scripts/run_windows_p1_build.ps1; do
  if [[ ! -s "$script" ]]; then
    echo "FAIL missing or empty: $script" >&2
    exit 1
  fi
done

while IFS= read -r -d '' file; do
  status=0
  output="$(git diff --no-index --check -- /dev/null "$file" 2>&1)" || status=$?
  if [[ $status -gt 1 || -n "$output" ]]; then
    echo "FAIL whitespace check for untracked file: $file" >&2
    [[ -n "$output" ]] && echo "$output" >&2
    exit 1
  fi
done < <(git ls-files --others --exclude-standard -z -- AGENTS.md docs scripts)

current_phase="$(rg -o 'CURRENT_PHASE:[A-Z0-9]+' docs/task-plan.md | cut -d: -f2)"
restricted_paths=()
case "$current_phase" in
  P0)
    restricted_paths=(
      '01_上位机_QT_新版_CigVision'
      '02_上位机_QT_老版_传统算法'
      '03_深度学习模型与TensorRT'
      '06_硬件电路资料'
    )
    ;;
  P1|P2|P3|P5|P6|P7)
    restricted_paths=(
      '02_上位机_QT_老版_传统算法'
      '03_深度学习模型与TensorRT'
      '06_硬件电路资料'
    )
    ;;
  P4)
    restricted_paths=(
      '02_上位机_QT_老版_传统算法'
      '06_硬件电路资料'
    )
    ;;
esac

if [[ ${#restricted_paths[@]} -gt 0 ]]; then
  restricted_status="$(git status --short --untracked-files=all -- "${restricted_paths[@]}")"
  if [[ -n "$restricted_status" ]]; then
    echo "FAIL $current_phase modified a restricted reference or hardware directory:" >&2
    echo "$restricted_status" >&2
    exit 1
  fi
fi

echo "PASS project documentation structure and diff checks"
