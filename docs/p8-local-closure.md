# P8 本地稳定性与部署工具闭环

## 状态与声明边界

P8 当前只建立可在无 Qt、Windows、GPU 和现场硬件环境运行的本地工具闭环：

```text
历史 Windows/package 工具链 76 项（HEAD 6886856 历史 full PASS）
→ 独立新增 P8 v2 连续运行防假绿语义 5 项
→ 当前连续专项 5/5、P8 精确 81/81、公开 C++17 wrapper run/self-verify/独立 verify PASS
→ KI-048 与正式 final-v2 2×300/独立 verify/QA/observability/cleanup PASS
→ hosted Linux 首轮发现两项短门跨平台缺口；当前返修 Mac/Linux full、reviewer 与 QA/observability PASS，hosted rerun 待完成
```

历史本地工具通过状态写作 `passed-local-tooling`；当前 P8 v2 正式 manifest 写作 `passed-local-continuous-tooling`。短 contract、当前 Mac/Colima/Linux 综合本地门、KI-048 dependency provenance、正式两轮 evidence、独立 verify、当前 CI 修复 reviewer/QA/observability 和正式 evidence QA/observability/cleanup 均已通过，因此 P8 v2 本地连续工具证据可以标 PASS。hosted rerun 尚未完成；它仍不等于 AC-08 或 P8 整体通过，不证明 Windows/Qt 产品、TensorRT/GPU、D 盘安装、生产吞吐、商业准确率、真实磁盘满恢复或现场 IO/剔除。

## 工具

| 工具 | 本地职责 | 禁止外推 |
| --- | --- | --- |
| `scripts/p8_preflight.py` | 严格 JSON/INI、外部 manifest SHA、包内路径/文件哈希、品牌和 local-only 安全门；严格验证 Windows/GPU v2 报告、本次 challenge/package 与同 capture/host/time/collector SHA | 不执行产品；`collectorAssertionsValidated=true` 只表示 collector 契约通过，`productAcceptanceChecked=false`、`semanticAcceptanceChecked=false` |
| `scripts/p8_package_manifest.py` | 在 package 外生成不可覆盖的严格 manifest；双重稳定性复扫并绑定完整相对路径/大小/SHA | 不证明 package 是产品发布包；package 或 manifest 变化后旧 SHA 立即失效 |
| `scripts/p8_soak_evidence.py` | 多轮独立输出、重启/超时/崩溃、RSS/磁盘/输出量采样和哈希清单 | GPU 固定 `not-collected`，短 CI profile 不是生产长稳 |
| `config/p8-continuous-soak-profiles-v1.json` | 固定 `contract-test-v1` 与 `local-sdkfree-v1` 的时长、采样、RSS、磁盘、输出和 timeout 门；必须与代码锁定值完全一致 | profile 不能由 CLI 放宽；tracked 文件变更必须和代码/测试一起审查 |
| `scripts/p8_continuous_soak.py` | project `run` 内部受控编译并快照 compiler/compile/8 source/executable；toolDependencies 快照新/旧 Python tool source 与 POSIX `ps` identity/binary；外部 provenance 仅限 contract helper；分离 verified duration 与真实 process lifetime；采集进程组 RSS/CPU/P50/P95，Linux CPU 使用 `/proc/<pid>/stat`；验证 progress、ProductRuntimeState、严格 inventory 和 Windows 双枚举 clean gate；支持离线二次 verify | 本地 SDK-free evidence，不采集 GPU，不证明 Windows 或产品稳定性；正式 `final-v2` 只支持本地连续工具证据声明 |
| `tests/CigVision.LocalSoak/LocalSoakRuntime.cpp` + `scripts/run_p8_local_continuous_soak.sh` | 公开 wrapper 前置检查 `python3`/`g++`/`ps`，再由 `run` 受控编译真实 C++ runner；循环 `OfflineInspectionSession` 与 `ProductRuntimeState`，写 `local-soak-summary.json` 并证明 frame/decision/sink/archive/product-state 守恒 | runner 固定 synthetic/fixture、real IO/reject/acceptance 为 false；不是 TensorRT、Qt 或真实数据运行 |
| `scripts/p8_release.py` | 生成/验证 fixture release；原子 activate/rollback；共享目录隔离 | artifact kind 固定 fixture，不称 Windows 产品发布包 |
| `scripts/collect_windows_p8_host_reports.ps1` | 由 wrapper 使用本次 challenge、package manifest SHA 和显式 RepositoryRoot 立即调用；RepositoryRoot 与 OutputDirectory 不得重叠。只读采集 12 项 Windows 与 6 项 GPU/依赖检查，应用取版本、依赖取大小/哈希、GPU 用 CIM，不执行 PATH 工具 | `collectionComplete=true` 只表示列举检查均通过；v2 报告固定所有产品/runtime/真实 IO/剔除声明为 false |
| `scripts/run_windows_p8_preacceptance.ps1` | v4 wrapper 在目标 Windows/D 盘生成 64-hex challenge，复制 collector 到 provenance 后现场采集，不接受历史 host input；ownership/reparse 防护串联 preflight、soak、fixture release/rollback，并用外置 32-byte key 对 manifest bytes 计算 HMAC | key 必须位于 Package、EvidenceRoot、整个 DeploymentRoot 外；当前 Mac 仅做静态接线门，不能声称 Windows/D 盘通过 |
| `scripts/p8_windows_evidence_verify.py` | 同时使用外部 manifest SHA/HMAC/key 验真 v4 evidence；精确复验 preflight 顶层/claims/inputs/9 项检查、v2 host input、采集窗口、ownership markers 和 7 个 provenance 受信源；原子 import 写 v4 receipt | verifier 拒绝 key 位于 bundle 内；HMAC 只认证证据，不把主机事实报告升级为产品验收 |
| `scripts/validate_powershell_scripts.ps1` | 对 `scripts/` 与 `tests/powershell/` 的 `.ps1` 执行 parser、PSScriptAnalyzer 和 Windows PowerShell 5.1 兼容检查，并输出机器可读边界声明 | `windowsRuntimeClaimed=false`；Linux 静态门不证明 Windows、Qt、GPU、D 盘或硬件运行 |

