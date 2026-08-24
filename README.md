# 烟支检测上位机项目

这是烟厂烟支外观检测上位机项目。产品主线是新版 Qt/C++ 工程，目标是由传统视觉检测逐步升级为可验证的深度学习检测闭环。

## 当前状态

当前阶段是 **P8：本地稳定性、部署与交付预验收（进行中）**。

本机统一验证入口：

```bash
./scripts/run_all_local_gates.sh --core
./scripts/run_all_local_gates.sh --full
```

`--core` 是 CI 使用的精确计数门，拒绝测试少发现或零发现假绿；默认 `--full` 另跑 C++14、20 次优化仿真和 ASan/UBSan。Apple 平台明确关闭不受支持的 LeakSanitizer，但保留 AddressSanitizer 与 UndefinedBehaviorSanitizer。

PowerShell 脚本另有独立静态门，不计入 P8 的 81 项 Python 清单：

```powershell
pwsh -NoProfile -File ./scripts/validate_powershell_scripts.ps1 -RequireScriptAnalyzer
pwsh -NoProfile -File ./tests/powershell/test_validate_powershell_scripts.ps1
```

2026-07-26 本轮在本机 Colima/Linux arm64 中使用 PowerShell 7.6.3 和 PSScriptAnalyzer 1.25.0 实跑：13 个 `.ps1` 的 parser/PSScriptAnalyzer finding 均为 0，CLI 契约 7/7 PASS，覆盖成功、损坏语法、PowerShell 7 三元语法被 5.1 兼容门拒绝、缺根、缺 `scripts/` 目录、空 `scripts/` 目录和缺 analyzer；exit 2/3 也输出机器可读 JSON。静态分析显式启用 `PSUseCompatibleSyntax`，目标为 Windows PowerShell 5.1；机器可读结果记录 `syntaxCompatibilityTargets=["5.1"]`、`scriptAnalyzerMinimumVersion="1.25.0"` 和 `windowsRuntimeClaimed=false`。这只证明跨平台语法兼容与静态分析，不证明 Windows、Qt、GPU、D 盘或硬件运行。

