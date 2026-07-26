#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

source_dir="01_上位机_QT_新版_CigVision/源码"

fail() {
  echo "FAIL $1" >&2
  exit 1
}

require_pattern() {
  local pattern="$1"
  local file="$2"
  rg -q "$pattern" "$file" || fail "missing expected pattern '$pattern' in $file"
}

forbid_pattern() {
  local pattern="$1"
  local file="$2"
  if rg -q "$pattern" "$file"; then
    fail "forbidden pattern '$pattern' found in $file"
  fi
}

xmllint --noout "$source_dir/CigVision.vcxproj"
xmllint --noout "$source_dir/CigVision.vcxproj.filters"
xmllint --noout "$source_dir/process/process.vcxproj"

require_pattern 'cameraMatchMap\.insert\("2-2"' "$source_dir/CigVision.cpp"
require_pattern 'frame\.mv_frame = \*pFrameInfo' "$source_dir/CigVision.cpp"
require_pattern 'std::atomic_bool systemRun' "$source_dir/CigVision.h"
require_pattern 'PictureNumberSnapshot' "$source_dir/CigVision.h"
require_pattern 'mutexPictureNumbers' "$source_dir/CigVision.cpp"
require_pattern 'beginCameraCallback' "$source_dir/CigVision.cpp"
require_pattern 'waitForCameraCallbacks' "$source_dir/CigVision.cpp"
require_pattern 'cameraLifecycleFault' "$source_dir/CigVision.cpp"
require_pattern 'CameraCallbackContext' "$source_dir/CigVision.cpp"
require_pattern 'detachCameraCallbacks' "$source_dir/CigVision.cpp"
require_pattern 'fatalReadError' "$source_dir/CigVision.cpp"
require_pattern 'requestStop' "$source_dir/readIOTask.h"
require_pattern 'bool initialize' "$source_dir/readIOTask.h"
require_pattern 'prepareStart' "$source_dir/readIOTask.h"
require_pattern 'readEnable\.load' "$source_dir/readIOTask.cpp"
require_pattern 'mainDlg->machineState\.pictureNumbers = snapshot' "$source_dir/readIOTask.cpp"
require_pattern 'readErrorTimes >= 100' "$source_dir/readIOTask.cpp"
require_pattern 'emit fatalReadError' "$source_dir/readIOTask.cpp"
require_pattern 'configuredComponent1 < 10' "$source_dir/readIOTask.cpp"
require_pattern 'runComponent2ToReject - runComponent1ToReject' "$source_dir/readIOTask.cpp"
require_pattern 'CopyImage' "$source_dir/CigVision.cpp"
require_pattern 'rejectEnabled=false' "$source_dir/config.ini"
require_pattern 'MvCameraControl\.lib' "$source_dir/CigVision.vcxproj"
require_pattern 'ProjectReference Include="process\\process\.vcxproj"' "$source_dir/CigVision.vcxproj"
require_pattern '<OutDir>\$\(SolutionDir\)\$\(Platform\)\\\$\(Configuration\)\\</OutDir>' "$source_dir/process/process.vcxproj"
require_pattern '\$\(ProjectDir\)\.\.' "$source_dir/process/process.vcxproj"
require_pattern '<AdditionalOptions>/utf-8 %\(AdditionalOptions\)</AdditionalOptions>' "$source_dir/CigVision.vcxproj"
require_pattern '<AdditionalOptions>/utf-8 %\(AdditionalOptions\)</AdditionalOptions>' "$source_dir/process/process.vcxproj"
require_pattern '<PreprocessorDefinitions>IMAGEPROCESS_EXPORTS;%\(PreprocessorDefinitions\)</PreprocessorDefinitions>' "$source_dir/process/process.vcxproj"
require_pattern 'defined\(IMAGEPROCESS_EXPORTS\)' "$source_dir/process/ImageProcess.h"

python3 - "$source_dir/CigVision.vcxproj" "$source_dir/process/process.vcxproj" <<'PY'
import sys
import xml.etree.ElementTree as ET

NS = {"msb": "http://schemas.microsoft.com/developer/msbuild/2003"}


def compile_groups(path):
    root = ET.parse(path).getroot()
    groups = []
    for group in root.findall("msb:ItemDefinitionGroup", NS):
        compile_node = group.find("msb:ClCompile", NS)
        if compile_node is not None:
            groups.append((group.get("Condition"), compile_node))
    return groups


def require_global_option(path, element_name, required_token, inherit_token):
    groups = compile_groups(path)
    global_values = []
    for condition, compile_node in groups:
        value_node = compile_node.find(f"msb:{element_name}", NS)
        if value_node is None:
            continue
        value = value_node.text or ""
        if condition is None:
            global_values.append(value)
        elif inherit_token not in value:
            raise SystemExit(
                f"FAIL {path}: conditional {element_name} must inherit {inherit_token}"
            )
    if not any(required_token in value and inherit_token in value for value in global_values):
        raise SystemExit(
            f"FAIL {path}: unconditional {element_name} must contain "
            f"{required_token} and {inherit_token}"
        )
    return groups


main_project, process_project = sys.argv[1:3]
require_global_option(
    main_project, "AdditionalOptions", "/utf-8", "%(AdditionalOptions)"
)
process_groups = require_global_option(
    process_project, "AdditionalOptions", "/utf-8", "%(AdditionalOptions)"
)
require_global_option(
    process_project,
    "PreprocessorDefinitions",
    "IMAGEPROCESS_EXPORTS",
    "%(PreprocessorDefinitions)",
)

export_occurrences = 0
for _, compile_node in process_groups:
    definitions = compile_node.find("msb:PreprocessorDefinitions", NS)
    if definitions is not None:
        export_occurrences += [
            token.strip() for token in (definitions.text or "").split(";")
        ].count("IMAGEPROCESS_EXPORTS")
if export_occurrences != 1:
    raise SystemExit(
        "FAIL process project must define IMAGEPROCESS_EXPORTS exactly once"
    )
PY

forbid_pattern 'MV_FRAME_OUT_INFO\* mv_frame' "$source_dir/CigVision.h"
forbid_pattern 'delete dataGray' "$source_dir/CigVision.cpp"
forbid_pattern 'nowPictureNumber1\.load' "$source_dir/CigVision.cpp"
forbid_pattern 'testWrite' "$source_dir/CigVision.vcxproj"
forbid_pattern 'testWrite' "$source_dir/CigVision.vcxproj.filters"
forbid_pattern 'E:\\王伟工作文件' "$source_dir/process/process.vcxproj"
forbid_pattern '^#define IMAGEPROCESS_EXPORTS$' "$source_dir/process/pch.h"

git diff --check
echo "PASS P1 static project and source invariants"