`config/p8-local-gates-v1.json` 固定 `localOnly=true`、`realIoEnabled=false`、`realRejectEnabled=false`。预检退出码：

- `0`：请求的本地输入范围 ready；
- `2`：Windows/GPU 等外部输入缺失；
- `3`：输入非法、路径不安全或哈希错配。

## 连续运行 profile

`contract-test-v1` 只验证短语义：1×1、最低 0.12 秒、至少 4 个 samples、CPU ≥0.01 秒、至少 2 条 progress、最大 gap 0.5 秒、至少 1 session/128 frames、0.04 秒 warm-up、RSS 增长 ≤16 MiB。它不能写成长稳证据。

每轮同时记录两个不可混淆的时长：`durationSeconds` 必须等于已验真的 runtime summary 工作时长；`processLifetimeSeconds` 在真实退出点立即冻结，表示外层观测到的进程寿命，验证器要求工作时长不得超过它（只允许 0.05 秒数值容差）。瞬时 exit 0 没有 runtime contract 时 `durationSeconds=0`，不能被退出后的资源采样耗时抬高。Linux 先用 `ps` 枚举目标进程组，再逐 PID 读取 `/proc/<pid>/stat` 的 user/system ticks 汇总 CPU；只 sleep 并伪造 runtime summary 的负路径仍会因 CPU <0.01 秒失败。

`local-sdkfree-v1` 是不可由 CLI 放宽的正式本地连续门：

- 1 次 restart × 2 轮，每轮最低 300 秒，timeout 330 秒；
- 采样间隔 1 秒，RSS warm-up 60 秒，每轮至少 240 个完整 samples；
- `local-soak-progress.ndjson` 每轮至少 240 条记录、最大相邻/收尾 gap 5 秒，且至少完成 240 sessions/122880 frames；
- 同一连续进程组 warm-up 后 RSS 增长不超过 64 MiB；
- 磁盘余量至少 1 GiB，单轮输出不超过 64 MiB；
- 累计 CPU 必须单调且每轮增长至少 10 秒，CPU interval 数至少覆盖 required samples，并记录 CPU P50/P95；
- 正常轮成功率 100%；
- crash、timeout 和残留进程为 0；
- 每轮使用新目录；
- runtime summary 必须证明 received=processed、processed=OK+NG+error，无 error/drop/source/detector/observer/save failure，sink/archive/product-state 均等于 processed，且 `productStateOk/Ng/Error` 与 Offline 分桶逐项一致；
- 正式 project `run` 必须接收 `--compiler/--standard`，在自身受控临时目录编译项目 C++ runner，确认 8 个源码在编译期间未变化，再把 runtime kind、compiler path/size/SHA/version、standard/flags/include、8 个 source hash + snapshot 和 executable hash + snapshot 写入 evidence；外部 `--build-provenance` 只接受 contract test helper，不能给 project runner 旁路注入；
- toolDependencies 必须绑定当前 `p8_continuous_soak.py` 与旧 `p8_soak_evidence.py` 的 repo-relative path/size/SHA/snapshot；POSIX 仅接受解析到 `/bin/ps` 或 `/usr/bin/ps` 的绝对可信 `ps`，记录 canonical/invocation path、size/SHA、version probe、resource/tree argv 并快照 binary。helper 漂移、PATH shadow、dependency record 与 helper/ps snapshot/identity 篡改均失败；Windows native collector 不伪造 binary snapshot；
- evidence inventory 除 manifest 外不忽略任何 `.tmp`，tmp/symlink/FIFO 均失败；Windows 正常退出执行两次 fresh tree enumeration，clean gate 同时要求 no residual 与 enumeration success；回归明确覆盖第一次成功、第二次失败时不得写 clean；
- `sdkFree=true`、`realIoEnabled=false`、`realRejectEnabled=false`、`productAcceptanceClaimed=false`、GPU `not-collected`。

