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
  docs/p7-ui-inventory.md
  docs/p7-parameter-profile.md
  docs/p8-local-closure.md
  docs/windows-target-execution.md
  docs/windows-build-baseline.md
  requirements-p5.txt
  .github/workflows/p5-local-gates.yml
)

for file in "${required[@]}"; do
  if [[ ! -s "$file" ]]; then
    echo "FAIL missing or empty: $file" >&2
    exit 1
  fi
done

historical_tensorrt_dir="03_深度学习模型与TensorRT/推理源码_expert_test"
historical_tensorrt_doc_count=0
while IFS= read -r historical_doc; do
  historical_tensorrt_doc_count=$((historical_tensorrt_doc_count + 1))
  if [[ ! -s "$historical_doc" ]]; then
    echo "FAIL missing or empty historical TensorRT document: $historical_doc" >&2
    exit 1
  fi
  rg -q '<!-- HISTORICAL_TENSORRT_PROTOTYPE -->' "$historical_doc"
  rg -q '历史原型资料.*不是当前构建、性能或交付证据' "$historical_doc"
  rg -q '\[README\]\(\.\./\.\./README\.md\)' "$historical_doc"
done < <(find "$historical_tensorrt_dir" -maxdepth 1 -type f -name '*.md' -print |
  LC_ALL=C sort)
if [[ "$historical_tensorrt_doc_count" -eq 0 ]]; then
  echo "FAIL no historical TensorRT Markdown documents discovered" >&2
  exit 1
fi
rg -q 'KI-022 .* 已修复 ' docs/known-issues.md
rg -q 'KI-028 .* 验证中（源配置已修复） ' docs/known-issues.md

for file in scripts/p5_input_readiness.py tests/p5/test_p5_input_readiness.py \
    scripts/p6_simulation_preflight.py tests/p6/test_p6_simulation_preflight.py \
    scripts/p6_windows_simulation_evidence.py tests/p6/test_p6_windows_simulation_evidence.py \
    tests/p6/fake_cigvision_runtime.py scripts/run_windows_p6_simulation.ps1 \
    scripts/p8_package_manifest.py scripts/p8_preflight.py \
    scripts/p8_soak_evidence.py scripts/p8_release.py \
    scripts/p8_windows_evidence_verify.py \
    scripts/collect_windows_p8_host_reports.ps1 \
    scripts/validate_powershell_scripts.ps1 \
    tests/powershell/test_validate_powershell_scripts.ps1 \
    scripts/run_all_local_gates.sh \
    scripts/run_windows_p8_preacceptance.ps1 config/p8-local-gates-v1.json \
    tests/p8/test_p8_package_manifest.py tests/p8/test_p8_preflight.py \
    tests/p8/test_p8_soak.py \
    tests/p8/test_p8_release.py tests/p8/test_p8_windows_wrapper.py \
    tests/p8/test_p8_windows_evidence_verify.py \
    01_上位机_QT_新版_CigVision/源码/core/BatchCommandLine.h \
    01_上位机_QT_新版_CigVision/源码/core/RealtimeSimulation.h \
    01_上位机_QT_新版_CigVision/源码/core/RealtimeLoadSimulation.h \
    01_上位机_QT_新版_CigVision/源码/core/Sha256.h \
    01_上位机_QT_新版_CigVision/源码/core/ProductParameterProfile.h \
    01_上位机_QT_新版_CigVision/源码/core/ProductRuntimeState.h \
    01_上位机_QT_新版_CigVision/源码/adapters/tensorrt/TensorRtDetector.h \
    01_上位机_QT_新版_CigVision/源码/config.ini \
    01_上位机_QT_新版_CigVision/源码/品牌设置/硬特醇/para.ini \
    tests/CigVision.Simulation/SimulationTests.cpp \
    tests/CigVision.Simulation/CigVision.Simulation.vcxproj \
    tests/CigVision.ProductState/ProductStateTests.cpp \
    tests/CigVision.ProductState/CigVision.ProductState.vcxproj; do
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
rg -q '^Pillow==11\.3\.0$' requirements-p5.txt
rg -q 'p5_input_readiness\.py' README.md docs/task-plan.md docs/evidence-matrix.md
rg -q 'RealtimeSimulation\.h' scripts/run_all_local_gates.sh
rg -q 'RealtimeLoadSimulation\.h' scripts/run_all_local_gates.sh
rg -q 'ProductRuntimeState\.h' scripts/run_all_local_gates.sh
rg -q 'ProductParameterProfile\.h' scripts/run_all_local_gates.sh
rg -q 'TensorRtDetector\.h' scripts/run_all_local_gates.sh
rg -q '^\[General\]$' 01_上位机_QT_新版_CigVision/源码/config.ini
rg -q 'applicationDirPath' 01_上位机_QT_新版_CigVision/源码/CigVisionParams.cpp
rg -q 'SeedRuntimeConfiguration' 01_上位机_QT_新版_CigVision/源码/CigVision.vcxproj
if rg -q '[A-Za-z]:[/\\]' 01_上位机_QT_新版_CigVision/源码/CigVision.ui; then
  echo "FAIL CigVision.ui contains a developer-machine absolute path" >&2
  exit 1