- P0-P3 已完成项目控制、Windows Release 构建基线、核心契约和离线检测闭环。
- P4 已完成 TensorRT 10 技术集成：新版 Qt/C++ 可在本机使用 GPU 批量推理并保存结构化结果和带框图。
- P4 没有人工 ground truth，只证明推理链和证据链可运行，不证明商业准确率；重叠框、大框和标签遮挡等效果问题由 P5 处理。
- P5-P8 全部在本地执行：数据与算法效果、本地实时流与模拟剔除、Qt 产品功能/UI 重构、稳定性/部署/交付预验收。
- P5 已完成数据工具基线、30 图确定性试标包、localhost 首标工作台、30 图 pilot 的人工复核晋级和受控输入就绪工具；其中 20 张进入比较、10 张 REVIEW 排除，并已形成仅供诊断的 ONNX Runtime CPU 临时基线。P5 仍因完整 train/validation/test 划分、类别业务批准、正式 TensorRT 和受控 artifact 恢复而阻断，未关闭且不声明商业准确率。
- P6 simulation 当前 15/15：P6-01A 完成录制序列节拍、元数据、停止取消和 Simulation-only 命令/回执；P6-01B 已接入 Qt manifest、CLI 和原子 `simulation-trace.json`；P6-02 新增 SDK-free 多相机/乱序/重复/跳号、两种队列溢出、P95 时延及停止重启模型。回执时间不会早于模拟计划时间，核心回放延迟统一受 60 秒本地安全上限约束。Windows/Qt 运行产物和生产容量仍未声称通过。
- P7 已新增线程安全、有界的 `ProductRuntimeState`，本机 8/8；类型化参数 profile、canonical SHA-256、configured/applied 区分、帧级参数哈希漂移拒绝、品牌七阈值独立页面与持久化已接入。离线回调携带完整产品元数据，运行、复核、统计和诊断页统一消费状态快照；会话仅在启动、结束和复核时原子写入 `product-session.json`。当前 UI 入口固定为 local-only，不初始化相机或 DAQNavi；Qt/Windows 页面、JSON 实际落盘和 Computer Use 仍待目标机验证。
- P8 Python 测试清单仍为 81 项：历史 76 项 package/Windows 工具门之外新增 5 项连续运行防假绿测试。当前候选已实跑连续专项 5/5 与 P8 精确 81/81；公开 `run_p8_local_continuous_soak.sh` 的 C++17 `contract-test-v1` 已完成 run + self-verify，并由独立 `verify` 再验。`contract-test-v1` 的 locked 最低时长仍为 0.12 秒，public wrapper 与 C++17/C++14 direct project contract 门实际运行 1.0 秒；累计进程组 CPU 至少 0.01 秒、RSS 增长不超过 16 MiB。`local-sdkfree-v1` 固定 2×300 秒、60 秒 warm-up、每轮至少 240 个资源 samples、240 条 progress、最大 progress gap 5 秒、至少 240 sessions/122880 frames、累计进程组 CPU 至少 10 秒、RSS 增长不超过 64 MiB、磁盘余量至少 1 GiB。
- 正式 project `run` 不再信任调用方准备的 project provenance：它在同一路径内使用 `--compiler/--standard` 受控编译真实 C++ runner，确认 8 个源文件编译期间未变化，再把 compiler/compile contract、源码和 executable 一并快照进 evidence；外部 `--build-provenance` 只允许 `test-helper`，伪造 project provenance 会 exit 2 且不创建 evidence。`durationSeconds` 来自已验真的 `local-soak-summary.json` 工作时长，`processLifetimeSeconds` 冻结于真实进程退出点；Linux 由 `ps` 枚举进程组并用每个 PID 的 `/proc/<pid>/stat` 汇总 CPU，pure-sleep 伪 runtime 会被最低 CPU 门拒绝。C++ runner 实际循环 `OfflineInspectionSession`/`ProductRuntimeState`，逐轮对齐 Offline 与 product-state 的 OK/NG/Error；`local-soak-progress.ndjson` 提供持续进度。独立 evidence inventory 除 manifest 外不忽略 `.tmp`，tmp/symlink/FIFO 均失败；Windows 正常退出还需两次 fresh process-tree enumeration，二次枚举失败会使 clean gate 失败。real IO、real reject、product acceptance 始终为 false。本地 manifest/verify 只证明包内一致性与 snapshot/源码绑定，没有 Windows v4 的带外 HMAC，跨信任边界时需另存外部摘要。
- 旧 P8 v1 证据链保持不变：严格 package/主机报告 preflight 17、SDK-free soak 9、fixture release 17、Windows wrapper/collector 2、package manifest 7、跨机 evidence verify/import 24，共 76 项；HEAD `6886856` 的 full gate 与其独立 reviewer/QA PASS 只作为历史快照。主机 v4 wrapper、现场 challenge、collector 文件锁、外置 32-byte key、SHA/HMAC 和离线复验语义没有被 P8 v2 替换。
- 正式 `local-sdkfree-v1` 2×300 秒 evidence 已在 `artifacts/p8-continuous-local-20260727-final-v2` 完成 self-verify、主代理独立 verify 和 reviewer 两次独立 verify；两轮分别为 300.046591/300.020852 秒，共处理 3,793,408 帧，manifest SHA-256 为 `2bd420fab44acbed98315ffac5cf6a2b22567dcae42d98e522be35de3a7bcdd4`。该 evidence 绑定的 `p8_continuous_soak.py`、profile、C++ runtime、旧 helper 和 core headers 未被当前 CI 返修改动，离线 verify 仍 PASS。提交 `5f6a06a` 的 hosted `Local gates` run `30219159920`（job `89838475434`）FAIL，准确暴露两项跨平台门禁缺口：0.12 秒 project contract 在 Linux CPU tick 下得到 0.00 秒，以及旧 `os.abort()` crash fixture 被 core-dump 路径误归类。修复提交 `cc7a3ab` 让 public wrapper/direct project contract 实际运行 1.0 秒（locked profile 最低仍为 0.12 秒、timeout 2.0 秒、其余阈值不变），并在 POSIX abort 前设置 `RLIMIT_CORE=0`；Mac 与 Colima/Linux 原始 `--full`、P8 81/81、当前 CI 修复 reviewer 与 QA/observability 均 PASS（P0/P1/P2/P3=0/0/0/0）。修复后的 hosted `Local gates` run `30221330296`（job `89844182732`）SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空；本地工具提交门已收口。KI-048 与正式 evidence 结论保持不变；P8/AC-08 整体仍进行中。
- 当前工作树另完成两项静态防回退：process DLL 的 `IMAGEPROCESS_EXPORTS` 只由工程统一定义，CigVision/process 全配置增加 `/utf-8`；TensorRT 原型目录的 22 份 Markdown 统一标明“历史原型资料，不是当前构建、性能或交付证据”。该 warning 修复快照的 `--full` 本地门曾通过，但不覆盖当前 P8 v2；结果仍待目标 Windows Release Rebuild 确认原 `IMAGEPROCESS_EXPORTS`/C4819 warning 消失。

