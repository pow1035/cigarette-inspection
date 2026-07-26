#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
profile="local-sdkfree-v1"
evidence_root=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --profile)
      if [[ $# -lt 2 || -z "${2:-}" ]]; then
        echo "ERROR --profile requires a value" >&2
        exit 2
      fi
      profile="${2:-}"
      shift 2
      ;;
    --evidence-root)
      if [[ $# -lt 2 || -z "${2:-}" ]]; then
        echo "ERROR --evidence-root requires a value" >&2
        exit 2
      fi
      evidence_root="${2:-}"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 --evidence-root PATH [--profile local-sdkfree-v1|contract-test-v1]"
      exit 0
      ;;
    *)
      echo "ERROR unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$evidence_root" ]]; then
  echo "ERROR --evidence-root is required" >&2
  exit 2
fi
if [[ "$profile" != "local-sdkfree-v1" && "$profile" != "contract-test-v1" ]]; then
  echo "ERROR unsupported profile: $profile" >&2
  exit 2
fi
for tool in python3 g++ ps; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR required tool unavailable: $tool" >&2
    exit 2
  fi
done
compiler="$(command -v g++)"

python3 "$repo_root/scripts/p8_continuous_soak.py" run \
  --evidence-root "$evidence_root" \
  --profile "$profile" \
  --cwd "$repo_root" \
  --compiler "$compiler" \
  --standard c++17 \
  -- \
  --output-dir '{output_dir}' \
  --duration-seconds '{minimum_duration_seconds}' \
  --frames-per-session 512 \
  --restart '{restart}' \
  --round '{round}' \
  --iteration '{iteration}'