这些阈值是版本化的本地 SDK-free 工具阈值，不是产品指标，也不允许推断 Windows/GPU/生产吞吐。正式结果必须写入新的、被忽略的 evidence 目录，并在同一目录二次 verify。

本地 `soak-manifest.json` 与离线 `verify` 严格复算包内哈希、派生值、runtime/source snapshot 和当前源码绑定，但这条本地链没有 Windows v4 的带外 HMAC；恶意方若能协调重写整个 evidence 包，单靠包内 manifest 不能认证其原始性。因此本地 evidence 应保存在可信目录，跨信任边界时另存带外摘要或使用专门认证链。该限制是证据交接边界，不是当前工具失败。

## 正式本地连续证据（2026-07-27）

正式目录：`artifacts/p8-continuous-local-20260727-final-v2`。`soak-manifest.json` SHA-256 为 `2bd420fab44acbed98315ffac5cf6a2b22567dcae42d98e522be35de3a7bcdd4`；manifest inventory 为 29 个文件、3,654,771 bytes，根目录共 30 个文件（含 manifest）。self-verify、主代理独立 verify、独立 reviewer 两次 verify 均 PASS。

| 轮次 | duration / lifetime | samples / post-warm-up | CPU / RSS 增长 | progress / 最大 gap | sessions / frames | OK / NG / Error |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 300.046591 / 300.358913 秒 | 291 / 232 | 30.64 秒 / 16 KiB | 3786 / 0.085584 秒 | 3786 / 1,938,432 | 969,216 / 969,216 / 0 |
| 2 | 300.020852 / 300.027510 秒 | 292 / 233 | 23.51 秒 / 0 | 3623 / 0.098617 秒 | 3623 / 1,854,976 | 927,488 / 927,488 / 0 |

两轮 exit 0、无 timeout/crash/residual process，资源采样完整；累计 3,793,408 帧，Offline 与 ProductRuntimeState 的 OK/NG/Error 逐轮一致，error/drop/subsystem/save failure 全为 0。独立 reviewer `/root/p8_v2_reviewer` 最终 PASS，P0/P1/P2/P3=0/0/0/0；`/root/p8_gate_docs_audit` 对正式 evidence 的 QA/observability/cleanup PASS。其唯一非阻断 P3 是旧 `artifacts/p8-continuous-local-20260727-invalid-dependency-gap` 失败目录仍存在：该目录无 manifest、被 Git 忽略，只隔离保留作失败审计，不计 acceptance evidence，不引用、不与 `final-v2` 混用、不交付；删除仅按用户或留存策略另行执行。

## 部署结构

fixture release 工具使用以下隔离结构：

```text
root/
  releases/<release-id>/...
  releases/.<release-id>.staging-*/
  config/
  brands/
  data/
  logs/
  evidence/
  state/current-release.json
```

release 文件不可在验证后静默变化。activate/rollback 只在目标 release 完整哈希验证和可选 smoke 成功后，以同目录临时文件加 `os.replace` 更新 current 指针；失败必须保持旧指针。工具不得删除旧 release，也不得覆盖 shared 内容。

## 当前本地回归

- preflight：17/17；
- soak：9/9；
- fixture release：17/17；
- Windows wrapper/collector 静态契约：2/2；
- package manifest generator：7/7；
- Windows evidence verify/import：24/24；
- 连续运行防假绿：5/5；
- P8 Python 清单：81 项；当前精确 81/81 PASS。
- 公开 wrapper：C++17 contract run + self-verify + 独立二次 verify PASS；伪造 project provenance exit 2 且不创建 evidence。
- 当前返修门：`5f6a06a` hosted run `30219159920` 有效发现 Linux CPU tick/core-dump 两项缺口；未提交修复现已取得 Mac 与 Colima/Linux 原始 full、P8 81/81、独立 reviewer 和 QA/observability PASS。提交/push 与 hosted rerun 待完成。
- 独立 PowerShell 静态门：13 个 `.ps1` parser/analyzer 0 finding；PowerShell CLI 契约 7/7 PASS（不计入上述 81 项清单）。

