# 评审结果

## 当前状态（2026-07-26）

- P5-02C3 双人复核真值晋级和 P5-02C5 临时 ONNX Runtime CPU 基线的独立 reviewer/QA 均已 PASS；相关历史章节中的“未开始”表述仅保留为当日记录。
- P5-02C7 受控输入就绪工具经多轮独立对抗返修后 reviewer/QA PASS；当前代码快照为 readiness 24/24、P5 100/100。工具通过不等于外部 artifact ready，KI-040 继续开放。
- P5 当前为外部阻断、未关闭；P6/P7 本地可完成切片已实现，剩余 Qt/Windows runtime 作为外部目标机阻断保留；当前阶段已切换到 P8。
- P7 产品状态已扩展到 8/8：新增 typed configured/applied profile、canonical/golden SHA-256、detector→batch→worker→state 帧级身份回传、严格 TensorRT v2 配置、品牌七阈值页面与持久化。本轮独立 reviewer 已确认参数 SHA 同源问题 resolved；Qt/Windows runtime 和 Computer Use 尚未完成。
- HEAD `6886856` 最终 full gate PASS：P5 100、P6 17、P8 76（preflight 17、soak 9、release 17、wrapper/collector 2、package manifest 7、evidence verify/import 24）、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个脚本零 finding、CLI 7/7。
- 2026-07-26 PowerShell 静态门当前在 Colima/Linux arm64 以 PowerShell 7.6.3、PSScriptAnalyzer 1.25.0 扫描 13 个 `.ps1`，parser/analyzer finding 0，7/7 CLI 契约 PASS，报告固定 `windowsRuntimeClaimed=false`。GitHub Actions 已在线执行；首次运行暴露的 analyzer 缺失用例隔离差异已改为显式 `ScriptAnalyzerModulePath` 并在本机复验。
- 当前工作树的 `IMAGEPROCESS_EXPORTS` 单点定义、两项目 `/utf-8` 和 22 份 TensorRT 历史原型免责声明已通过最新本地 `--full` 门；独立 reviewer `/root/review_release_warning_fix` 最终 PASS，本轮未另行声明独立 QA，且目标 Windows Release Rebuild 未执行。
- P8 仍不关闭：没有真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV、D 盘产品运行、真实数据、许可或硬件证据；`windowsRuntimeAccepted`、`productAcceptance`/`productAcceptanceClaimed`、真实 IO/剔除声明均为 false。

## 2026-07-26 Windows warning 源配置与历史原型免责声明

- 同代理文档核对确认变更范围与 `docs/known-issues.md` 一致：KI-022 的 22 份历史原型文档已统一降级为非证据资料；KI-028 仅完成源配置修复和静态防回退。
- 主代理的 `./scripts/run_all_local_gates.sh --full` 结果为 PASS：P5 100/100、P6 17/17、P8 76/76、C++17/C++14、20 次重复、ASan/UBSan。
- 独立 reviewer `/root/review_release_warning_fix` 首轮报告 P2=1、P3=1：原静态门只做 XML 文本匹配，且 KI-022 写成 5 份而实际覆盖 22 份。返修后 ElementTree 门确认无条件 `/utf-8`/`IMAGEPROCESS_EXPORTS`、条件覆盖继承占位和导出宏唯一性，计数同步为 22；最终 P0/P1/P2/P3=0/0/0/0，PASS。
- 当前结论仅为本地实现/静态回归通过。本轮未另行声明独立 QA，目标 Windows Release Rebuild 未执行，因此不判定 KI-028 runtime PASS，也不声明历史 TensorRT 原型具备当前构建、性能或交付效力。

## 2026-07-26 P8 主机报告与 v4 证据返修

- 原 reviewer/QA 指出历史报告可重放、preflight 可协调伪造、递归清理、PATH 执行、依赖 reparse 祖先链与 collector 执行窗口等风险；当前树已全部返修。最终增量 reviewer `019f9a55-8131-7423-96a7-44d934c28255` PASS，P0/P1/P2/P3=0/0/0/0；最终 QA `019f9a55-99a2-7011-887f-119893e3ec4f` PASS，P0/P1/P2/P3=0/0/0/0。
- v4 wrapper 不接受历史 `WindowsInput`/`GpuInput`，每次生成 64-lowercase-hex challenge，复制 collector 到 provenance 后以 mandatory `-RepositoryRoot $repoRoot` 立即现场采集 Windows/GPU v2 报告；RepositoryRoot 与 OutputDirectory 不得重叠，报告绑定 package manifest、同 capture/host/time/collector SHA。
- collector 不执行 PATH 工具；`Get-LockedFileSnapshot` 检查完整 reparse 祖先链，以 `FileShare.Read` 锁定文件并在锁内计算 SHA。wrapper 对 provenance collector 持读句柄贯穿执行，执行前后复算 SHA 并复查 reparse 链。
- verifier 精确重验 preflight 顶层、claims、inputs 和 9 项成功检查，拒绝未知 claim/检查或缺项；host capture 必须位于 wrapper started/finished 窗口。
- v4 evidence 需要 Package、EvidenceRoot、DeploymentRoot、bundle 外的 exactly 32-byte key。manifest 以 `CreateNew + Flush(true)` 写入固定 UTF-8 bytes，SHA/HMAC 对同一 bytes 计算并复读确认未漂移；verify/import 同时要求外部 SHA、HMAC 与 key，receipt 为 v4。
- 最终本地门已通过；真实 Windows/Qt/GPU/SDK/硬件仍未验收，P5 受控数据仍缺失，不能关闭产品验收。

## 2026-07-25 P8 cleanup 审计返修