fi
rg -q 'CigVision\.ProductState' scripts/run_all_local_gates.sh
rg -q 'a973097f62c125cab024aa3db40ce304bb0e7ce57b22e1565daf5cf5032605a6' \
  docs/p7-parameter-profile.md
rg -q 'p6_simulation_preflight\.py' README.md docs/task-plan.md docs/evidence-matrix.md
rg -q 'p6_windows_simulation_evidence\.py' README.md docs/task-plan.md docs/evidence-matrix.md
rg -q 'run_windows_p6_simulation\.ps1' README.md docs/task-plan.md docs/evidence-matrix.md
rg -q 'run_python_suite tests/p6 17' scripts/run_all_local_gates.sh
rg -q 'p8_preflight\.py' README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/p8-local-closure.md
rg -q 'p8_package_manifest\.py' README.md docs/task-plan.md \
  docs/evidence-matrix.md docs/p8-local-closure.md \
  docs/windows-target-execution.md
rg -q 'p8_soak_evidence\.py' README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/p8-local-closure.md
rg -q 'p8_release\.py' README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/p8-local-closure.md
rg -q 'p8_windows_evidence_verify\.py' README.md docs/task-plan.md \
  docs/evidence-matrix.md docs/p8-local-closure.md
rg -q 'collect_windows_p8_host_reports\.ps1' README.md docs/task-plan.md \
  docs/evidence-matrix.md docs/p8-local-closure.md \
  docs/windows-target-execution.md
rg -q 'run_windows_p8_preacceptance\.ps1' README.md docs/task-plan.md \
  docs/evidence-matrix.md docs/p8-local-closure.md
rg -q 'run_all_local_gates\.sh --core' .github/workflows/p5-local-gates.yml
python3 - .github/workflows/p5-local-gates.yml <<'PY'
import sys
from pathlib import Path

import yaml


class UniqueKeyLoader(yaml.SafeLoader):
    pass


def construct_unique_mapping(loader, node, deep=False):
    loader.flatten_mapping(node)
    mapping = {}
    for key_node, value_node in node.value:
        key = loader.construct_object(key_node, deep=deep)
        if key in mapping:
            raise yaml.constructor.ConstructorError(
                "while constructing a mapping",
                node.start_mark,
                f"found duplicate key {key!r}",
                key_node.start_mark,
            )
        mapping[key] = loader.construct_object(value_node, deep=deep)
    return mapping


UniqueKeyLoader.add_constructor(
    yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG,
    construct_unique_mapping,
)

workflow = Path(sys.argv[1])
references = []
try:
    document = yaml.load(
        workflow.read_text(encoding="utf-8"),
        Loader=UniqueKeyLoader,
    )
except yaml.YAMLError as exc:
    raise SystemExit(f"FAIL {workflow}: invalid or ambiguous YAML: {exc}") from exc


def collect_uses(node):
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "uses":
                if not isinstance(value, str):
                    raise SystemExit(
                        f"FAIL {workflow}: uses value must be a string, found {value!r}"
                    )
                references.append(value)
            collect_uses(value)
    elif isinstance(node, list):
        for value in node:
            collect_uses(value)


collect_uses(document)

