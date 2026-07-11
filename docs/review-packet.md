# 评审包

## P5 路线文档维护评审包（2026-07-11）

状态：文档维护完成，P5 仍为待开始；本轮没有实现 P5 代码、没有修改脚本、没有提交或推送。

### 范围与结论

- 维护 `README.md`、`AGENTS.md`、需求、架构、计划、验收、问题、QA、可观测性、证据和评审记录。
- 当前唯一阶段为 `CURRENT_PHASE:P5`；P0-P4 历史结论保留，P4 仅声明 TensorRT 技术集成通过，不声明商业准确率。
- P5-P8 统一为本地数据效果、本地实时流与模拟剔除、沿用现有风格的 Qt 产品化、本地稳定性/部署/交付预验收。
- 真实相机、MVS 采集、DAQNavi 输入输出、卷烟机同步和真实剔除冻结，不是 P5-P8 的待补验收项。
- 老版 Qt/Halcon 与仓库历史资料保留为业务、算法和时序参考；历史结论不能覆盖当前 `docs/` 状态或直接进入产品构建。

### 验证

```text
python C:\Users\hp\.codex\skills\codex-long-task-architecture\scripts\light_gate.py .
PASS: no obvious doc/evidence warnings

git diff --check
PASS: exit 0（仅 CRLF 转换提示）

阶段标记检查：CURRENT_PHASE 仅 1 处，为 P5
旧路线扫描：无“P5 接相机/P6 真实硬件”等当前计划命中
PowerShell 等价文档结构检查：必需文件、关键 ID、未跟踪文件空白、P5 受限目录全部 PASS
```

`scripts/validate_project_docs.sh` 可由 `D:\git\Git\bin\bash.exe` 启动，但其未跟踪文件 `git diff --no-index --check` 在当前 Windows CRLF 工作树中会把换行转换提示或 CR 字节判为 whitespace；已读取脚本并逐项用 PowerShell 等价复核。此项记录为 degraded validator compatibility，不冒充原命令已通过。

## P3 当前评审

状态：P4 TensorRT 技术集成提交门 PASS 并已关闭；阶段指针切换到 P5 本地数据与算法效果闭环待开始。模型效果无人工 ground truth，准确率和真实硬件均不在 P4 通过声明内。用户已决定当前 P5-P8 全部在本地推进，现场硬件工作冻结。

### 范围

- `core/OfflineInspection.h`：离线编排、判定、统计、停止与 fixture detector。
- `adapters/qt/QtOfflineInspection.*`：图片 source、输入哈希、原子 JSON/PNG、worker 和批处理。
- `CigVision.cpp/.h`、`main.cpp`、`CigVision.vcxproj`：离线 UI、无硬件模式和命令入口。
- `tests/CigVision.Offline`、`tests/fixtures/p3-samples.json`、`scripts/run_windows_p3_offline.ps1`。
- 返修正式证据：`artifacts/p3-offline-20260711-132348`；另含 `ui-qa-observation.json` 与 `ui-qa-output`。

### 验证结果

~~~text
VS 2022 Developer PowerShell:
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p3_offline.ps1

offline tests Debug: MSBuild 0, tests 0, 7/7
offline tests Release: MSBuild 0, tests 0, 7/7
main Release environment/MSBuild: 0/0
fixed batch: exit 0, 8 JSON, 8 PNG
statistics: received=8 processed=8 OK=4 NG=4 error=0 dropped=0 saveFailures=0
offline UI: Responding=true, screenshot=true, controls clicked=false
invalid manifest: exit 2, no summary
Computer Use UI: one image selected, preview visible, OK=1/NG=0/error=0, 3 output files hashed
top-level: passed, exit 0

git diff --check: exit 0 before docs synchronization; must rerun after review fixes
~~~

### 评审重点