- cleanup 初审：FAIL，4 P1、3 P2；相关实现修复将 P8 回归扩展到 70/70，并新增 `docs/windows-target-execution.md`。
- reviewer 返修复核继续发现 2 P1、2 P2：gate/external 身份绑定、release 精确 schema、import 根白名单及文档边界；修复后定向与全量门通过。
- QA 随后发现 1 P1：wrapper 曾提示执行待验 evidence 内 verifier。现已只提示使用独立取得并信任的仓库 verifier，并明确禁止执行 `provenance/` 代码；wrapper 静态门和手册同步修复。
- 最终 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c`：PASS，P0/P1/P2=0/0/0；P8 70/70、文档门和 diff PASS。
- 最终 QA `019f99d7-8a02-7da1-8641-2d533409d06d`：PASS，P0/P1/P2=0/0/0；wrapper 1/1 PASS。主代理定向安全切片 32/32、P8 全量 70/70。
- 主代理最终 full gate：`./scripts/run_all_local_gates.sh --full` PASS，覆盖 P5 100、P6 17、P8 70、C++17/C++14、repeat 20、ASan/UBSan、文档/P1/diff；`.ruff_cache` 已清除。
- 最终结论（历史时点）：当时 70 项 local-tooling 提交门 PASS；P8 整体因目标 Windows/PowerShell/Qt/GPU/D 盘、真实数据/许可/硬件外部阻断保持进行中。

## 2026-07-25 P7-01A 产品运行状态与最近结果复核

- 自查结论：SDK-free 状态模型的配置安全、参数 profile 身份、生命周期、统计守恒、容量边界、复核、非法枚举、复合相机身份、故障重启和并发路径通过 8/8。
- 源码审查：worker 到 UI 的 payload 现包含 frame/station/camera/cigarette/timestamps/parameter/defects/error；运行、复核、统计和诊断页只从状态快照读取；session 仅在启动、结束和复核时由 `QSaveFile` 原子写入；退出先停止离线 worker，在线停机或最终落盘失败会取消退出；没有删除旧控件。
- 边界：`realIoEnabled=true` 固定拒绝；当前 UI 构造固定 local-only，不初始化相机、DAQNavi 或真实输出对象。
- 降级项：当前主机无 Qt/MSVC，未编译或运行 Qt 页面；因此布局、信号槽、选择/复核按钮和重复启停只标为源码已实现，不计 runtime PASS。
- 首轮独立 reviewer/QA：FAIL。发现默认 UI 硬件初始化、在线停机失败退出、启动即停止丢失、active source 竞态、非法枚举、相机键碰撞、待复核计数、逐帧落盘和最终落盘失败门。
- 修复后独立 reviewer：PASS，首轮 8 项全部关闭，未发现新增 P0-P2；Offline/ProductState/Simulation 7/7 + 7/7 + 15/15、TSan、ASan/UBSan、启动窗口 BarrierSource stop 探针、文档门和 diff 均通过。
- 修复后独立 QA：PASS；C++14/C++17 strict、ASan/UBSan、ProductState 50/50、Offline 50/50、Simulation 20/20、文档门和 diff 均通过。两者均明确当前 Mac 无 Qt/MSVC，不声明 Qt/Windows runtime PASS。
- 主代理最终全门：文档/P1、P5 100/100、P6 17/17、contracts/offline/simulation/product-state 7/7 + 7/7 + 15/15 + 7/7、Python 编译、workflow YAML、相关 vcxproj/filters XML 和 `git diff --check` 全通过。

## 2026-07-25 P6 目标机证据编排器

- 实现范围：`scripts/p6_windows_simulation_evidence.py` 负责输入 preflight、Simulation batch、逐帧 JSON/PNG、`input-manifest.json`、`summary.json`、`simulation-trace.json`、trace preflight、六类 CLI rejection、日志和 SHA-256 evidence manifest；`scripts/run_windows_p6_simulation.ps1` 负责 Release 构建、依赖 DLL 搜索路径和 wrapper manifest。
- 测试：`tests/p6/test_p6_windows_simulation_evidence.py` 8/8 PASS；覆盖成功与负路径、六类运行产物/类型篡改（含 PNG signature）、`realIoEnabled` 篡改、错误负路径退出、`rejectEnabled=true`、已有 evidence root、非法 manifest、非 Windows 默认拒绝和 PowerShell 接线。
- 观察到的安全边界：驱动只传 `--simulation-*` 参数，配置要求 `rejectEnabled=false`，报告固定 `realHardwareUsed=false`、`realIoEnabled=false`、`accuracyMetricsClaimed=false` 和 `tensorRtEvidenceClaimed=false`；`--allow-non-windows-test` 只用于测试替身，结果标记 `passed-test-only`。
- 评审结论：实现自查和同代理 QA 式回归 PASS；该历史会话上层约束禁止启动新独立代理，因此不计独立 reviewer/QA PASS。当时 Mac 宿主机无原生 PowerShell/Qt/MSVC；2026-07-26 的 Colima/Linux PowerShell 静态兼容门不替代目标 Windows。目标 Windows runtime、产品 trace、MSVC 构建和现场硬件仍未验证，不能关闭 AC-06。

## 2026-07-25 P6-01B manifest/trace 只读预检加固

- 新增标准库 Python 预检：manifest 先核对相对路径、root containment、非 symlink regular file、SHA-256、去重和 station/camera/cigarette/delay/expected 范围；重复 JSON key 和非有限值也拒绝。
- trace 复算 schema、`realIoEnabled=false`、manifest/frame metadata/expected decision、Simulation command、scheduled/completed clock、failed error binding 和 observed/command/status 统计；report 原子写且拒绝覆盖 manifest/trace。
- 本机定向回归 9/9 PASS；仓库 4 图 fixture 的 manifest-only 预检 PASS，`--require-trace` 在产品 trace 尚未生成时按设计 exit 2。
- 同代理复核未发现新的实现 blocker；独立 reviewer/QA 受当前上层约束未执行，故结论保持 degraded。该工具不运行 Qt、MSVC、GPU、真实录制流或硬件，不能关闭 AC-06 或 Windows runtime 缺口。

## 2026-07-25 P5-02C6 工具跨平台回归与提交门硬化

- 实现范围：Pillow 位图降级兼容、P5 依赖固定、当前文档/issue ID/stale pending 门禁、P5 Linux local-gates workflow。
- 独立 reviewer：PASS；以 `git archive HEAD` 加最终 diff 建立 clean snapshot，复跑文档/P1/P5/C++ 门禁，无 P0/P1/P2 finding。
- 独立 QA：PASS；新 Python 3.11 venv（仅安装 `requirements-p5.txt`）76/76、`pip check` clean；双字体工厂同时抛错时完整视觉包仍生成；workflow 等价 C++ 7/7+7/7、YAML/结构检查和 tracked-file 检查通过。actionlint 在本快照未独立验证。
- 本机复核：`light_gate.py` 无 warning；P5 76/76；`git diff --check` PASS。该切片不扩大 Windows/MSVC、TensorRT/GPU 或现场硬件结论。

## 2026-07-25 P5-02C7 受控输入就绪与 fresh-clone 复核

- 独立 reviewer `/root/p5_independent_review` 首轮及迭代对抗发现：缺 `size_bytes` 假绿、断链 symlink 误报 missing、base input/目录内缺失错误使用 exit 2、default class catalog 未固定、attestation 必需字段与交叉绑定不足、父目录/深层祖先 symlink、`symlink/..` 词法折叠绕过、非目录 artifact 祖先、大小写/NFC-NFD 等价路径、artifact 根祖先 output、output symlink loop、untracked tests 未进 whitespace gate，以及 C7 文档 pending。以上均已返修并补回归。
- 最终实现门：模型与默认类别目录固定 SHA-256；整个受控路径扫描到四输入共同根；原始 `..`、目标/父级/断链/loop symlink 全拒绝；缺失受控根被普通文件祖先阻断时归 invalid；输出以绝对/解析、Unicode NFC+casefold 和 samefile/inode 三层规则拒绝受控文件、artifact 根及其祖先重叠；manifest schema、必需输出、大小/SHA-256、reviewed attestation 身份/时间/批准/source-pass1/GT/approved-manifest/catalog 绑定均检查。
- 最终 reviewer 复跑 readiness 24/24、P5 100/100、文档/P1 静态、`py_compile`、C++ 7/7+7/7、diff 和 light gate，gate PASS；无开放 P0-P3 implementation finding。
- 独立 QA `/root/p5_independent_qa` 对最终树复跑 24/24、100/100、strict fresh exit 2、success/missing/blocked/tamper/attestation/path/symlink/alias/output-overlap 矩阵、actionlint v1.7.12 和 workflow 等价步骤；artifact 前后快照一致，无 tracked 测试副作用，最终 PASS。
- 文档与 report 明确 `manifest_trust=internal-consistency-only`：工具不认证 manifest 与全部输出被协调替换。受控恢复必须另核对 evidence-manifest SHA-256；fallback 摘要已有历史记录，reviewed 摘要当前未提供。
- 最终结论：P5-02C7 工具切片 PASS；KI-040（外部 artifact/摘要缺失）和 KI-039（正式 TensorRT）继续开放，P5 整体不关闭。按用户要求，外部阻断不再阻塞 P6 本地实现；仍不声明商业效果或现场验证。

## 2026-07-25 P6-01A 核心回放与模拟剔除安全边界

- 新增可注入/可取消 pacer、逐帧 replay source、正向 simulation clock、Simulation-only output 与 reject trace observer。
- 本机 GCC/Clang 严格警告构建和 8/8 测试通过：元数据/节拍、非法输入、source/session 停止取消、端到端模拟命令、unsafe/throwing output、编号/时间边界和重复 frame-id 均覆盖。
- 当前结论是核心切片自查 PASS，不是 P6 完成或独立门结论；未接 Qt、真实相机、DAQNavi、真实 IO 或现场参数。

## 2026-07-25 P6-01B Qt manifest 与 Simulation CLI 源码切片

- `QtOfflineInput` 现在保留 station/camera/cigarette/delay 元数据；缺失字段沿用旧 manifest 默认值，显式 JSON 值拒绝错误类型、空标识、零编号、负延迟和超出 JSON 安全整数范围的延迟。
- `QtImageListFrameSource` 通过 `SteadyReplayPacer` 实现可取消的逐帧节拍；`main.cpp` 通过无 SDK parser 接入 simulation batch，并拒绝 fixture/TensorRT/simulation 冲突、重复选项和非法数值。
- simulation 入口只构造 `DeterministicFixtureDetector` + `SimulationRejectOutput`，不构造相机、DAQNavi 或 real output；`validateSimulationTrace` 先校验每帧绑定，再由 `QSaveFile` 原子生成 `simulation-trace.json`。
- 本机自查（该源码切片当时的增量门）：strict C++ contracts 7/7、offline 7/7、simulation 10/10；随后 P6-02 将同一 executable 扩展并回归到 15/15。无 Qt/Windows runtime 证据，因此本切片不关闭 AC-06、不声明 MSVC/现场通过。

## 2026-07-25 P6-02 多相机、异常与容量曲线 SDK-free 切片

- 同代理代码审查：`RealtimeLoadSimulator` 只依赖核心契约/队列，使用单 worker 虚拟时钟；没有相机、DAQNavi、Qt、TensorRT 或 Real output 构造路径。
- QA 式回归：多相机、arrival/frame/cigarette 乱序、重复/跳号、RejectNewest/DropOldest、P95 queue/end-to-end、stop drain/cancel、restart、invalid/overflow 和 validator mutation 均通过；simulation 15/15。
- 容量证据：固定 20 帧模型下 capacity 1/4/32 的 dropped 为 15/12/0；更大队列消除该输入的 drop，但 P95 wait 从 50 增至 720 µs，明确展示“容量换积压/时延”的模型内取舍，不外推为产品参数。
- 动态检查：C++14/C++17 strict PASS；优化构建 20/20；ASan/UBSan PASS。
- 文档同步后的综合门：文档门（预期 readiness exit 2）、P1 静态门、P5 100/100、C++17 contracts/offline/simulation 7/7 + 7/7 + 15/15、C++14 simulation 15/15、Clang C++14/C++17 15/15、ASan/UBSan 15/15、repeat 20/20、`py_compile`、XML/YAML、`light_gate.py` 和 `git diff --check` 全部 PASS。
- 独立门：当前会话受上层约束未启动新代理，故该结论是降级的同代理 review/QA，不计独立 PASS。P6 继续进行；Qt/Windows runtime、真实录制流和生产门槛仍未验证。

## 2026-07-25 P6-02 trace/validator 安全加固复核

- 代码审查发现并修复一处时间语义缺口：正 reject delay 下模拟回执曾可能早于命令计划时间；现在回执逻辑时间取 `max(now, scheduledAt)`，并由 validator 强制检查。
- 核心回放和 Qt source 两条入口均在 start 阶段拒绝超过 60 秒的单帧 delay；`RealtimeSimulation.h` 改为显式包含 `OfflineInspection.h`，消除单独 include 编译失败。
- validator 负路径新增/加固：命令烟支号与 trace 绑定、命令契约、NG 必须有 simulated/failed receipt、失败 receipt 错误绑定、逐相机计数/序列账目、queue/end-to-end max/P95 篡改；测试总数保持 15。
- 本机复核：C++14/C++17 严格 warning、Clang C++14/C++17、ASan/UBSan、优化重复 20/20、三个 SDK-free 头文件自包含编译均通过；随后复跑的完整综合门（文档/P1/P5、XML/YAML、light gate、diff）全部 PASS，受控 P5 readiness 仍按预期 exit 2。
- 评审状态仍为降级同代理 review/QA：当前会话受上层约束未启动新独立代理；Qt/Windows runtime、MSVC、GPU、真实录制流和现场硬件仍未验证。

## P3

状态：首轮 reviewer findings 已全部返修；同一 reviewer/QA 最终 PASS，P3 gate 关闭。

- 实现前只读 explorer：`019f4f73-404a-79d0-b267-a0d93a5d63b6`（UI/线程边界）与 `019f4f73-5447-7e80-a70d-62ebaf522a30`（样本/故障场景）。二者未修改文件，不计为最终独立评审。
- 实现自查修正：补充空输入测试；修复 `QVariantMap` typedef 编译错误；清除 OK 帧时旧 NG 预览；P3 脚本改为动态定位中文目录并按子进程退出码判定。
- 返修证据：`artifacts/p3-offline-20260711-132348`，顶层 passed/exit 0；两配置 7/7；主工程 Release Rebuild 0；8 图完整追踪/统计/哈希一致；非法 manifest exit 2；Computer Use 单图 UI 流程通过。
- 安全边界：不连接相机、不执行 DAQNavi 读写、不生成或执行 RejectCommand；`rejectEnabled=false`。fixture detector 不计为生产算法。
- 独立 reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 首轮 BLOCKED：停止竞态、worker UAF、协作者异常、不受控容量/停止排空、追踪不足，以及 UI/manifest/文档缺口。两轮返修复核确认全部 resolved，最终 reviewer gate PASS。
- 独立 QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 最终复算 37 项 manifest 文件记录全部一致，确认两配置 7/7、非法 manifest exit 2、完整追踪、UI QA 和 `rejectEnabled=false`，最终 QA gate PASS。
- P3 最终 gate PASS，阶段关闭并切换到 P4 待开始；不声明 TensorRT、准确率、相机、DAQNavi、HALCON 许可算子或真实剔除完成。

## P2

状态：实现自查、Debug/Release 契约测试、CigVision Release 回归构建和独立 reviewer/QA 均通过，P2 已关闭。

- 候选范围：`core/InspectionContracts.h`、`InspectionInterfaces.h`、`BoundedQueue.h`、独立契约测试工程和 Windows 取证脚本。
- 正式证据：`artifacts/p2-contracts-20260711-122211` 与 `artifacts/p2-main-regression-20260711-120926`。
- 明确排除：现有 picStruct/相机回调迁移、队列消费者、离线闭环、TensorRT、真实 IO 和剔除。
- 实现前只读 explorer：`019f4f56-4819-7a20-b9b0-16820addf9cf`（边界/迁移风险）与 `019f4f56-5c24-7712-b9fb-3c116d3ea189`（无硬件测试方案）。二者均未修改文件，不计为最终独立评审。
- 首轮 reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c` 判定 BLOCKED：安全枚举缺少闭集校验；队列拒绝会提前消费 move 所有权；并发/关闭测试不足；artifact 未绑定脚本。现已显式拒绝域外枚举、用 const/rvalue 重载在接受后才复制/移动、增加三组队列测试，并将确定命令及脚本 SHA-256 写入新 manifest。
- 首轮 QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b` 独立复跑两配置 4/4 并检查 PE 依赖后给出 PASS；其非阻断命令/脚本哈希 finding 已随 reviewer 返修一起解决。新证据两配置 7/7，等待同一代理返修复核。
- 最终 reviewer 返修复核：四项 finding 全部 resolved；Debug/Release artifact 复跑并各连续执行 20 次 7/7，无新增 finding，reviewer gate PASS。
- 最终 QA 返修复核：40/40 artifact 文件及六个源码/脚本哈希一致，命令非空；两配置各连续 20 次 7/7，PE 依赖无 Qt/Halcon/MVS/DAQ/TensorRT，QA gate PASS。
- P2 最终 gate PASS。阶段关闭并切换到 P3 待开始；不声明 picStruct/相机迁移、离线闭环、TensorRT 或真实剔除完成。

## P1

状态：P1 独立 reviewer、独立 QA 与提交门均已通过，P1 可关闭。实现代理自查和静态门不计为独立复核。

### 2026-07-11 Release 构建与启动候选

- HALCON 22.11.4.0 完整包保存于 D 盘，大小 `6516267818` 字节，SHA-512 与 MVTec 官方值一致；最小组件安装到 `D:\MVTec\HALCON-22.11-Steady`。安装器明确显示本机无许可证。
- 首次 Release MSBuild 暴露 `dllmain.cpp` 同时引入父目录和本地同内容 `paramStructs.h`，产生结构体重定义。两文件 SHA-256 相同；删除 `dllmain.cpp` 不使用的冗余 include 后，Release 构建通过。
- 当前正式证据 `artifacts/p1-windows-20260711-115033`：All 顶层 `partial`/exit 1；Debug 因 25.05 缺失 environment-check exit 1；Release environment/MSBuild exit 0，manifest 含 `CigVision.exe/process.dll` 哈希。
- CigVision 在 `rejectEnabled=false` 下启动并以 `startup-window.png` 记录主界面，没有点击任何应用控件。进程持续响应并加载 Qt 5.9.9、HALCON 22.11.4.0、MVS 4.8.0.3、DAQNavi 4.1.22.0；相机采集、实际 IO、许可算子和剔除均不声明通过。
- 当前结论：经同一 reviewer/QA 返修复核，P1 技术退出条件和提交门均通过。

### 2026-07-11 当前 Release 候选独立复核

- reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` 确认至少一个 Windows x64 配置的技术退出条件成立，并发现两项证据完整性问题：旧 manifest 仅记录 HEAD、不能绑定 dirty 工作树；脚本未捕获异常时可能错误写出 `passed`。现已在 manifest 记录 Git 状态及 `source-snapshot.diff` SHA-256，增加 `unhandled-exception` failure，并以 `115033` 重新执行 All/Release Rebuild。
- 同一 reviewer 发现 Windows 基线仍有“静态基线/工具集待 MSBuild”旧表述，已同步为 Release 实测基线。返修复核确认三项 reviewer finding 均 resolved，reviewer gate PASS。
- QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` 复核 `artifacts/p1-windows-20260711-115033`：Release environment-check 和 Rebuild exit 0，产物 SHA-256 与 manifest 一致；Debug 仅因 HALCON 25.05 Progress 缺失而阻断，All 保持 `partial`/exit 1。
- QA 确认启动证据仅支持 `CigVision.exe` 主界面可见、进程响应并加载 Qt/HALCON/MVS/DAQNavi 运行库；`process.dll` 已构建、输出和哈希，但启动模块列表未显示其已加载。没有执行 Run、相机采集、实际 IO 或剔除。
- QA 首轮发现两项文档门禁：旧 artifact 的 blocked 结论未明确归档，以及证据矩阵误把 `process.dll` 表述为已随程序加载。两项修正后，同一 QA 返修复核为 No findings，QA gate PASS。
- 最终门禁：reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` PASS；QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` PASS。P1 关闭；HALCON 许可证、Debug 25.05、MVS 相机采集、DAQNavi 实际 IO、重复启停和剔除仍按已知问题留待后续安全阶段。

### 2026-07-11 历史 `235900` 证据独立复核

- reviewer `019f4eec-cbd9-7263-bd2c-826506e81f10`：针对历史 `artifacts/p1-windows-20260710-235900`，两个 vcxproj 的属性传播、x64 配置和 XML 无新增确定性问题；发现当时的环境门主要检查文件存在，不能把已记录的实际版本表述为兼容版本门通过。当时 AC-01-01、证据矩阵和任务计划降级为进行中；该结论已被 `115033` 的 Release 实际 Rebuild 和启动证据更新，但仍不代表硬件兼容或功能通过。
- reviewer 同时确认本地 artifact 含主机名、用户名绝对路径和安装产品清单。未发现密码、令牌或密钥，且 `artifacts/` 已忽略；该目录只作本机审计证据，禁止提交，外发前必须脱敏。
- reviewer 的低严重度系统名称问题已修正：实际系统为 Windows 11 家庭中文版 23H2/build 22631；`Windows 10 Home China` 仅为注册表兼容字段值。
- QA `019f4eec-dfe0-74f0-8b1f-fcc6e0beae55`：针对历史 `235900` 首轮 no findings；返修复核发现基线仍有“审查后可能提交 artifact”的条件式措辞，已统一为只作本机审计、绝对禁止提交、外发前脱敏，并经同一 QA 确认 finding 关闭。该历史目录顶层 exit 1、两配置 environment-check exit 1、`Builds` 为空、无 MSBuild/输出/启动证据，因此当时 QA gate 为 FAIL/BLOCKED；此结论已被后续 `115033` 证据更新。

### 2026-07-10 Windows 验证尝试

当前正式取证：`artifacts/p1-windows-20260710-235900`。该目录由 VS2022 Developer PowerShell 17.11.2 的 All 运行生成，包含 `full-terminal.log`、两份环境日志和 `manifest.json`；顶层 exit 1，`OverallResult=failed-before-msbuild`。Qt 5.9.9、MVS 5.0.1 / SDK 4.8.0.3、DAQNavi header 与 x64 runtime 已检出；Debug/Release 均因指定 HALCON 完整文件集缺失而 environment-check exit 1。未进入 MSBuild、未生成输出、未启动程序，不能据此关闭 P1。

依赖获取补充事实：MVTec Software Manager 1.7.2 的下载包 SHA-512 已与官网校验值一致，登录后可识别 HALCON 22.11.4.0 Steady Windows x64 完整包；多次自动下载未形成完整文件，安装未开始。当前官方 HALCON 下载目录不列出 25.05 Progress，因此 Debug 依赖继续记录为外部阻断，不用其他版本替代。

本轮独立 reviewer `019f4c33-ecab-7591-976f-52c7cc1fd243` 与 QA `019f4c34-157d-77c2-b504-f98ef223e776` 均判定 P1 不能关闭，并发现 MVS 属性传播、All 提前终止、HALCON 预检过弱、DAQ runtime 证据缺失及评审材料冲突。集中返修后，同一 reviewer/QA 确认上述高风险问题已解决；随后补充混合结果 `partial` 语义和 MVS/DAQNavi runtime 环境门并生成 `213607`。最终门禁仍因 HALCON、MSBuild 与启动证据缺失而阻断。

以下为首次 Windows 取证的历史结果：脚本首次失败于 Windows PowerShell 5.1 对中文路径字面量的编码误读；修复后历史证据为 `artifacts/p1-windows-20260710-160725`。

历史 `160725` 结果：Debug 环境检查 exit 1，Qt、目标 Halcon、MVS 和 DAQNavi 均未检出；未进入 MSBuild、未生成 `CigVision.exe/process.dll`、未启动程序。该结果已被后续依赖补齐证据更新，但仍保留为审计轨迹。

本轮独立 reviewer：`019f4b0c-ee7e-7dd2-8fb9-b6cb96375da9`。结论：P1 不能关闭，AC-01-02 仍失败/阻断；脚本改动范围符合 P1，未触碰业务源码、TensorRT 或真实剔除逻辑；`artifacts/` 被忽略，但 artifact 内含机器名、绝对路径和安装产品清单，外发需脱敏。发现一个中等问题：`review-packet.md` 残留旧验证叙述，已改为当前 Windows 失败证据与此前静态证据分段。

本轮独立 QA：`019f4b0d-224e-7043-b47c-e0d3d168523d`。结论：证据只支持“Windows 取证脚本已在 VS2022 Developer PowerShell 中执行，并在 Debug 环境检查阶段失败”；不能支持“构建成功、Release 已验证、程序启动成功、输出文件生成成功”。QA 要求补充顶层命令退出状态字段并同步 `windows-build-baseline.md`，本轮已处理。

- reviewer：独立代理 `019f49ea-1bf9-7f60-971e-fd69f6b0e57c`。
- QA：独立代理 `019f49ea-1ba5-7693-a15c-8036c6003ae7`。

| 严重度 | 首轮发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | process 工程缺少父目录 include，`<paramStructs.h>` 无法解析 | 已给 Debug/Release x64 增加 `$(ProjectDir)..`；静态复核通过，仍待 MSBuild |
| 阻断 | Stop/析构与活动相机回调存在竞态 | 已增加活动回调计数、进程期守卫和等待屏障；静态复核通过，仍待 Windows/MVS QA |
| 高 | 多个 atomic 编号字段不能形成同一触发周期快照 | 已改为互斥保护的单次结构体提交/读取；静态复核通过 |
| 高 | IO 初始化失败仍被上层报告成功，用户 Stop 不结束读取任务 | 已改同步 initialize、Run 时 prepareStart、Stop 时 requestStop + 等待；静态复核通过 |
| 高 | 组件 1 可产生 100-255，组件位置无效时静默按 0 处理 | 原始编号大于 99 明确丢弃；派生编号使用 0-99 环绕；静态复核通过 |
| 高 | Stop 和启动回滚忽略 SDK 返回值 | 已记录返回码；Close 返回 Stop/Close/Destroy 首个失败；仍待 Windows QA |
| 高 | 临时灰度缓冲的 Halcon 所有权缺少证明 | 增加 `CopyImage` 形成队列对象副本并捕获回调异常；仍待目标 Halcon 验证 |
| 高 | Stop 失败后 shutdown 仍可能释放裸回调 `pUser`，且运行中读取可变工位参数 | 回调改用进程期守卫并在 Stop/shutdown detach；Run 前校验并冻结 10-30/20-40 工位参数；静态复核通过，晚到帧仍待 Windows QA |
| 中 | 静态脚本遗漏 process include、回调屏障、快照及构建基线文档 | 已扩充两个验证脚本，返修后运行通过 |
| 后续阶段 | 队列无消费者、回调仍做整帧转换、真实剔除门不存在 | 分别由 KI-015、KI-023、KI-024 跟踪到 P2/P3/P5/P6，不作为 P1 已实现能力 |

首轮 review 明确指出：没有 Windows/MSBuild、Qt、Halcon、MVS 或 DAQNavi 运行证据。即使返修复核通过，AC-01-02 和 P1 阶段仍必须保持未验证，直到目标机完成构建与启动。

## P1 QA 返修复核

独立 QA 重新读取最终工作树并实跑 P1 静态门、文档门、`git diff --check`、`xmllint` 和 skill 严格门，全部 exit 0。未发现确定的成员引用、信号槽签名、C++ 语法或必现死锁问题；确认 Stop 失败锁定再次 Run、IO 故障 queued signal 安全停止、完整编号快照和 process 双配置 include 路径均存在。

返修期间又发现 Stop 失败 shutdown 的裸回调上下文风险和运行中工位参数竞争。第二轮返修把 SDK `pUser` 改为进程期存活的回调守卫，Stop/shutdown 会先 detach 窗口并等待活动回调；IO 任务在 Run 前校验范围并冻结工位参数。MVS 是否在 Stop 后产生会污染下一次 Run 的晚到帧仍由 KI-026 追踪，不能以静态通过替代运行证据。

## P0

状态：P0 独立评审与返修复核已通过。

源码审计结论已写入 `docs/code-audit.md`，覆盖新版 Qt、老版闭环和 TensorRT 原型。审计均为静态只读检查，不作为 Windows、GPU、相机或 IO 的运行通过证据。

## 独立评审记录

- reviewer：独立代理 `019f49b6-62f7-7320-8f43-228c21bfca93`。
- documentation maintenance：独立代理 `019f49b6-62b5-7511-9b1c-93f055de83bc`。
- 范围：P0 入口、需求、架构、计划、验收、证据、代码审计、验证脚本和提交门。

| 严重度 | 发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | 错误声称 `test_yolo_trt_v2.cpp` 不存在 | 已纠正、加入反证说明并经 reviewer 复核通过 |
| 阻断 | 评审包和证据矩阵未记录本次评审/最新门禁 | 已同步并经 reviewer 复核通过 |
| 高 | `git diff --check` 不检查未跟踪 P0 文件 | 验证脚本已增加未跟踪文件检查和业务目录变更检查，返修后运行通过 |
| 高 | 历史 TensorRT 文档的完成/性能声明缺少当前证据 | 已在 `code-audit.md` 明确历史资料边界，KI-022 追踪后续整理 |
| 中 | AC 状态和 QA/证据记录不同步 | 已同步为最终通过状态 |
| 中 | `AGENTS.md` 对深层文档导航不完整 | 已补充紧凑导航行 |
| 中 | 大范围历史文档重复和命名漂移 | P0 不做大清理；P4 指定唯一入口，P8 前强制清理 |

## 最终复核

原 reviewer 对四项返修逐项复核：错误 CMake 结论 resolved；最新评审与命令输出在写入本结论后 resolved；验证脚本覆盖 resolved；AC/QA/证据状态在本次同步后 resolved。documentation maintenance 同时确认过期 warning、AC-00-07、AGENTS 导航、历史 TensorRT 声明边界和状态一致性均已解决，历史目录大清理留到 P4/P8。

P0 已通过独立评审和提交门，可以关闭；Windows、GPU、相机、IO 和算法效果不属于 P0 通过声明。

当前不把实现代理自查计为独立评审。

## P4 评审状态

- 两名独立只读 explorer 已完成前置审计：确认 `[1,3,992,992] -> [1,300,6]`、9 类、旧 API 风险、无 ground truth 和 116/113 样本事实。
- 实现 worker 自查只计实现自查，不计独立 reviewer；其初版默认输出名错误已由编排代理依据 ONNX 本体改为 `output0`。
- 编排代理运行/视觉自查发现自写 resize 与 ONNX 差异，已切换 OpenCV 标准预处理并用 FP16/FP32/ONNX 对照复核。
- P4 独立 reviewer 与独立 QA 均已完成返修复核，最终 gate PASS；P4 技术集成关闭并切换到 P5 本地数据与算法效果闭环待开始。

### P4 首轮独立门禁

- reviewer `019f4ff6-8554-7700-992b-ab2773108818`：gate FAIL。阻断为残缺批处理 CLI 会回落普通硬件 UI；高优先级为配置在校验前可能参与巨量分配、正式证据未绑定未跟踪 P4 源码；中低项为 detector latency 命名错误、评审包顶层陈旧、`noexcept` logger 可抛。
- QA `019f4ff6-9972-7b73-83a5-a288e6714ab5`：首轮运行 gate PASS，独立复跑 116/116、361 个证据文件哈希一致、engine 负路径 exit 4、P2/P3 回归通过；视觉独立确认 frame 1/3/6/12 的重叠框、大框和文字遮挡。该 PASS 不声明准确率。
- 返修：批处理参数只要残缺/重复/冲突即 exit 2；JSON 尺寸和禁用类别先做精确整数/范围校验；detector 构造在任何 host buffer 分配前验证 config；P4 manifest 新增 10 个源码/工程/脚本 SHA-256；时延更名 detector latency；logger 内部捕获异常；评审包顶层状态已更新。
- 返修正式证据：`artifacts/p4-tensorrt-20260711-150510` 顶层 PASS，engine/尺寸 exit 4、残缺/冲突 CLI exit 2，源码哈希复算 0 差异。

### P4 最终复核

- 原 reviewer 逐项确认首轮 6 项均 resolved：CLI 不再回落硬件 UI；配置在分配前严格校验；10 个 P4 源文件哈希绑定；detector latency 命名准确；评审包顶层一致；logger `noexcept` 兜底。技术 gate PASS。
- 原 QA 在 `artifacts/p4-qa-independent-postfix-20260711` 独立复跑：外层/内层 exit 0；invalid engine/shape exit 4；missing/conflicting CLI exit 2；116/116、246 框、错误 0；364 个证据文件和 10 个 source hash 全匹配；最终 gate PASS。
- QA 再次视觉确认 frame 1/3/6/12 的重叠框、大框和标签遮挡。无 ground truth，因此这类观察转入 KI-030/P5 的本地标注、量化和优化闭环，不转换成未经证实的误检率或漏检率。

P4 关闭后的路线调整属于用户产品规划决定，不改变 P4 reviewer/QA 结论，也不被计为新的实现通过声明。P5-P8 的代码、效果、UI 和稳定性必须在各自阶段重新接受独立评审与 QA。

## P5-01 评审状态

- 实现 worker 自查不计独立评审；编排代理完成类别目录、真值 provenance、授权/复核声明、证据绑定、匹配守恒、macro 口径、REVIEW、测试发现和源码稳定性返修。
- 首轮独立 reviewer `019f505a-151e-79a3-8384-fadca72c4e4d`：gate FAIL，8 项发现覆盖真值 provenance、匹配守恒、证据绑定、REVIEW、类别元数据、macro 口径、PowerShell 假绿和 README 漂移。
- 首轮独立 QA `019f505a-3ef0-7ea3-830f-a320d7a7c3c0` 在返修前工作树 PASS；因源码随后改变，该结论只保留过程证据，不作为最终门禁。
- 返修后正式证据 `artifacts/p5-data-20260711-170640` 20/20、源码运行前后 7/7 稳定、顶层 PASS。
- 同一 reviewer 逐项确认首轮 8 项全部 resolved，独立 reviewer gate PASS。
- 同一 QA 最终证据 `artifacts/p5-qa-independent-20260711-170802`：主脚本/独立单测 20/20、对抗 12/12、证据门 12/12、源图哈希不变，QA gate PASS。
- P5-01 工具切片提交门关闭；P5 整体继续进行，人工真值、冻结 split、实际准确率、类别业务确认和许可仍为开放项。
- P4 最终结论：技术集成与可复现效果审查通过；不声明准确率，不证明相机、DAQNavi、HALCON 许可、在线节拍或真实剔除。

## P5-02 评审状态

- 实现自查：返修后 29/29，30 canonical、30 原图、30 预览，所有覆盖特征满足，源图/源码哈希稳定，30 个 preview 语义和哈希绑定；纯函数与一键脚本均通过。
- 视觉自查：复核包可读，但仍见大框、低置信框、重叠框和疑似误报；KI-035 开放。
- 独立 reviewer `019f50b8-2d80-7e10-af4a-cdbd9b12cddc`：首轮 gate FAIL 后多轮构造边界，所有可复现 finding 最终 resolved；`192148` 的 29/29、6 个源码/输入哈希、71 个证据哈希、30 个 frame/preview binding 和文档指针均复核一致，最终 gate PASS。
- 首轮独立 QA `019f50b8-417d-7653-8200-0a82c6991cfd`：早期结论因源码多轮返修过期；最终改由全新 QA `019f50ef-db01-7cb0-b37a-f1af708b8cde` 对代码冻结快照重跑。
- reviewer 首次返修复核确认原六项 resolved，但随后依次构造 frame `parameterVersion`、畸形标量、P4/COCO/catalog 布尔 ID 绕过，gate 持续 FAIL；全链严格类型门和最终证据已由同一 reviewer 复核，无新增 blocker，最终 gate PASS。
- 最终 QA `019f50ef-db01-7cb0-b37a-f1af708b8cde`：独立生成 `artifacts/p5-pilot-20260711-192926`，29/29、fresh manifest passed，gate PASS；额外对抗/视觉检查由代理报告但无独立逐项落盘 transcript，故不作数量声明。
- P5-02A/B 技术复核包门关闭；P5-02C 人工授权、业务映射和双人 reviewed 真值未开始，P5 整体继续进行且无准确率声明。
- 人工双人试标：未开始；P5-02 和 P5 整体均不能关闭，不声明实际准确率。

## 2026-07-11 P5 路线 documentation maintenance

- documentation maintenance agent 已检查 README、AGENTS 和完整记录系统，确认此前只有主计划部分完成路线切换，README、可观测性和后续 QA 场景仍存在旧 P1/P5/P6 表述。
- 已统一为 P5 数据与算法效果、P6 本地实时流与模拟剔除、P7 沿用现有风格的 Qt 产品功能/UI 重构、P8 本地稳定性/部署/交付预验收；历史 P0-P4 条目保留并标注历史边界。
- 该 documentation maintenance 条目发生在 P5 实现开始前，当时阶段标记唯一且为 P5；当前 `docs/task-plan.md` 唯一阶段标记已为 P8。该历史条目不虚构标注、准确率、UI、实时流、稳定性或硬件证据。
- `light_gate.py`、全工作树 `git diff --check`、旧路线扫描和 PowerShell 等价文档结构检查通过。`validate_project_docs.sh` 可由 Git Bash 启动，但其未跟踪文件检查与当前 Windows CRLF 工作树不兼容，明确记录为 degraded validator compatibility。
- 本轮只维护文档，不是 P5 实现评审，也不改变 P4 reviewer/QA 的既有结论。未提交、未推送。

## P5-02C1 首标工作台评审状态

- 首轮独立 reviewer：gate FAIL。阻断/高风险为 annotated 真值标志假绿、pass1 reviewed/approved provenance 污染、类别目录/持久化状态未绑定、预览/完成态编辑门不足和证据未绑定源码。
- 另一独立 reviewer/QA 首轮同为 FAIL：package/workspace/preview 未绑定、operator 可整批重署名、导出缺 state/revision、REVIEW 空备注、Host rebinding 和非原子工具输出。类别篡改与源码证据 finding 已在第一轮返修解决。
- 三轮返修自查：48/48；超限 JSON 拒绝目标测试连续 10/10；前端语法、Python 编译、PowerShell parser 和 diff 门通过。
- Browser 二轮返修自查：30 图可加载；模型预览四类编辑全只读；Alice 完成 1 张后 Bob 只改全局 operator，逐图仍为 Alice/completed_revision 1；29 张未完成时导出拒绝；console 0 warning/error。证据为 `artifacts/p5-review-workbench-20260711-203902`。
- 当前只证明本地工作台技术行为；一次性 QA workspace 中完成的 1 张 OK 不是责任人员人工标注、人工 QA 或 ground truth。
- 独立 reviewer/QA：首轮 FAIL 后三轮返修，最终均 PASS。
- reviewer 返修复核唯一残留 blocker 为预览态键盘 Delete 绕过；已加双层 original-mode 守卫并用 Browser 实测框数 4→4，等待同一 reviewer 最终确认。
- 深度 reviewer 新发现启动后替换同尺寸源图/preview 的运行期身份漂移；GET/save/export 现对 65 个绑定文件执行受控哈希门，畸形 Host 转为 400，原 reviewer 最终确认 resolved。
- 独立 reviewer `019f4fd0-7aa5-7af3-b82b-7b7ebb14ed55` 与 `019f511d-c5ed-7571-86e5-71372fd9d220`：最终 gate PASS，无开放 blocker。
- 独立 QA `019f4fd0-8ec7-7801-a493-e2d6797aea23`：48/48、运行期替换/Host、20/20 manifest、9/9 source、65/65 package，最终 gate PASS。
- 提交门：P5-02C1 技术切片关闭；P5-02C2/C3 未开始，P5 整体不声明准确率。

## 2026-07-18 P5 探索性分歧分析评审结果

- 独立 reviewer `019f73a2-d454-7d91-82be-3ac4de258711`：PASS；无阻断、高或中严重度 finding。确认双侧非真值门、IoU 0.5 空间优先确定性贪心、四类框结果、REVIEW 排除、身份/哈希绑定和禁用表述均正确。
- reviewer 的 P3 建议：正式测试原先未逐项锁定预测侧门、字段缺失、效果声明、非 pass1、IoU 边界和同 IoU 决胜。现已补齐，targeted 从 4/4 增至 6/6，全 P5 从 53/53 增至 55/55；建议关闭。
- 独立 QA `019f73a2-d5d2-74f2-9d7c-d9f4c39e53c4`：PASS；独立重算 30 图片、10 REVIEW、70/71 框和 101 结果行，六文件及三输入哈希一致，禁用指标术语扫描为 0。
- 技术 gate：PASS（仅探索性分歧包）。P5 整体仍进行中；该 gate 不把单标注员参考升级为真值，不授权训练、阈值/NMS 调整、正式效果或验收结论。

## 2026-07-19 P5 探索性分歧可视化评审结果

- 首轮独立 reviewer `019f7866-87b9-7d33-a78f-ed4a97d13167` 对候选目录 `artifacts/p5-exploratory-visual-pack-20260719-112152` 判定 FAIL：1 项 P1 为源图相对路径可用 `../` 逃逸；3 项 P2 分别为整包生成非原子、FreeType 失败后的默认字体可能被中文动态文本触发崩溃、关键失败路径测试不足。
- 返修采用受限目录 `resolve(strict=True)` 与 `relative_to` 双重边界验证、同父目录 staging 完整生成后原子重命名、异常清理、默认位图字体下动态文本 ASCII Unicode 转义，并把可视化定向测试扩充到 12 项。
- 同一 reviewer 对正式目录 `artifacts/p5-exploratory-visual-pack-20260719-113656` 完成返修复核：首轮 4 项 finding 全部 RESOLVED，最终 PASS，P0/P1/P2/P3 均无开放 finding。
- 独立 QA `019f7866-8956-7af3-b466-c578bce34bc6`：PASS，P0=0、P1=0、P2=0、P3=1。唯一 P3 是当前 Windows 会话缺少创建符号链接权限，符号链接逃逸子分支未取得运行态证据；`..`、绝对路径、分析输出和构建入口逃逸均已实际复现并拒绝，中途失败清理与默认字体中文动态文本降级均通过。
- QA 复核正式目录共 24 个文件，其中 manifest 绑定 23 个输出；23/23 输出 SHA-256 和大小匹配，19/19 源图绑定匹配，19 张案例可解码，CSV 与 HTML 本地链接一致。新旧两个正式审查目录的 24/24 文件逐字节一致。
- 最终本机门禁：可视化 targeted 12/12、P5 全量 67/67、`py_compile`、`node --check` 和 `git diff --check` 均通过。
- 技术 gate：PASS（仅 P5-02C4 探索性可视化切片）。该结论不是人工肉眼业务复核，不把模型输出或单标注员 pass1 升级为真值，也不授权训练、阈值/NMS 调整、正式效果指标或验收结论；P5 整体保持进行中。
## 2026-07-19 P5-02C3 双人复核真值晋级

- 人工事实：肖朗完成 30 图逐页标注，小狼逐页检查；旧工作台只保存肖朗。项目负责人批准这 30 图作为真实数据。原 pass1 保持不变，SHA-256 为 `E09708A8B6AB989E01F68E5B854EB673B66C12CB51448D5F55AABC6D33CC5E43`。
- 首轮候选 `122518` 未通过 reviewer 门：attestation 对时间依据表述过强，且 staging 写出后失败清理缺少测试；返修后候选 `123517` 的 reviewer 为 PASS。
- QA 对 `123517` 判定 FAIL：晋级脚本允许 `reviewed_at` 相对源 mtime 偏差 ±500ms。该门已改为 datetime 精确相等，并新增 `mtime + 250ms` 拒绝测试；实现哈希改变后未原地修改旧候选，而是重新生成 `artifacts/p5-reviewed-truth-20260719-124537`。
- 最终 reviewer `019f789f-575c-7ed0-ab72-809d63f11c1f`：PASS；4 项输入、2 项实现和 5 项输出绑定均通过，正式候选无开放 P0-P3 finding。
- 最终 QA `019f789f-7a77-7be3-a447-fc32133c0534`：PASS；独立 250ms 漂移探针正确拒绝，定向 8/8、P5 全量 75/75，正式 GT/predictions validator 均 error 0，无 staging 残留。
- 正式统计：30 reviewed、20 comparable、10 REVIEW excluded；OK/NG/REVIEW 各 10；正式 GT 35 框，REVIEW 的 36 个参考框只保留于原 pass1；正式 GT 中类别 7/8 为 0。标注人肖朗，复核人小狼。
- 最终结论：P5-02C3 复核晋级切片 PASS。P5 整体仍进行中；冻结 pilot 不用于训练或阈值/NMS 调优，下一步仅运行精确绑定 P4 模型、engine 与 detector config 的小规模 pilot 基线，不据此声明完整数据集或商业效果。

## P5-02C5 final independent review (2026-07-19)

- Initial reviewer: FAIL because `runtime_contract` was hash-bound but not semantically validated.
- Repair: strict schema/backend/provider/version/scope validation; model and predictions cross-hash checks; source runtime manifest hash check; detector-config identity check; explicit report classification; negative tests.
- Independent reviewer after repair: PASS.
- Independent QA: PASS, limited to provisional ONNX Runtime CPU fallback baseline.
- Regression after repair: 76/76 PASS; `git diff --check` PASS (line-ending warnings only).
- Evidence report SHA-256: `2bc9b7115ba67563308fcb70fe3c4435c8849b89346962b8148d6487917b24d3`.
- Evidence manifest SHA-256: `9a9fd67471413eebcd8cd57179a8048bf7e868f313035921459046b6735968d6`.
- Formal TensorRT baseline remains BLOCKED / NOT VERIFIED under KI-039.
