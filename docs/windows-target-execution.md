# P8 Windows 目标机执行与跨机证据导入

## 适用范围

本文是 P8 目标 Windows 主机的唯一操作入口，覆盖 package manifest、外置证据密钥、v4 wrapper 现场采集、D 盘 local-tooling 编排，以及跨机 verify/import。

当前工具结论只能是 `passed-local-tooling`。challenge、SHA-256、HMAC、host report 或 receipt 都不证明 Windows/Qt 产品功能、TensorRT/GPU 性能、商业准确率、生产长稳、真实相机、DAQNavi 或真实剔除通过。

HEAD `6886856` 最终 `./scripts/run_all_local_gates.sh --full` 已通过：P5 100、P6 17、P8 76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个脚本零 finding、CLI 7/7。这仍不代表下述目标 Windows 步骤已实际执行。

## 前置条件

- 使用目标 Windows 主机上的当前可信仓库快照，安装 Python 3、Git 和 Windows PowerShell 5.1。
- `Package`、package manifest、`DeploymentRoot`、`EvidenceRoot` 和 `EvidenceKeyPath` 不得经过符号链接、junction 或其他 reparse point。
- `DeploymentRoot` 和 `EvidenceRoot` 必须位于 `D:\`；`EvidenceRoot` 必须尚不存在。
- `EvidenceKeyPath` 必须是 exactly 32 raw bytes，且同时位于 `Package`、`EvidenceRoot` 和整个 `DeploymentRoot` 之外。不要使用 64 字符十六进制文本代替 32 字节 key。
- key 不得进入 Package、evidence bundle、Git、日志或普通工单附件；必须通过安全的带外渠道转移和保管。
- package 必须固定为 local-only 配置，`rejectEnabled=false`；`SoakCommand` 的可执行文件必须位于 package 内。
- wrapper 不接受 `WindowsInput` 或 `GpuInput`。不得手写、预采集或复用历史 host report。
- collector 的 `-RepositoryRoot` 为必填项，必须指向真实可信仓库根；它与 `OutputDirectory` 不得双向重叠。实际执行的是 evidence `provenance/` 中的复制 collector，不能从该脚本的 `$PSScriptRoot` 推导仓库根。

建议目录：

```text
D:\CigVision-inputs\package-B\
D:\CigVision-inputs\manifests\
D:\CigVision-secure\p8-evidence.key
D:\CigVision\evidence\
D:\CigVision\releases\
```

其中 `DeploymentRoot` 为 `D:\CigVision`，密钥放在独立的 `D:\CigVision-secure`，二者不得重叠。

## 1. 生成 package manifest

manifest 输出必须位于 package 外，父目录已存在且目标文件尚不存在：

```powershell
$repo = (Resolve-Path ".").Path
$package = "D:\CigVision-inputs\package-B"
$manifest = "D:\CigVision-inputs\manifests\package-B.json"

$manifestJson = & python `
  (Join-Path $repo "scripts\p8_package_manifest.py") `
  --package $package `
  --output $manifest
if ($LASTEXITCODE -ne 0) {
  throw "package manifest generation failed"
}
$manifestResult = $manifestJson | ConvertFrom-Json
$packageManifestSha = $manifestResult.manifestSha256
$packageManifestSha
```

`p8-package-manifest-v1` 对 package 做生成前后复扫，绑定完整文件集、相对路径、大小和 SHA-256，并拒绝空目录包、遍历错误、package/祖先链接、reparse point、大小写或 NFC 冲突、运行中变更和覆盖已有 manifest。

把 `$packageManifestSha` 保存在 evidence 外。package 内容或 manifest 发生任何变化后必须重新生成，不得手工修改或继续使用旧摘要。

## 2. 安全生成 32 字节 evidence key

以下命令兼容 Windows PowerShell 5.1，使用 `CreateNew`，目标已存在时会失败而不会覆盖：

```powershell
$evidenceKeyPath = "D:\CigVision-secure\p8-evidence.key"
$keyBytes = New-Object byte[] 32
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
  $rng.GetBytes($keyBytes)
  $stream = [System.IO.File]::Open(
    $evidenceKeyPath,
    [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None
  )
  try {
    $stream.Write($keyBytes, 0, $keyBytes.Length)
    $stream.Flush($true)
  }
  finally {
    $stream.Dispose()
  }
}
finally {
  $rng.Dispose()
  [Array]::Clear($keyBytes, 0, $keyBytes.Length)
}
```

先创建并加固 `D:\CigVision-secure` 的访问权限。不要打印 key 内容。目标机运行结束后，通过独立受控渠道把同一 32-byte key 安全转移到离线 verifier 所在机器，并与 bundle 分开保存。

## 3. 准备 Soak argv

`SoakCommand` 是 argv 数组，不是拼接后的命令字符串。第一项必须是 package 内文件；输出参数使用字面量 `{output_dir}`，由 soak harness 为每轮替换为新隔离目录。