expected = {
    "actions/checkout": "actions/checkout@d23441a48e516b6c34aea4fa41551a30e30af803",
    "actions/setup-python": "actions/setup-python@ece7cb06caefa5fff74198d8649806c4678c61a1",
}
for action, pinned_reference in expected.items():
    prefix = f"{action}@"
    actual = [
        reference
        for reference in references
        if reference.casefold().startswith(prefix.casefold())
    ]
    if actual != [pinned_reference]:
        raise SystemExit(
            f"FAIL {workflow}: expected exactly {pinned_reference!r}, found {actual!r}"
        )
PY
for ci_status_doc in README.md HANDOFF_P5.md AGENTS.md \
    docs/evidence-matrix.md docs/observability.md docs/progress-log.md \
    docs/review-packet.md docs/task-plan.md; do
  rg -q '7e7c2de2b157cf5c2ace8b5263e8db5f57c89902' "$ci_status_doc"
  rg -q '30169095184' "$ci_status_doc"
  rg -q '89706968923' "$ci_status_doc"
  rg -q '30169095184.*(SUCCESS|PASS)' "$ci_status_doc"
  rg -q '(check-run annotations 为空|无 annotation)' "$ci_status_doc"
  rg -q '019f9a6b-f76e-7620-a9e4-1e681e7f8d0b' "$ci_status_doc"
  rg -q '019f9a6c-0733-7da1-986f-6176824be92d' "$ci_status_doc"
done
if rg -n 'v6.*(尚待|等待).*在线复验' README.md HANDOFF_P5.md AGENTS.md \
    docs/evidence-matrix.md docs/observability.md docs/progress-log.md \
    docs/review-packet.md docs/task-plan.md; then
  echo "FAIL stale checkout/setup-python v6 online-validation status" >&2
  exit 1
fi
rg -q 'run_python_suite tests/p8 76' scripts/run_all_local_gates.sh
rg -q 'validate_powershell_scripts\.ps1' \
  .github/workflows/p5-local-gates.yml README.md docs/task-plan.md \
  docs/evidence-matrix.md
rg -q 'RequireScriptAnalyzer' .github/workflows/p5-local-gates.yml \
  scripts/validate_powershell_scripts.ps1
rg -q 'test_validate_powershell_scripts\.ps1' \
  .github/workflows/p5-local-gates.yml
rg -q 'caseCount = 7' tests/powershell/test_validate_powershell_scripts.ps1
rg -q 'PowerShell.*7/7' README.md docs/task-plan.md \
  docs/evidence-matrix.md docs/progress-log.md docs/p8-local-closure.md
rg -q 'productAcceptanceClaimed = \$false' scripts/run_windows_p8_preacceptance.ps1
rg -q 'realIoEnabledClaimed = \$false' scripts/run_windows_p8_preacceptance.ps1
rg -q 'realRejectEnabledClaimed = \$false' scripts/run_windows_p8_preacceptance.ps1
rg -q 'Assert-NoReparsePointChain' scripts/run_windows_p8_preacceptance.ps1
rg -q -- '--source-manifest-sha256' scripts/run_windows_p8_preacceptance.ps1 \
  scripts/p8_release.py
rg -q '_require_no_windows_reparse_ancestors' scripts/p8_preflight.py \
  scripts/p8_release.py
rg -q 'treeEnumerationSucceeded' scripts/p8_soak_evidence.py \
  tests/p8/test_p8_soak.py
rg -q 'math\.isfinite' scripts/p8_soak_evidence.py
rg -q 'p8-windows-preacceptance-wrapper-v4' \
  scripts/run_windows_p8_preacceptance.ps1 \
  scripts/p8_windows_evidence_verify.py
rg -q 'p8-windows-host-report-v2' scripts/p8_preflight.py \
  scripts/collect_windows_p8_host_reports.ps1
rg -q 'p8-gpu-host-report-v2' scripts/p8_preflight.py \
  scripts/collect_windows_p8_host_reports.ps1
rg -Fq '[string]$RepositoryRoot' scripts/collect_windows_p8_host_reports.ps1
rg -Fq '"-RepositoryRoot", $repoRoot' scripts/run_windows_p8_preacceptance.ps1
rg -q 'RepositoryRoot and OutputDirectory must not overlap' \
  scripts/collect_windows_p8_host_reports.ps1
rg -q 'RepositoryRoot' README.md HANDOFF_P5.md \
  docs/windows-target-execution.md
