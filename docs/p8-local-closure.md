# P8 本地稳定性与部署工具闭环

## 状态与声明边界

P8 当前只建立可在无 Qt、Windows、GPU 和现场硬件环境运行的本地工具闭环：

```text
严格输入/包预检 → SDK-free soak → fixture 发布包验证
→ 原子激活/回滚 → SHA/HMAC 证据 → 当前测试集 76 项
→ challenge/HMAC/v2/v4 返修后的最终 full gate 与独立 review/QA 待执行
→ 独立 PowerShell parser/PSScriptAnalyzer/5.1 兼容静态门
```

本地通过状态统一写作 `passed-local-tooling`。它不等于 AC-08 整体通过，不证明 Windows/Qt 产品、TensorRT/GPU、D 盘安装、生产吞吐、商业准确率、真实磁盘满恢复或现场 IO/剔除。

## 工具

| 工具 | 本地职责 | 禁止外推 |
| --- | --- | --- |
| `scripts/p8_preflight.py` | 严格 JSON/INI、外部 manifest SHA、包内路径/文件哈希、品牌和 local-only 安全门；严格验证 Windows/GPU v2 报告、本次 challenge/package 与同 capture/host/time/collector SHA | 不执行产品；`collectorAssertionsValidated=true` 只表示 collector 契约通过，`productAcceptanceChecked=false`、`semanticAcceptanceChecked=false` |
| `scripts/p8_package_manifest.py` | 在 package 外生成不可覆盖的严格 manifest；双重稳定性复扫并绑定完整相对路径/大小/SHA | 不证明 package 是产品发布包；package 或 manifest 变化后旧 SHA 立即失效 |
| `scripts/p8_soak_evidence.py` | 多轮独立输出、重启/超时/崩溃、RSS/磁盘/输出量采样和哈希清单 | GPU 固定 `not-collected`，短 CI profile 不是生产长稳 |
| `scripts/p8_release.py` | 生成/验证 fixture release；原子 activate/rollback；共享目录隔离 | artifact kind 固定 fixture，不称 Windows 产品发布包 |
| `scripts/collect_windows_p8_host_reports.ps1` | 由 wrapper 使用本次 challenge、package manifest SHA 和显式 RepositoryRoot 立即调用；RepositoryRoot 与 OutputDirectory 不得重叠。只读采集 12 项 Windows 与 6 项 GPU/依赖检查，应用取版本、依赖取大小/哈希、GPU 用 CIM，不执行 PATH 工具 | `collectionComplete=true` 只表示列举检查均通过；v2 报告固定所有产品/runtime/真实 IO/剔除声明为 false |
| `scripts/run_windows_p8_preacceptance.ps1` | v4 wrapper 在目标 Windows/D 盘生成 64-hex challenge，复制 collector 到 provenance 后现场采集，不接受历史 host input；ownership/reparse 防护串联 preflight、soak、fixture release/rollback，并用外置 32-byte key 对 manifest bytes 计算 HMAC | key 必须位于 Package、EvidenceRoot、整个 DeploymentRoot 外；当前 Mac 仅做静态接线门，不能声称 Windows/D 盘通过 |
| `scripts/p8_windows_evidence_verify.py` | 同时使用外部 manifest SHA/HMAC/key 验真 v4 evidence；精确复验 preflight 顶层/claims/inputs/9 项检查、v2 host input、采集窗口、ownership markers 和 7 个 provenance 受信源；原子 import 写 v4 receipt | verifier 拒绝 key 位于 bundle 内；HMAC 只认证证据，不把主机事实报告升级为产品验收 |
| `scripts/validate_powershell_scripts.ps1` | 对 `scripts/` 与 `tests/powershell/` 的 `.ps1` 执行 parser、PSScriptAnalyzer 和 Windows PowerShell 5.1 兼容检查，并输出机器可读边界声明 | `windowsRuntimeClaimed=false`；Linux 静态门不证明 Windows、Qt、GPU、D 盘或硬件运行 |

`config/p8-local-gates-v1.json` 固定 `localOnly=true`、`realIoEnabled=false`、`realRejectEnabled=false`。预检退出码：

- `0`：请求的本地输入范围 ready；
- `2`：Windows/GPU 等外部输入缺失；
- `3`：输入非法、路径不安全或哈希错配。

## 本地暂定阈值

短 CI profile 只验证编排逻辑：

- 正常轮成功率 100%；
- crash、timeout 和残留进程为 0；
- 每轮使用新目录；
- 输出量不超过配置预算；
- 磁盘余量不低于 profile 门槛；
- warm-up 后 RSS 增长不超过 profile 门槛；
- GPU 字段必须为 `not-collected`。

这些阈值是版本化的本地工具阈值，不是产品指标。后续 `local-sdkfree-v1` 长稳和 Windows 产品 profile 必须使用独立证据目录，并先确认运行时长、CPU/GPU、内存、磁盘、队列和 P95/P99 数值门槛。

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
- P8 合计：76/76。
- 独立 PowerShell 静态门：13 个 `.ps1` parser/analyzer 0 finding；PowerShell CLI 契约 7/7 PASS（不计入上述 76/76）。

P8 测试总数仍为 76，PowerShell 静态门计数仍为 13 个 `.ps1`、CLI 7/7。当前返修新增 challenge/package/time 绑定、现场采集、ownership/reparse 非递归失败边界、preflight 精确 9 项校验和 SHA+HMAC+key 三项外部认证；7 个 provenance 副本必须逐字节匹配 verifier 所在的当前受信仓库。外部 key 必须是 32 raw bytes，位于 Package、EvidenceRoot 和 DeploymentRoot 外；verifier 也拒绝 key 位于 bundle 内。返修前 full gate 曾通过，返修后的最终 full gate 与独立 reviewer/QA 尚待执行。GitHub Actions 未在线运行；真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV、数据、许可、硬件和 D 盘证据仍为外部阻断。

历史 70 项范围已通过最终独立门；该结论只保留为历史证据，不能覆盖本轮 v2/v4/HMAC 返修。返修后的最终 review/QA/full gate 完成前，P8 提交门保持未通过。

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