```powershell
$exe = Join-Path $package "CigVision.exe"
$simulationManifest = Join-Path $package `
  "tests\fixtures\p6-simulation-samples.json"
$soakCommand = @(
  $exe,
  "--simulation-batch-manifest", $simulationManifest,
  "--simulation-output", "{output_dir}",
  "--simulation-reject-delay-micros", "500",
  "--simulation-queue-capacity", "4",
  "--simulation-target-output", "simulation-reject"
)
```

不得在 Soak argv 中启用真实 IO 或真实剔除。输入 manifest 应包含在 immutable package 中，或另有外部哈希绑定。

## 4. 运行 v4 Windows wrapper

```powershell
$deploymentRoot = "D:\CigVision"
$evidence = "D:\CigVision\evidence\p8-windows-20260726-001"

$wrapperParameters = @{
  Package = $package
  PackageManifest = $manifest
  PackageManifestSha256 = $packageManifestSha
  EvidenceKeyPath = $evidenceKeyPath
  SoakCommand = $soakCommand
  DeploymentRoot = $deploymentRoot
  EvidenceRoot = $evidence
  ReleaseId = "fixture-B-20260726-001"
  Rounds = 2
  Restarts = 2
  TimeoutSeconds = 300
}

& (Join-Path $repo "scripts\run_windows_p8_preacceptance.ps1") `
  @wrapperParameters
if ($LASTEXITCODE -ne 0) {
  throw "P8 wrapper failed; retain evidence only as failed diagnostics"
}
```

v4 wrapper 执行以下顺序：

```text
生成 64 lowercase hex cryptographic challenge
→ 创建 .wrapper-owner
→ 复制 7 个受信源到 provenance/
→ 立即用 provenance/collect_windows_p8_host_reports.ps1 子进程现场采集
→ 生成 host-inputs/.collector-owner 与 Windows/GPU v2 reports
→ preflight
→ soak
→ preflight-after-soak
→ release-package
→ release-verify
→ release-activate
→ 可选 release-rollback
→ 在内存生成固定 UTF-8 manifest bytes
→ 以 CreateNew + Flush(true) 写入 v4 wrapper manifest
→ 对同一 bytes 计算 SHA-256/HMAC-SHA-256并复读确认未漂移
```

wrapper 调用 provenance collector 时会传入真实仓库根，等价参数形态如下；这是说明性转录，不要求操作员另行手动执行：

```powershell
& $provenanceCollector `
  -RepositoryRoot $repo `
  -Package $package `
  -OutputDirectory (Join-Path $evidence "host-inputs") `
  -Challenge $hostChallenge `
  -PackageManifestSha256 $packageManifestSha
```

`RepositoryRoot` 与 `OutputDirectory` 必须互不重叠；collector 会拒绝不存在的仓库根、reparse 祖先或重叠路径。

两个 host report 分别为 `p8-windows-host-report-v2` 和 `p8-gpu-host-report-v2`，必须绑定：

- 本次 wrapper challenge；
- package manifest SHA-256；
- 同一 `captureId`、`capturedAtUtc` 和 `hostIdSha256`；
- provenance collector 的 schema、仓库路径和 SHA-256；
- Windows 精确 12 项、GPU 精确 6 项全部 `passed`。

collector 不执行 PATH 中的 python、git、qmake 或 nvidia-smi。它只读取应用文件版本，使用 `Get-CimInstance` 读取 GPU 设备/驱动事实，并对 HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV 等依赖拒绝空文件与 reparse point、记录大小和 SHA-256。报告固定 `productAcceptance=false`、`windowsRuntimeAccepted=false`、`gpuRuntimeAccepted=false`、`realIoTested=false`、`realRejectTested=false`。

collector 与 wrapper 都使用 challenge ownership marker，并在关键写入前反复检查 reparse chain。失败路径不递归删除输出目录或其中未知内容；失败目录只能作为诊断残留，不能当作通过证据。

wrapper manifest 不通过可被覆盖的文本写入路径生成：它先在内存序列化为无 BOM 的固定 UTF-8 bytes，再用 `FileMode.CreateNew` 与 `Flush(true)` 持久化；SHA-256 和 HMAC-SHA-256 直接针对同一组内存 bytes 计算，输出带外信任锚前复读文件并逐字节确认未漂移。该约束用于缩小签名前替换窗口，但仍不替代带外安全保存。

文件身份读取也使用锁定快照：collector 的 `Get-LockedFileSnapshot` 先后复查完整 reparse 祖先链，以 `FileShare.Read` 打开文件，并在句柄持有期间计算 SHA-256；wrapper 对 evidence/provenance 中的 collector 持同类读句柄贯穿子进程执行，执行前后从同一句柄复算 SHA，并再次检查 reparse 链。

需要 A→B→A 时，先保证 `D:\CigVision\state\current-release.json` 指向已验证的 A，再增加：

```powershell
$wrapperParameters.ExerciseRollback = $true
```

没有既有 current release 时不得使用该开关。

## 5. 带外保存 SHA、HMAC 和 key

成功结束时 wrapper 会分别打印：

```text
Record externally: wrapper manifest SHA-256 <64-lowercase-hex>
Record externally: wrapper manifest HMAC-SHA-256 <64-lowercase-hex>
```

立即把以下材料保存在 evidence bundle 之外：

- evidence 目录名；
- package manifest SHA-256；
- wrapper manifest SHA-256；
- wrapper manifest HMAC-SHA-256；
- 32-byte evidence key 的受控副本或安全存放位置；
- Git HEAD、目标机标识、操作人和时间。

SHA 与 HMAC 可以保存到受控工单或独立只读介质；key 必须走更严格的独立安全渠道，不能与 bundle 同介质裸放，也不能复制进 `D:\CigVision`、Package 或 evidence。bundle 内的 `.sha256`、HMAC 字段或 provenance 都不能替代这三项外部材料。

## 6. 拷回并离线 verify

按字节复制整个 evidence 目录，不改名、不增删文件、不重新保存 JSON。把 key 通过独立渠道放到 verifier 主机的 bundle 外路径。

```bash
python3 scripts/p8_windows_evidence_verify.py verify \
  --evidence-root /absolute/path/to/copied-p8-evidence \
  --manifest-sha256 <externally-recorded-wrapper-manifest-sha256> \
  --manifest-hmac-sha256 <externally-recorded-wrapper-manifest-hmac-sha256> \
  --evidence-key /secure/outside-bundle/p8-evidence.key