当前无法到现场。真实相机、卷烟机同步、DAQNavi 输入输出和真实剔除全部冻结，不属于 P5-P8 的完成声明；`rejectEnabled=false` 必须保持默认值。

P6 本地仿真入口（只写模拟命令和 trace，不写真实 IO）：

```text
CigVision.exe --simulation-batch-manifest tests/fixtures/p6-simulation-samples.json --simulation-output artifacts/p6-simulation --simulation-reject-delay-micros 500 --simulation-queue-capacity 4 --simulation-target-output simulation-reject
```

`simulation-trace.json` 是原子写入的运行记录；目标机运行前可先执行 SDK-free 只读预检：

```bash
python3 scripts/p6_simulation_preflight.py \
  --manifest tests/fixtures/p6-simulation-samples.json
```

目标机在生成 trace 后再加 `--trace <output>/simulation-trace.json --require-trace`。预检只检查 manifest/trace 的内部路径、哈希、元数据、Simulation-only 命令和回执账目，不执行 Qt、TensorRT、相机或真实 IO，也不替代 Windows runtime 证据。

目标 Windows 机器可用一键证据包装脚本执行 Release 构建、Simulation batch、产物/trace 复核和 CLI 负路径矩阵：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p6_simulation.ps1 `
  -Manifest .\tests\fixtures\p6-simulation-samples.json `
  -EvidenceRoot .\artifacts\p6-windows-simulation-YYYYMMDD-HHMMSS
```

包装脚本调用 `scripts/p6_windows_simulation_evidence.py`，输出 `input-manifest.json`、逐帧 JSON/PNG、`summary.json`、`simulation-trace.json`、原始 stdout/stderr、预检报告、负路径回执和哈希绑定的 `manifest.json`。它要求 `rejectEnabled=false`，只接受 Simulation 命令；生成的目录属于本机证据，禁止提交。当前 Mac/CI 只能用测试替身验证编排逻辑，不能将 `--allow-non-windows-test` 的结果写成 Windows runtime 证据。

P8 目标 Windows 主机先用 `scripts/p8_package_manifest.py` 在 package 外生成严格 manifest，并在 `DeploymentRoot` 外准备 exactly 32-byte key（建议 `D:\CigVision-secure\p8-evidence.key`）。不要手动运行 collector 或准备历史报告；v4 wrapper 生成 challenge，把 collector 复制到 `provenance/`，再以 mandatory `-RepositoryRoot $repoRoot` 立即现场采集 v2 `windows.json`/`gpu.json`。`RepositoryRoot` 与 collector `OutputDirectory` 不得重叠，provenance 中的复制脚本不能从自身 `$PSScriptRoot` 推导仓库根。wrapper 随后串联 preflight、soak、package 身份复检、fixture package/verify/activate 和可选 rollback。manifest 以 `CreateNew + Flush(true)` 写入固定 UTF-8 bytes，SHA/HMAC 针对同一 bytes 计算并复读确认未漂移。离线验真只能执行独立取得并信任的仓库 verifier；严禁执行待验 evidence 的 provenance 代码。完整步骤见 `docs/windows-target-execution.md`。

P8 v2 本地连续运行入口与 Windows v1 证据链相互独立。短门仅用于快速验证命令、证据与守恒语义；正式本地证据必须使用固定 profile，不能从 CLI 放宽阈值。正式 `final-v2` evidence 已完成；不得覆盖或复用该目录，未来重跑必须使用新的 evidence root：

```bash
./scripts/run_p8_local_continuous_soak.sh \
  --profile contract-test-v1 \
  --evidence-root artifacts/p8-continuous-contract-YYYYMMDD-HHMMSS

./scripts/run_p8_local_continuous_soak.sh \
  --profile local-sdkfree-v1 \
  --evidence-root artifacts/p8-continuous-local-YYYYMMDD-HHMMSS