rg -Fq 'D:\CigVision-secure\p8-evidence.key' \
  README.md HANDOFF_P5.md docs/windows-target-execution.md \
  docs/p8-local-closure.md
rg -q 'Get-CryptographicChallenge' scripts/run_windows_p8_preacceptance.ps1
rg -q 'host reports were not captured during this wrapper run' \
  scripts/p8_windows_evidence_verify.py
rg -q 'PREFLIGHT_CHECK_KEYS' scripts/p8_windows_evidence_verify.py
rg -Fq 'if set(by_name) != set(PREFLIGHT_CHECK_KEYS):' \
  scripts/p8_windows_evidence_verify.py
rg -q 'Get-CimInstance' scripts/collect_windows_p8_host_reports.ps1
rg -q 'Get-LockedFileSnapshot' scripts/collect_windows_p8_host_reports.ps1
rg -Fq '[System.IO.FileShare]::Read' \
  scripts/collect_windows_p8_host_reports.ps1 \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'script identity changed before execution' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'script identity changed during execution' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'Get-LockedFileSnapshot' README.md HANDOFF_P5.md \
  docs/evidence-matrix.md docs/progress-log.md \
  docs/windows-target-execution.md
rg -Fq 'FileShare.Read' README.md HANDOFF_P5.md \
  docs/evidence-matrix.md docs/progress-log.md \
  docs/windows-target-execution.md
rg -Fq '.collector-owner' scripts/collect_windows_p8_host_reports.ps1 \
  scripts/p8_windows_evidence_verify.py
rg -Fq '.wrapper-owner' scripts/run_windows_p8_preacceptance.ps1 \
  scripts/p8_windows_evidence_verify.py
if rg -q 'Remove-Item[^[:cntrl:]]*-Recurse' \
    scripts/collect_windows_p8_host_reports.ps1 \
    scripts/run_windows_p8_preacceptance.ps1; then
  echo "FAIL P8 collector/wrapper must not recursively delete evidence directories" >&2
  exit 1
fi
rg -q 'collectorAssertionsValidated' scripts/p8_preflight.py \
  scripts/p8_windows_evidence_verify.py
rg -q '_validate_host_report_inputs' scripts/p8_windows_evidence_verify.py
rg -q 'trusted repository source' scripts/p8_windows_evidence_verify.py \
  tests/p8/test_p8_windows_evidence_verify.py
rg -q '2026-07-25T04:00:00\.0000000Z' \
  tests/p8/test_p8_windows_evidence_verify.py
rg -q 'host-inputs/windows\.json' scripts/p8_windows_evidence_verify.py \
  docs/windows-target-execution.md
rg -q 'p8-windows-evidence-import-receipt-v4' \
  scripts/p8_windows_evidence_verify.py docs/windows-target-execution.md
rg -q 'Record externally: wrapper manifest SHA-256' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'Record externally: wrapper manifest HMAC-SHA-256' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q -- '--manifest-hmac-sha256' scripts/p8_windows_evidence_verify.py \
  scripts/run_windows_p8_preacceptance.ps1 README.md \
  docs/windows-target-execution.md docs/p8-local-closure.md
rg -q -- '--evidence-key' scripts/p8_windows_evidence_verify.py \
  scripts/run_windows_p8_preacceptance.ps1 README.md \
  docs/windows-target-execution.md docs/p8-local-closure.md
rg -q 'EvidenceKeyPath must contain exactly 32 bytes' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'EvidenceKeyPath must be outside DeploymentRoot' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'external evidence HMAC key must be outside the evidence bundle' \
  scripts/p8_windows_evidence_verify.py
rg -Fq '[System.IO.FileMode]::CreateNew' \
  scripts/run_windows_p8_preacceptance.ps1
rg -Fq '$wrapperManifestStream.Flush($true)' \
  scripts/run_windows_p8_preacceptance.ps1
rg -q 'Wrapper manifest changed before its trust anchors were emitted' \
  scripts/run_windows_p8_preacceptance.ps1
rg -Fq 'CreateNew + Flush(true)' docs/windows-target-execution.md \
  docs/evidence-matrix.md docs/progress-log.md docs/review-packet.md
rg -q 'provenanceFiles' scripts/run_windows_p8_preacceptance.ps1 \
  scripts/p8_windows_evidence_verify.py
