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

forbid_pattern 'MV_FRAME_OUT_INFO\* mv_frame' "$source_dir/CigVision.h"
forbid_pattern 'delete dataGray' "$source_dir/CigVision.cpp"
forbid_pattern 'nowPictureNumber1\.load' "$source_dir/CigVision.cpp"
forbid_pattern 'testWrite' "$source_dir/CigVision.vcxproj"
forbid_pattern 'testWrite' "$source_dir/CigVision.vcxproj.filters"
forbid_pattern 'E:\\王伟工作文件' "$source_dir/process/process.vcxproj"

git diff --check
echo "PASS P1 static project and source invariants"