```

`scripts/p8_windows_evidence_verify.py` 必须来自独立取得并信任的仓库快照；绝不能执行待验 evidence 内 `provenance/p8_windows_evidence_verify.py`。verifier 会拒绝 key 位于待验 bundle 内，并要求 key 恰好 32 bytes。

exit `0` 只表示 bundle 通过当前 local-tooling verifier。verifier 会：

- 同时验证外部 manifest SHA-256 和 HMAC-SHA-256；
- 精确校验 v4 wrapper 顶层结构、claims、文件全集与 7 个 provenance 受信源；
- 精确校验两份 preflight 的顶层、claims、inputs 和 9 项成功检查，拒绝未知 claim、未知检查或缺项；
- 独立重验 Windows/GPU v2 host report 的 challenge、package、capture、host、time、collector、12+6 checks 和 false claims；
- 要求 `capturedAtUtc` 位于 wrapper `startedAt`/`finishedAt` 窗口；
- 复核 `.wrapper-owner` 与 `host-inputs/.collector-owner`；
- 严格核对 argv、profile、阈值、gate、每轮 run、真实 soak 产物、release/activate/rollback 与汇总守恒；
- 在结束前重读 wrapper manifest 并复扫整个 evidence tree。

任何 SHA/HMAC/key 不匹配、协调改写、未知字段、时间窗外采集、reparse/symlink、文件变更或产品验收过度声明都必须 exit `3`。

## 7. 原子 import

只有 `verify` 成功后才允许导入：

```bash
python3 scripts/p8_windows_evidence_verify.py import \
  --evidence-root /absolute/path/to/copied-p8-evidence \
  --manifest-sha256 <externally-recorded-wrapper-manifest-sha256> \
  --manifest-hmac-sha256 <externally-recorded-wrapper-manifest-hmac-sha256> \
  --evidence-key /secure/outside-bundle/p8-evidence.key \
  --store-root /absolute/path/to/p8-evidence-store
```

导入器先验证 source，再复制到同文件系统 staging，复验复制结果，写入 `p8-windows-evidence-import-receipt-v4` 后原子发布到：

```text
<store-root>/imports/<wrapper-manifest-sha256>/
```

receipt v4 绑定 `verifierSha256`、source manifest SHA、manifest HMAC、release/package 身份、Git/provenance、文件数和 `productAcceptanceClaimed=false`。同摘要重复导入只有在既有 bundle 与 receipt 完整复验后才幂等返回；不得覆盖损坏或冲突目录。

## 8. 完成判定

只有以下条件同时成立，才能记录“目标 Windows 主机 local-tooling evidence 已验真并导入”：

- wrapper exit `0`，v4 manifest 为 `passed-local-tooling`；
- 外部 manifest SHA、HMAC 与 32-byte key 已在 bundle 外安全保存；
- 可信 verifier 使用三项外部材料 exit `0`；
- 需要入库时 import exit `0` 且 receipt v4 复验成功；
- 记录仍明确 `productAcceptanceClaimed=false`。

即使全部满足，也不能关闭 Windows/Qt/GPU/SDK、真实数据、许可、硬件或产品验收项。HMAC 只证明持有 key 的流程认证了 manifest bytes，不证明主机检查之外的产品行为。