1. 生产/消费线程、停止、队列关闭、重复运行和异常路径是否可能死锁、泄漏或假成功。
2. 输入哈希、图片深拷贝、frame_id、判定校验与 JSON/PNG 原子保存是否一致。
3. UI worker 生命周期和 queued signal 是否只在 UI 线程更新 QWidget。
4. `--offline`/批处理是否确实绕开相机、DAQNavi 与剔除，fixture 是否被明确限制为非生产。
5. 七组测试及 8 图证据是否覆盖 AC-03-01/02，是否存在 false-green 或证据哈希缺口。

### 已知限制

- fixture 以奇偶 frame_id 生成 OK/NG，不是 ground truth，不证明准确率或 P4。
- UI 已用 Computer Use 完成单图文件选择、输出目录选择、预览和统计刷新；长批次 UI 停止仍只由核心自动测试覆盖。
- HALCON 许可、MVS 相机、DAQNavi IO、真实磁盘满/长时压力和真实剔除均未验证或不在 P3 范围。
- artifact 含本机绝对路径，只作本机审计，禁止提交；`07_运行环境与依赖包`、构建产物和 artifact 同样禁止提交。

### 首轮独立发现与返修

- reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 首轮 BLOCKED：停止请求可丢失、worker 裸指针悬空、协作者异常可 `std::terminate`、UI 容量随文件数增长且停止仍排空、追踪 JSON 字段不足；另指出 UI 未实际运行、manifest 校验宽松和阶段文档漂移。
- 已用 state mutex 消除启动/停止竞态，新增启动即停止测试；worker 改用 `QPointer` 并在线程完成时删除。
- source/sink/archive/observer 均增加异常边界，新增 throwing collaborator 测试；两配置 7/7。
- UI/批处理固定容量 4，默认满队列施加背压，UI `drainOnStop=false`；DropOldest 由容量 1 测试覆盖。
- 输入 manifest 与逐帧 JSON 现含源路径哈希、station/source/cigarette/capturedAt、尺寸、框和 detector version；严格拒绝缺失/非法/重复 manifest 字段，正式脚本验证 exit 2。
- Computer Use 已在 `--offline` 下完成单图文件选择、输出目录、预览、统计与文件落盘；未初始化硬件、未触发剔除。

## P2 当前评审

状态：P2 实现、本机验证及同一 reviewer/QA 返修复核均 PASS，P2 已关闭；P3 仅切换为待开始。

### 范围与证据

- `core/InspectionContracts.h`：FramePacket、Detection、InspectionResult、RejectCommand 及支持类型。
- `core/InspectionInterfaces.h`：采集、检测、结果存储、剔除输出、时钟接口。
- `core/BoundedQueue.h`：显式容量、溢出、丢弃计数和关闭语义。
- `tests/CigVision.Contracts`：无框架、无 SDK 的 C++14 Level4/WX 测试工程。
- `scripts/run_windows_p2_contract_tests.ps1`：Debug/Release 构建、执行、哈希和 manifest。
- 正式契约证据：`artifacts/p2-contracts-20260711-122211`，两配置 MSBuild/test exit 0，7/7 通过；manifest 含确定命令和取证脚本自身哈希。
- 主程序回归：`artifacts/p2-main-regression-20260711-120926`，CigVision Release Rebuild exit 0。

### 评审重点

1. 图像内存是否真正由 FramePacket 拥有，尺寸/步长校验是否有溢出风险。
2. 检测器与 OK/NG 判定是否保持分层，接口是否反向依赖 UI/SDK。
3. RejectCommand 是否默认模拟，且没有真实 IO 实现或调用。
4. BoundedQueue 的满、关闭、丢弃计数和等待语义是否确定。
5. P2 是否克制在契约边界，没有进入 P3/P4/P5。

### 首轮独立发现与返修

- reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c`：非法 InspectionDecision/RejectMode 可通过、队列拒绝前按值消费所有权、缺少真实并发/唤醒测试、artifact 未绑定脚本。四项均已最小返修。
- QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b`：首轮 QA gate PASS；指出命令为空和脚本未哈希。已增加确定性命令 fallback 和脚本 SHA-256。
- 返修测试新增非法枚举、move-only 拒绝所有权、多个等待者关闭唤醒、关闭后排空和并发生产消费；同一 reviewer/QA 最终复核均 PASS。