2026-07-27 KI-048 与正式 `final-v2` 2×300 秒、独立 verify、正式 evidence QA/observability/cleanup 均已通过。提交 `5f6a06a` 的 hosted run `30219159920` 随后发现短 contract CPU tick 和 POSIX core-dump fixture 两项跨平台缺口；当前未提交修复的 Mac/Linux full、独立 reviewer 与 QA/observability 已通过，hosted rerun 尚未收口。因此正式本地连续 evidence 结论保持有效，最终提交门仍不标 PASS；P8/AC-08 整体继续进行中。

历史快照：HEAD `6886856` 最终 full gate PASS：P5 100/100、P6 17/17、P8 76/76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个 `.ps1` parser/analyzer 0 finding、CLI 7/7。collector 的 `Get-LockedFileSnapshot` 以完整 reparse 祖先链检查和 `FileShare.Read` 锁内 SHA 固化文件身份；wrapper 持 provenance collector 读锁贯穿执行并前后复算 SHA/复查 reparse。该时点最终 reviewer/QA 均 PASS，P0/P1/P2/P3=0/0/0/0；它不覆盖当前连续运行增量。

历史 70 项与 76 项范围均保留为审计轨迹；当前 P8 v2 已完成新 core/full、dependency provenance 独立复核和正式 2×300 本地连续工具证据，不能再以 HEAD `6886856` 或短 contract 替代当前 evidence。新旧两类本地结论都不构成 Windows/GPU/产品验收。

2026-07-26 的 PowerShell 静态门是后续独立增量，不由上述 reviewer/QA 结论自动覆盖。其本机实现、Windows PowerShell 5.1 兼容静态检查和 7/7 CLI 契约已通过。独立 reviewer `019f9a17-7e58-7350-9ec3-7373f3151f4f` 在两项 P3 和一项文档计数 P2 修复后最终 PASS，P0/P1/P2/P3=0/0/0/0；独立 QA `019f9a17-98ad-7f23-9c36-6d62a7fa49ec` 最终 PASS，P0/P1/P2=0/0/0。P8 仍不会在 Windows/Qt/GPU/D 盘和外部输入阻断关闭前完成。

## 跨机证据交接

完整目标机步骤见 `docs/windows-target-execution.md`。先在 package 外生成 manifest，并在 Package、EvidenceRoot、DeploymentRoot 之外准备 exactly 32-byte key（建议 `D:\CigVision-secure\p8-evidence.key`）。v4 wrapper 现场采集成功后，必须把终端打印的 manifest SHA-256 与 HMAC-SHA-256 都保存在 evidence 外，并通过独立安全渠道转移 key。复制 evidence 后先执行：

```text
python scripts/p8_windows_evidence_verify.py verify --evidence-root <copied-evidence> --manifest-sha256 <external-sha256> --manifest-hmac-sha256 <external-hmac> --evidence-key <outside-bundle-32-byte-key>
```

需要纳入受控本地证据库时执行 `import`，同样提供 SHA、HMAC、key 和独立 `--store-root`。导入器写入 `p8-windows-evidence-import-receipt-v4` 后原子发布到 `imports/<manifest-sha256>/`；已有同摘要导入只允许完整复验后的幂等返回，不覆盖篡改或不完整目录。

## 当前源码部署基线修复

- `config.ini` 使用有效 `[General]`；
- 配置与品牌根改为 `QCoreApplication::applicationDirPath()`，不再依赖调用者 cwd；
- build 只在输出目录缺少默认配置时播种 `config.ini` 和默认品牌；
- 默认品牌 `硬特醇` 有 `para.ini` 和模板目录；
- QRC 嵌入全部实际引用图标，UI 移除开发机绝对路径和不存在的变体资源。

这只消除了静态部署阻断，不等于完整 DLL/plugin/prerequisite 清单已验证。

## 仍需外部完成

- Windows Release 实际二进制及完整 Qt/HALCON/MVS/DAQNavi/VC runtime、CUDA/TensorRT/OpenCV 依赖审计；
- 使用 `scripts/run_windows_p8_preacceptance.ps1` 取得目标 D 盘 A→B→A 激活/回滚转录；
- Qt 页面、`product-session.json`、TensorRT profile 的真实运行；
- GPU 显存、模型兼容性、推理时延和长时资源曲线；
- 商业模型与第三方许可；
- 独立产品级 review、QA 和人工 UI 验收；本地工具独立门仍须在提交前完成。