rg -q 'def import_evidence' scripts/p8_windows_evidence_verify.py
rg -q 'run_python_suite tests/p8 76' scripts/run_all_local_gates.sh
rg -q 'PASS all local gates' scripts/run_all_local_gates.sh
rg -q 'P8.*76' README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/progress-log.md docs/review-packet.md docs/review-results.md
rg -q '13 个 .*\.ps1|13 个 `\.ps1`|13 脚本' \
  README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/progress-log.md docs/p8-local-closure.md
rg -q '7 个 provenance|7 个 `provenance`' \
  README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/windows-target-execution.md
rg -q '019f99b4-2e4c-7742-9d20-60a0396d7900' \
  docs/evidence-matrix.md docs/qa-checklist.md docs/review-results.md
rg -q '019f99b4-4746-7d72-b5b6-568af6d8dfdd' \
  docs/evidence-matrix.md docs/qa-checklist.md docs/review-results.md
if rg -q 'P8.*(39/39|38/38|32/32)|soak 8/8|preflight.*11/11' \
    README.md HANDOFF_P5.md docs; then
  echo "FAIL P8 documentation contains stale regression totals" >&2
  exit 1
fi
if rg -q '当前(仍为|合计| local tooling|本地回归)[^。]*70/70|P8 当前精确计数 70' \
    README.md AGENTS.md HANDOFF_P5.md docs; then
  echo "FAIL current P8 status still reports the historical 70/70 total" >&2
  exit 1
fi
if rg -q 'p8-(windows|gpu)-host-report-v1|wrapper v3|receipt v3|WindowsInput =|GpuInput =' \
    README.md HANDOFF_P5.md docs/windows-target-execution.md \
    docs/architecture.md docs/p8-local-closure.md; then
  echo "FAIL current P8 operating docs contain superseded v1/v3/history-input instructions" >&2
  exit 1
fi
for final_gate_doc in README.md HANDOFF_P5.md docs/task-plan.md \
    docs/evidence-matrix.md docs/progress-log.md docs/review-packet.md \
    docs/review-results.md docs/qa-checklist.md docs/p8-local-closure.md; do
  rg -q '6886856' "$final_gate_doc"
done
rg -q 'P5 100.*P6 17.*P8 76' README.md HANDOFF_P5.md \
  docs/task-plan.md docs/evidence-matrix.md docs/progress-log.md \
  docs/review-packet.md docs/review-results.md docs/qa-checklist.md \
  docs/p8-local-closure.md
rg -q 'C\+\+14/C\+\+17|C\+\+14.*C\+\+17' \
  README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/progress-log.md docs/review-packet.md docs/review-results.md \
  docs/qa-checklist.md docs/p8-local-closure.md
rg -q '20 次重复' README.md docs/task-plan.md docs/evidence-matrix.md \
  docs/progress-log.md docs/review-packet.md docs/review-results.md \
  docs/qa-checklist.md docs/p8-local-closure.md
if rg -q '返修后.*full gate.*待|最终门待执行|最终门待跑|full gate/reviewer/QA 待执行' \
    README.md HANDOFF_P5.md AGENTS.md docs/acceptance-criteria.md \
    docs/evidence-matrix.md docs/known-issues.md docs/observability.md \
    docs/p8-local-closure.md docs/progress-log.md docs/qa-checklist.md \
    docs/review-packet.md docs/review-results.md docs/task-plan.md \
    docs/windows-target-execution.md; then
  echo "FAIL P8 documentation still reports the completed final full gate as pending" >&2
  exit 1
fi
if rg -q '最终 reviewer/QA 仍须复核' \
    README.md HANDOFF_P5.md AGENTS.md docs; then
  echo "FAIL P8 documentation still reports completed final reviewer/QA as pending" >&2
  exit 1
fi

duplicate_issue_ids="$({
  rg -o '^\| KI-[0-9]{3} \|' docs/known-issues.md || true
} | sed -E 's/^\| (KI-[0-9]{3}) \|$/\1/' | sort | uniq -d)"
if [[ -n "$duplicate_issue_ids" ]]; then
  echo "FAIL duplicate known-issue IDs:" >&2
  echo "$duplicate_issue_ids" >&2
  exit 1
fi