```

第二条固定执行两轮、每轮至少 300 秒。只有 `soak-manifest.json` 自验通过、两轮资源/CPU/时长/磁盘门和 runtime 守恒都满足，并在同一目录完成独立二次 verify，且最终 QA/cleanup 与完整本地门收口后，才能把它列为 P8 v2 本地连续运行证据；它仍不是 Windows、GPU 或产品验收。

本轮正式证据为 `artifacts/p8-continuous-local-20260727-final-v2`：manifest inventory 为 29 个文件、3,654,771 bytes（根目录另含 manifest，共 30 个文件），manifest SHA-256 为 `2bd420fab44acbed98315ffac5cf6a2b22567dcae42d98e522be35de3a7bcdd4`。独立复验命令如下，当前结果为 PASS：

```bash
python3 scripts/p8_continuous_soak.py verify \
  --evidence-root artifacts/p8-continuous-local-20260727-final-v2
```

证据复制离开目标机后，必须同时使用带外 SHA、HMAC 和 bundle 外 32-byte key 验证；需要纳入本地证据库时再做原子导入：

```bash
python3 scripts/p8_windows_evidence_verify.py verify \
  --evidence-root /path/to/copied-p8-evidence \
  --manifest-sha256 <externally-recorded-wrapper-manifest-sha256> \
  --manifest-hmac-sha256 <externally-recorded-wrapper-manifest-hmac-sha256> \
  --evidence-key /secure/outside-bundle/p8-evidence.key
python3 scripts/p8_windows_evidence_verify.py import \
  --evidence-root /path/to/copied-p8-evidence \
  --manifest-sha256 <externally-recorded-wrapper-manifest-sha256> \
  --manifest-hmac-sha256 <externally-recorded-wrapper-manifest-hmac-sha256> \
  --evidence-key /secure/outside-bundle/p8-evidence.key \
  --store-root artifacts/p8-evidence-store
```

## 生产验收状态

用只读状态聚合器快速查看当前可推进任务。它不会把本地或仿真结果外推为生产验收：

```bash
python3 scripts/production_acceptance_status.py --output work/production-acceptance-status.json
```

可选执行本地工程门：`--run-local-gates core` 或 `--run-local-gates full`。目标 Windows/GPU、真实相机、DAQNavi、IO/剔除、安全联锁和三方签字仍必须有独立证据。

恢复 artifact 后，先用受控渠道提供的外部摘要核对具体 manifest 文件，再运行 readiness：

```bash
python3 scripts/p5_verify_external_digest.py \
  --file artifacts/p5-reviewed-truth-YYYYMMDD-HHMMSS/evidence-manifest.json \
  --expected-sha256 <externally-recorded-sha256>
```

## 本地路线

| 阶段 | 目标 | 当前状态 |
| --- | --- | --- |
| P5 | 建立标注真值、冻结测试集和量化效果基线，优化阈值、NMS、异常框，必要时受控训练 | 外部阻断（未关闭） |
| P6 | 用文件/录制流模拟多相机、编号、节拍、积压、异常和模拟剔除 | 外部目标机阻断（本地源码、核心与证据编排已实现；Windows runtime 未验证） |
| P7 | 沿用现有 Qt UI 风格，精简旧功能，增强检测、复核、统计、配置和诊断 | 外部目标机阻断（本地源码完成） |
| P8 | 本地长时运行、性能优化、故障恢复、D 盘部署和商业交付预验收 | 进行中（本地工具） |

## 先从这里看

1. `AGENTS.md`：项目协作规则和文档导航。
2. `docs/task-plan.md`：当前阶段和后续主线。
3. `docs/architecture.md`：新版、老版和 TensorRT 原型的职责边界。
4. `docs/known-issues.md`：尚未解决的风险和证据缺口。
5. `docs/windows-build-baseline.md`：Windows 构建环境与取证步骤。
6. `docs/windows-target-execution.md`：P8 package manifest、Windows wrapper、外部摘要、跨机 verify/import 操作手册。
7. `docs/p5-source-inventory.md`、`docs/p5-labeling-guide.md`：P5 可用资料、类别边界、真值门和效果指标。
8. `docs/p7-ui-inventory.md`：P7 页面/控件消费者、保留与后续处理清单。

## 主要目录

| 目录 | 用途 |
| --- | --- |
| `01_上位机_QT_新版_CigVision/源码` | 当前产品主线，Qt/C++ 上位机 |
| `02_上位机_QT_老版_传统算法` | 老版传统算法闭环，仅作业务参考 |
| `03_深度学习模型与TensorRT` | 已接入模型的来源、历史 engine、训练和转换资料；历史结论需由当前证据复核 |
| `04_测试数据与样例图片` | P5 数据审计、标注、评估和回归的候选资料 |
| `05_项目文档与汇报` | 传统算法、业务背景和项目汇报参考；不覆盖 `docs/` 当前计划 |
| `07_运行环境与依赖包` | 本机保留的大依赖包，不进入 Git |
| `docs` | 当前执行计划、验收、证据、风险与评审记录 |
| `scripts` | 静态检查和 Windows 构建取证脚本 |

## 同伴协作

```bash
git clone git@github.com:pow1035/cigarette-inspection.git
cd cigarette-inspection
git pull origin main
```

修改前先看 `docs/task-plan.md`，只做当前阶段的工作。验证命令按阶段选择；文档和 Git 基础门至少运行：

```bash
./scripts/validate_project_docs.sh
git diff --check
```

P1-P4 的阶段验证脚本仍可用于回归。Windows 构建基线命令为：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All
```