## P1 已关闭评审

状态：Release Windows 构建与安全启动已完成；独立 reviewer/QA 最终 gate PASS，P1 已关闭；P2 仅切换为待开始，尚未实现。

### 范围

- 新版 CigVision 的相机映射、帧元数据所有权、有界队列和线程同步。
- Run/Stop、析构、MyCamera 和 readIOTask 生命周期。
- Debug/Release 的 MVS、DAQNavi、process DLL 工程关系。
- testWrite 编译排除和 rejectEnabled=false 安全配置。
- P1 静态/Windows 环境检查脚本及构建基线文档。
- 本轮额外范围：Qt/MVS/DAQNavi 实装取证、环境变量路径适配、失败 manifest 与两配置预检证据。

### Current Windows validation

~~~text
VS2022 Developer PowerShell:
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All

最终结果:
artifacts/p1-windows-20260711-115033/full-terminal.log
OverallResult=partial; OverallCommandExitCode=1
Debug environment-check exit 1; Release environment-check/MSBuild exit 0
MSBuild found: E:\visual studio\\MSBuild\Current\Bin\amd64\MSBuild.exe
qmake: D:\smokeqt\5.9.9\msvc2017_64\bin\qmake.exe (5.9.9)
MVS header/lib: D:\Hikrobot\MVS\MVS\Development (present)
DAQNavi header: D:\advantexh\DAQNavi\Inc\bdaqctrl.h (present)
HALCON Debug root: D:\MVTec\HALCON-25.05-Progress (header/lib/runtime missing)
HALCON Release root: D:\MVTec\HALCON-22.11-Steady (22.11.4.0 header/lib/runtime present)
DAQNavi runtime: C:\Windows\System32\biodaq.dll 4.1.22.0 (manifest SHA-256)
MSBuild: Release exit 0; `CigVision.exe/process.dll` SHA-256 in manifest
application startup: `startup-window.png` captures the CigVision main window; no application controls clicked

PowerShell parser tokenization:
PASS

git diff --check:
exit 0

bash validation scripts:
not run; bash/Git Bash not available；发现的独立 POSIX sh 不是 Bash，记录为 degraded
~~~

### Earlier static validation

以下为 `9266166` 之前 P1 静态切片已记录的历史证据，本轮 Windows PATH 中未重跑 shell 脚本，不能替代本轮 MSBuild 或启动证据。

~~~text

./scripts/validate_p1_static.sh
PASS P1 static project and source invariants

./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

xmllint --noout 三个 vcxproj
exit 0

bash -n 两个 shell 脚本
exit 0

git diff --check
exit 0

light_gate.py . --strict
no obvious doc/evidence warnings
~~~

### Test count

P1 没有独立 C++ 单元测试套件；本轮已执行 Release 全量 Rebuild 并生成产物，但这不等同于算法、硬件或重复启停测试。Debug 仍在环境检查阶段失败。

### Git status

本轮修改两个 PowerShell 脚本、两个 vcxproj、`process/dllmain.cpp` 和 P1 文档。工程改动参数化 Qt/MVS/DAQNavi/HALCON 路径；源码改动仅删除导致重复定义的冗余 include。取证脚本记录 Git 状态和 `source-snapshot.diff` 哈希，并将未捕获异常写入 failure，避免假 `passed` manifest。没有修改老版、TensorRT、相机/IO 业务或真实剔除逻辑。`artifacts/` 由 `.gitignore` 排除，不应提交。

当前 `git status --short` 为 15 个已跟踪文件修改：两个 vcxproj、`process/dllmain.cpp`、两个 PowerShell 脚本及十个 P1 文档；artifact 仅以 ignored 状态存在。

### 验收映射

见 evidence-matrix.md 的 AC-01-01 至 AC-01-07。