while IFS= read -r issue_id; do
  if ! rg -q "^\| ${issue_id} \|" docs/known-issues.md; then
    echo "FAIL detailed issue section has no table entry: $issue_id" >&2
    exit 1
  fi
done < <(rg -o '^## KI-[0-9]{3}:' docs/known-issues.md | sed -E 's/^## (KI-[0-9]{3}):$/\1/')

if rg -q '当前仍等待真实人工首标' README.md; then
  echo "FAIL README contains the superseded P5 human-review status" >&2
  exit 1
fi
rg -q '30 图 pilot 的人工复核晋级' README.md
if rg -q '待绑定 P4 模型/engine/config 运行正式小规模基线' docs/acceptance-criteria.md; then
  echo "FAIL acceptance criteria contains the superseded pre-P5-02C5 baseline status" >&2
  exit 1
fi
rg -q 'P5-02C5 已对 reviewed pilot 运行 ONNX Runtime CPU 诊断基线' docs/acceptance-criteria.md
rg -q 'P6/P7 的本地核心.*当前阶段已推进到 P8' docs/evidence-matrix.md
rg -q '^\| P5-02C4 探索性分歧可视化 \|' docs/task-plan.md
rg -q '^\| P5-02C5 临时 ONNX Runtime CPU pilot 基线 \|' docs/task-plan.md
rg -q '^\| P5-02C7 受控输入就绪与 fresh-clone 复核 \|' docs/task-plan.md
rg -q '^\| AC-05-06 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-06-01 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-06-02 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-06-03 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q 'CURRENT_PHASE:P8' docs/task-plan.md
rg -q '^\| P6-01A 核心回放与模拟剔除安全边界 \|' docs/task-plan.md
rg -q '^\| P6-01B Qt 图片/录制 manifest 接入 \|' docs/task-plan.md
rg -q '^\| P6-02 多相机/乱序/丢帧容量曲线 \|' docs/task-plan.md
rg -q '^\| P7-01A 产品运行状态与最近结果复核 \|' docs/task-plan.md
rg -q '^\| AC-07-01 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-07-02 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-08-01 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-08-02 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q '^\| AC-08-03 \|' docs/acceptance-criteria.md docs/evidence-matrix.md
rg -q -- '--simulation-batch-manifest' README.md docs/task-plan.md docs/evidence-matrix.md
rg -q 'simulation-trace\.json' docs/task-plan.md docs/evidence-matrix.md
rg -q 'stationId' 01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.h \
  01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp
rg -q 'delayBeforeMicros' 01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.h \
  01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp
rg -q 'SteadyReplayPacer' 01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp
rg -q 'SimulationRejectOutput' 01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp
rg -q 'RealtimeLoadSimulator' 01_上位机_QT_新版_CigVision/源码/core/RealtimeLoadSimulation.h \
  tests/CigVision.Simulation/SimulationTests.cpp
rg -q 'ProductRuntimeState' 01_上位机_QT_新版_CigVision/源码/core/ProductRuntimeState.h \
  tests/CigVision.ProductState/ProductStateTests.cpp \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q 'configuredParametersApplied' \
  01_上位机_QT_新版_CigVision/源码/core/ProductRuntimeState.h \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q 'parameterSha256' \
  01_上位机_QT_新版_CigVision/源码/core/InspectionContracts.h \
  01_上位机_QT_新版_CigVision/源码/core/OfflineInspection.h \
  01_上位机_QT_新版_CigVision/源码/core/ProductRuntimeState.h \
  tests/CigVision.ProductState/ProductStateTests.cpp \
  01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q 'DETECTOR_PARAMETER_IDENTITY_MISMATCH' \
  01_上位机_QT_新版_CigVision/源码/core/OfflineInspection.h \
  tests/CigVision.Offline/OfflineTests.cpp
rg -q 'hasDuplicateTopLevelJsonKey' \
  01_上位机_QT_新版_CigVision/源码/adapters/qt/QtOfflineInspection.cpp
rg -q 'a973097f62c125cab024aa3db40ce304bb0e7ce57b22e1565daf5cf5032605a6' \
  tests/CigVision.ProductState/ProductStateTests.cpp
rg -Fq 'CigVision w(nullptr, true);' \
  01_上位机_QT_新版_CigVision/源码/main.cpp