P5 数据基线和 30 图试标包可分别用以下命令复现，输出只进入忽略的 `artifacts/`：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_data_baseline.ps1 -P4Results .\artifacts\p4-tensorrt-20260711-150510\batch-output
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_pilot.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_review_workbench.ps1 -Package .\artifacts\p5-pilot-20260711-192148\review-package -Workspace .\artifacts\p5-review-workspace
```

工作台只允许绑定 localhost，首轮导出为 `annotated` 且 `is_ground_truth=false`；`/api/export-reviewed` 继续受控拒绝。已完成的 30 图 pilot 通过独立哈希绑定的晋级脚本记录不同复核人和项目负责人批准，不修改旧 pass1。

P5 Python 工具的唯一第三方运行依赖是 Pillow，版本固定在 `requirements-p5.txt`；其余 P5 数据、晋级、一致性和工作台模块只使用 Python 标准库。安装和回归：

```bash
python3 -m pip install -r requirements-p5.txt
python3 -m unittest discover -s tests/p5 -p 'test_*.py'
python3 -m unittest discover -s tests/p6 -p 'test_*.py'
```

受控 pilot 输入在开始评估前必须只读通过模型、类别目录、artifact manifest、文件大小和 SHA-256 检查：

```bash
python3 scripts/p5_input_readiness.py --require-reviewed --require-fallback
```

该命令 exit 0 表示指定范围输入通过内部 manifest 一致性检查，exit 2 只表示 reviewed-truth/fallback 证据目录尚未通过受控渠道恢复，exit 3 表示已有输入缺失、错配或格式非法。它不是 manifest 自身的签名认证：恢复 artifact 时还必须用受控渠道提供的 evidence-manifest SHA-256 做外部核对（fallback 的历史摘要见 `HANDOFF_P5.md`，reviewed 摘要当前未随 clone 提供）。当前 fresh clone 的预期结果是 exit 2；不能用缺失输入开始 pilot，也不能把 CPU smoke 当作正式 TensorRT 证据。

GitHub Actions 的 `Local gates` 工作流会在 pull request、`main` push 和手动触发时执行文档/P1 静态门、P5/P6/P8 Python 精确计数回归、无 SDK C++ 回归和 PowerShell 静态门。提交 `5f6a06a` 的首次 P8 v2 hosted run `30219159920`（job `89838475434`）FAIL；失败是有效跨平台发现，不是可忽略抖动。修复提交 `cc7a3ab` 已通过 Mac 与 Colima/Linux 原始 `--full`、独立 reviewer 和 QA/observability；修复提交已 push，hosted `Local gates` run `30221330296`（job `89844182732`）SUCCESS。历史提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 的在线运行 `30169095184`（job `89706968923`）SUCCESS、check-run annotations 为空，最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` 与 QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS；该结果只覆盖当时 76 项/CI 硬化快照。Windows/MSVC、Qt UI、TensorRT/GPU 和现场硬件仍保留为人工或目标机门禁，不能由 Linux CI 代替。

上一成功 hosted 基线为提交 `6d492528c7f9c0b0b2e3cc70e1c60a74cb62cede` 的 `Local gates` run `30188084112`（job `89756233568`）SUCCESS；首轮 `5f6a06a` / run `30219159920` 为 FAIL；修复提交 `cc7a3ab` / run `30221330296` 已 SUCCESS。两者都不能代替目标 Windows/MSVC、Qt、GPU、TensorRT、D 盘或硬件验证。

构建产物、IDE 缓存、依赖安装包、模型/engine、大测试数据和 `artifacts/` 证据目录不得提交。不要提交账号、许可证、设备序列号、客户资料、密钥或含机器身份的未脱敏日志。