### 已知缺口

- Release 环境、MSBuild 和仅启动证据已通过；Debug 25.05、重复启停、相机采集和实际 IO 仍未验证。
- Debug/Release Halcon 版本仍分裂；Release 目标事实已确认，Debug 25.05 继续作为外部阻断，不擅自替换版本。
- HALCON 22.11.4.0 已从校验通过的完整包安装到 D 盘；Release 组合由 MSBuild 和启动模块证明。25.05 Progress 不在当前官方目录，Debug 保持阻断但不否定 AC-01-02 的“至少一个配置”要求。
- 安装器显示本机无 HALCON license；HDevelop 和许可算子未验证。CigVision 启动只证明 DLL 可加载。
- 本地 artifact 含主机名、用户名绝对路径及安装产品清单，只作本机审计证据；`artifacts/` 已忽略，禁止提交，外发前必须生成脱敏副本。

## P0 归档

状态：P0 提交门已通过；未执行 Git commit。

## 评审范围

- 新增 `AGENTS.md`、`docs/*.md` 和 `scripts/validate_project_docs.sh`。
- 检查这些文档是否准确描述现状、主线、依赖方向、阶段退出条件、风险、证据和提交门。
- P0 不评审业务源码改动，因为本阶段不应修改业务源码。

## 变更文件

当前未跟踪新增项：`AGENTS.md`、`docs/`、`scripts/`。P0 没有修改 `01`、`02`、`03` 或硬件业务源码。完整文件清单以 `git status --short` 和 reviewer 的仓库读取为准。

## Validation / 验证命令

```bash
./scripts/validate_project_docs.sh
python3 /Users/c/.codex/skills/codex-long-task-architecture/scripts/light_gate.py .
git diff --check
git status --short --branch
```

## Raw output / 原始输出

最新一次返修前输出：

```text
./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

git diff --check
exit 0, no output

python3 .../light_gate.py .
codex-long-task-architecture light gate: no obvious doc/evidence warnings

python3 .../light_gate.py . --strict
codex-long-task-architecture light gate: no obvious doc/evidence warnings

bash -n scripts/validate_project_docs.sh
exit 0, no output
```

独立 reviewer 指出原脚本的 `git diff --check` 不覆盖未跟踪文件；脚本已增强并完成返修后重跑：

```text
./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

python3 .../light_gate.py . --strict
codex-long-task-architecture light gate: no obvious doc/evidence warnings

bash -n scripts/validate_project_docs.sh
exit 0, no output

git diff --check
exit 0, no output

git status --short --untracked-files=all -- 01... 02... 03... 06...
exit 0, no output

rg -n '不存在的 test_yolo_trt_v2|CMake 源文件名错误|CMake 指向不存在' AGENTS.md docs scripts
exit 1, no matches
```

最终 reviewer 复核确认：错误 CMake 结论、验证脚本覆盖和事实证据问题均已 resolved。复核要求的最后一步是把结论和 AC/QA/证据状态落盘，本次更新已完成。

## Test count / 测试数量

P0 是文档引导阶段，代码测试不在范围内，未运行代码测试。这里不把 tests 0 声称为测试通过；P0 的有效检查是文档结构、Git diff、静态代码审计和独立文档评审。

## Git status

```text
## main...origin/main
?? AGENTS.md
?? docs/
?? scripts/
```

## 验收映射

见 `docs/evidence-matrix.md` 的 AC-00-01 至 AC-00-07。

## 已知缺口

- 本轮运行环境是 macOS，不能证明 Windows Qt 工程可构建或可运行。
- P0 不连接 GPU、相机或 IO，不声明运行 QA、UI QA 或硬件 QA 通过。
- 现场参数和模型验收基准仍需用户或项目方提供。

## P4 独立评审包（验收候选）

### 范围