rg -Fq 'if (!offlineOnlyMode)' 01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -Fq 'ui.btn_run->setEnabled(false);' \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q 'invalid review enum must fail' \
  tests/CigVision.ProductState/ProductStateTests.cpp
rg -q 'invalid diagnostic severity must fail' \
  tests/CigVision.ProductState/ProductStateTests.cpp
rg -q 'station and camera identities must not collide through separators' \
  tests/CigVision.ProductState/ProductStateTests.cpp
rg -q 'pendingStopSession.requestStop' tests/CigVision.Offline/OfflineTests.cpp
rg -q 'int unreviewedRows = 0;' 01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q '最终会话证据落盘失败，已取消退出' \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
rg -q '在线任务未能安全停止，已取消退出' \
  01_上位机_QT_新版_CigVision/源码/CigVision.cpp
if sed -n '/void CigVision::onOfflineFrameProcessed/,/void CigVision::onOfflineFinished/p' \
    01_上位机_QT_新版_CigVision/源码/CigVision.cpp |
    rg -q 'persistProductState'; then
  echo "FAIL per-frame product-session persistence is forbidden" >&2
  exit 1
fi
rg -q 'btn_search' docs/p7-ui-inventory.md
rg -q '^\| KI-040 \|' docs/known-issues.md
rg -q 'readiness 24/24、P5 100/100' docs/task-plan.md docs/evidence-matrix.md
if rg -q '^#+ P5-02C4 provisional fallback' docs; then
  echo "FAIL provisional fallback baseline reuses the P5-02C4 visual-pack ID" >&2
  exit 1
fi
rg -q '^- \[x\] Independent reviewer and QA verdicts\.$' docs/task-plan.md
if rg -q 'independent review/QA pending' \
    docs/evidence-matrix.md docs/progress-log.md docs/review-packet.md docs/review-results.md; then
  echo "FAIL P5-02C5 still reports a completed independent gate as pending" >&2
  exit 1
fi
if rg -q 'P5-02C7.*(进行中|待最终|待读取)' \
    docs/qa-checklist.md docs/review-packet.md docs/review-results.md docs/progress-log.md; then
  echo "FAIL P5-02C7 review records still contain stale pending status" >&2
  exit 1
fi

readiness_report="$(mktemp)"
readiness_stdout="$readiness_report.stdout"
cleanup_readiness() {
  rm -f "$readiness_report" "$readiness_stdout"
}
trap cleanup_readiness EXIT
readiness_status=0
python3 scripts/p5_input_readiness.py \
  --require-reviewed --require-fallback --output "$readiness_report" \
  >"$readiness_stdout" || readiness_status=$?
if [[ "$readiness_status" -ne 0 && "$readiness_status" -ne 2 ]]; then
  echo "FAIL P5 input readiness returned invalid/mismatched status ($readiness_status)" >&2
  cat "$readiness_stdout" >&2 || true
  exit 1
fi
python3 - "$readiness_report" "$readiness_status" <<'PY'
import json
import sys

report = json.loads(open(sys.argv[1], encoding="utf-8").read())
status = int(sys.argv[2])
allowed_missing = {"reviewed_truth", "fallback_baseline"}
if report.get("invalid_or_mismatched_required_count") != 0:
    raise SystemExit("P5 input readiness has invalid/mismatched required inputs")
missing = set(report.get("missing_required", []))
if not missing <= allowed_missing:
    raise SystemExit(f"unexpected missing P5 inputs: {sorted(missing - allowed_missing)}")
if status == 0 and not report.get("ready"):
    raise SystemExit("readiness exit 0 but report is not ready")
if status == 2 and report.get("ready"):
    raise SystemExit("readiness exit 2 but report is ready")
PY
if [[ "$readiness_status" -eq 2 ]]; then
  echo "INFO P5 controlled reviewed-truth/fallback artifacts are absent; strict pilot scope remains blocked" >&2
fi
cleanup_readiness
trap - EXIT

git diff --check
xmllint --noout tests/CigVision.Simulation/CigVision.Simulation.vcxproj
xmllint --noout tests/CigVision.ProductState/CigVision.ProductState.vcxproj

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
done < <(git ls-files --others --exclude-standard -z -- AGENTS.md .github docs scripts tests requirements-p5.txt \
  01_上位机_QT_新版_CigVision/源码)

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