- 新增 `adapters/tensorrt/TensorRtDetector.h/.cpp`，实现 TensorRT 10.x `IDetector`。
- 扩展 `QtOfflineInspection.*`、`main.cpp` 和 `CigVision.vcxproj`，增加显式 TensorRT 离线批处理、带框图和 D 盘依赖链接。
- 新增 `scripts/run_windows_p4_tensorrt.ps1`，收集构建、116 图、时延、确定性、负路径和 SHA-256 证据。
- 不审查为已实现：在线相机消费、DAQNavi、模拟/真实剔除、HALCON 算法、模型训练或人工 ground truth。

### 实现自查

- 模型契约来自 ONNX/PT 本体，不采信冲突历史 README。
- 旧 TRT 8.6 engine 精确失败已保留；当前 FP16 engine 仅在 artifacts，不提交。
- P3 fixture CLI 与 TensorRT CLI 分离，不允许静默回退。
- 配置缺失/engine 不存在 exit 4 并写 `initialization-error.json`；检测失败进入 `DetectionBatch` 错误，不伪装零框。
- `rejectEnabled=false`；P4 代码不引用 `RejectCommand`、`IRejectOutput` 或 DAQNavi。
- `git diff --check` exit 0（仅现有 CRLF 提示）。

### 验证命令与结果

- `trtexec --onnx=... --fp16 --saveEngine=... --skipInference`：exit 0，engine SHA-256 `B5175DCC31DFA1A8A88D593B4C4487F2758A79114798E1918300310A8B133DD8`。
- `trtexec --loadEngine=... --warmUp=1000 --duration=10 --iterations=100`：exit 0，约 138.9 qps；host median 6.98 ms/p95 7.81 ms。
- `scripts/run_windows_p4_tensorrt.ps1 -EnginePath <artifact engine>`：返修后 exit 0，证据 `artifacts/p4-tensorrt-20260711-150510`。
- P2 回归：`artifacts/p2-contracts-20260711-145147`，Debug/Release 各 7/7。
- P3 回归：`artifacts/p3-offline-20260711-145147`，两配置测试、主程序、固定 8 图和无硬件启动均 PASS。

### 证据与限制

- P4 manifest SHA-256：`C1266D908215D7FB611D4592153FC12FB58DBE8CD93271836D1D65CD8598A845`。
- 116 文件/113 唯一哈希，3 组重复结果一致；246 框；detector latency median 33.593 ms、p95 95.783 ms、p99 112.084 ms。
- 返修负路径：缺失/冲突 CLI exit 2，缺失 engine/非法尺寸 exit 4；manifest 显式绑定 10 个 P4 源码/工程/脚本 SHA-256，复算无差异。
- 无人工 ground truth、来源授权和训练集重叠证明，不声明 accuracy/precision/recall/mAP/误检率/漏检率。
- 正式中文类别、逐类阈值和业务禁用类别待用户/现场确认；视觉抽查存在重叠框、超大框和文字遮挡。
- 请求 reviewer 检查 C++ 生命周期、TensorRT/CUDA API、预后处理、配置校验、错误路径、工程依赖和安全边界；请求 QA 复跑 formal script、抽查 JSON/PNG/时延与负路径。

### P4 最终门禁

- 独立 reviewer `019f4ff6-8554-7700-992b-ab2773108818`：首轮 FAIL 的 6 项 finding 全部 resolved，返修技术 gate PASS；文档状态同步后提交门条件满足。
- 独立 QA `019f4ff6-9972-7b73-83a5-a288e6714ab5`：返修后独立复跑 gate PASS，证据 `artifacts/p4-qa-independent-postfix-20260711`；364 个证据文件、10 个 P4 source hash 全部一致。
- 串行 P3 最终回归：`artifacts/p3-offline-20260711-151220` PASS。此前 `150846` 因与 QA 同时 Rebuild 发生默认输出目录竞争而失败，保留为失败证据，不用于通过声明。
- 当前提交建议仅覆盖 P1-P4 累积源码/脚本/文档；不包含 artifacts、engine、构建产物、依赖包或根目录临时 config。
