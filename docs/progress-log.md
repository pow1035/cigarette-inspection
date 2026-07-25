# 进度日志

## 当前状态（2026-07-26）

- 当前阶段已推进到 P8。P7 本地源码和 SDK-free 参数/状态契约已完成，剩余 Windows/Qt runtime 作为外部目标机阻断保留，不再占用本地实现主线。
- P7 产品状态已扩展为 8/8：新增 typed configured/applied 参数 profile、canonical SHA-256、golden 身份、帧级参数哈希漂移拒绝和 TensorRT adapter 投影。品牌七阈值改用独立页面并写入当前品牌 `para.ini`；当前 Mac 无 Qt/MSVC，页面和 JSON runtime 尚未声称通过。
- 完整仓库已重新克隆到本地，`main` 与 `origin/main` 同步，拉取完成时工作树干净；旧稀疏副本已移入系统废纸篓。
- P5-02C3 双人复核真值晋级与 P5-02C5 临时 ONNX Runtime CPU 基线均已完成独立 reviewer/QA 门禁；正式 TensorRT 基线仍按 KI-039 保持 BLOCKED / NOT VERIFIED。
- P5-02C6 历史快照为 76/76；加入 P5-02C7 readiness 24 项回归后，当前 P5 全量 100/100。文档门、P1 静态门、Python 编译、SDK-free C++ contracts/offline/simulation 7/7、7/7、15/15、workflow、`git diff --check` 与 `light_gate.py` 均通过。
- P5-02C7 最终独立 QA 与 clean-snapshot reviewer 均 PASS；P5 因 KI-039/KI-040、完整数据和业务确认转为外部阻断、未关闭，唯一阶段指针切换到 P6。
- P6-01A 已新增 SDK-free 录制序列回放和 Simulation-only 剔除安全核心；P6-01B 已完成 Qt manifest/trace、simulation CLI 和只读 preflight。P6-02 已新增确定性多相机容量模型，simulation executable 总计 15/15，P6 Python 回归现为 17/17。时间线复核确认 P6-01B 历史切片只完成同代理降级检查；后续 P7/P8 独立门未明确覆盖该切片，不能反向升级。Windows/Qt runtime 和产品运行产物仍未验证。
- `Local gates` GitHub Actions 定义当前覆盖文档/P1 静态、P5/P6/P8 Python 精确计数、无 SDK C++ 回归和 PowerShell 静态门；`6886856` 的首次在线运行暴露了“全局已安装 PSScriptAnalyzer 时缺失模块用例不能隔离”的 CI 差异，现改为显式 `ScriptAnalyzerModulePath` 隔离并在本机复验 CLI 7/7。Windows/MSVC、TensorRT/GPU 和现场硬件继续保留为目标机/人工门禁。
- 提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 已 push；2026-07-26（Asia/Shanghai）的在线 Actions `Local gates` 运行 `30169095184`（job `89706968923`）SUCCESS，全部步骤通过、check-run annotations 为空，原 Node 20 warning 已消失。workflow 的 checkout/setup-python v6 均固定完整提交 SHA；最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` PASS（P0/P1/P2/P3=0/0/0/0，对抗 18/18），QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS（P0/P1/P2/P3=0/0/0/0，对抗 17/17）。
- 提交 `6886856` 后的最终 `./scripts/run_all_local_gates.sh --full` PASS：P5 100/100、P6 17/17、P8 76/76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个脚本 parser/analyzer 0 finding、CLI 7/7。
- P8 安全返修使用 Windows/GPU host report v2、wrapper/receipt v4：每次生成 challenge，复制 provenance collector 后以 mandatory `-RepositoryRoot $repoRoot` 立即现场采集。collector 新增 `Get-LockedFileSnapshot`，检查完整 reparse 祖先链，以 `FileShare.Read` 锁定文件并在锁内计算 SHA；wrapper 对 provenance collector 持 `FileShare.Read` 句柄贯穿子进程执行，执行前后复算 SHA 并复查 reparse 链。
- v4 证据认证要求 Package、EvidenceRoot、DeploymentRoot 和 bundle 外的 exactly 32-byte key；manifest 在内存形成固定 UTF-8 bytes，以 `CreateNew + Flush(true)` 写入，SHA/HMAC 针对同一 bytes 计算并复读确认未漂移。verifier 精确复验 preflight 顶层/claims/inputs/9 项检查、v2 reports、采集窗口和 7 个受信 provenance；receipt v4 绑定 HMAC。以上本地门已在 `6886856` 之后执行最终 full gate。
- 独立 PowerShell 静态门当前在本机 Colima/Linux arm64 使用 PowerShell 7.6.3、PSScriptAnalyzer 1.25.0 扫描 13 个 `.ps1`，parser/analyzer finding 均为 0；CLI 契约 7/7 PASS。结果固定 `windowsRuntimeClaimed=false`，不并入 P8 76/76。
- 使用隔离的 Python 3.12 环境安装 ONNX Runtime 1.20.1，精确加载 SHA-256 为 `956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4` 的模型；`images`/`output0` 运行时形状为 `[1,3,992,992]`/`[1,300,6]`，CPU 零输入推理输出为有限 FP32。该检查只证明当前 Mac 可运行 ONNX CPU 诊断链，不是图像效果、正式 TensorRT 或性能证据；受控 reviewed-truth artifact 未随 Git 克隆，完整 pilot 复算仍需恢复该外部输入。

## 2026-07-26 - PowerShell parser/PSScriptAnalyzer 独立静态门

- 新增 `scripts/validate_powershell_scripts.ps1`：递归扫描 `scripts/` 和 `tests/powershell/`，逐文件调用 PowerShell parser，并在要求时强制加载最低 1.25.0 的 PSScriptAnalyzer；报告 schema 为 `cigvision-powershell-static-gate-v1`，明确 `scope=parser-and-static-analysis-only` 和 `windowsRuntimeClaimed=false`。
- PSScriptAnalyzer 显式启用 `PSUseCompatibleSyntax`，目标 Windows PowerShell 5.1；JSON 记录 `syntaxCompatibilityTargets=["5.1"]` 和 `scriptAnalyzerMinimumVersion="1.25.0"`。静态分析保留默认规则，仅透明排除三项与项目证据包装脚本约定冲突的低信号风格规则：`PSAvoidUsingWriteHost`、`PSAvoidUsingPositionalParameters`、`PSUseSingularNouns`。
- 修复两个实际 analyzer finding：`run_windows_p3_offline.ps1` 不再向 PowerShell 自动变量 `$args` 赋值，改用 `$buildArguments`；`run_windows_p4_tensorrt.ps1` 的重复输入索引改为显式 `$sampleIndex` 循环，避免闭包索引的未使用赋值告警。
- 新增 `tests/powershell/test_validate_powershell_scripts.ps1`，以子进程覆盖有效仓库 exit 0、畸形脚本 exit 1、PowerShell 7 三元语法被 5.1 门拒绝、缺失根目录 exit 3、缺失 scripts 目录 exit 3、空 scripts 目录 exit 3、缺 analyzer exit 2，共 7 个 CLI 契约；exit 2/3 均校验机器可读 JSON。
- 本机 Colima/Linux arm64 当前实测 PowerShell 7.6.3 + PSScriptAnalyzer 1.25.0：13 个 `.ps1`，parser finding 0、analyzer finding 0；CLI 契约 7/7 PASS。5.1 兼容静态门不等于 Windows PowerShell 5.1 实跑，也不证明 Qt、GPU、D 盘或硬件运行。
- `.github/workflows/p5-local-gates.yml` 已增加 `pwsh` 静态门和契约测试步骤；该条是 PowerShell 增量完成当时尚无在线结果的历史快照，当前 hosted workflow PASS 证据见本页“当前状态”的运行 `30169095184`。
- P8 测试集仍为 76 项；PowerShell 7/7 单独记录。2026-07-25 的最终 reviewer/QA 只覆盖历史 70 项，PowerShell reviewer/QA 只覆盖当时静态门快照，均不自动覆盖当前 challenge/HMAC/v2/v4 返修。
- PowerShell 增量独立 reviewer `019f9a17-7e58-7350-9ec3-7373f3151f4f` 在两项 P3 和一项文档计数 P2 修复后最终 PASS，P0/P1/P2/P3=0/0/0/0；确认 12 个脚本零 finding、CLI 7/7、exit 2/3 JSON、PowerShell 7 三元语法被 5.1 兼容门拒绝、文档防回退及文档门/`light_gate.py`/diff 全通过。
- 独立 QA `019f9a17-98ad-7f23-9c36-6d62a7fa49ec` 最终 PASS，P0/P1/P2=0/0/0；验证 12 个脚本零 finding、7/7 CLI、exit 2/3 JSON、5.1 三元语法拒绝、`windowsRuntimeClaimed=false` 和验证前后文件哈希一致。
- 最终 documentation maintenance 已同步 README、计划、证据、进度、P8 闭环、评审、QA、问题和可观测性边界；`./scripts/validate_project_docs.sh` PASS，`light_gate.py` 无 warning，`git diff --check` PASS。本轮切片集中、无临时缓存或重复工具漂移，cleanup 暂不触发；待目标机证据进入后重新判断。

## 2026-07-25 - P7-01A 产品运行状态与最近结果复核

- 新增 SDK-free `core/ProductRuntimeState.h`：显式 run/mode/detector/parameter/model identity、真实 IO 安全拒绝、Running/Stopping/Idle/Faulted 生命周期、逐帧与逐相机/逐类别统计、有界最近结果/诊断/重复窗口。
- `tests/CigVision.ProductState/ProductStateTests.cpp` 当前 8/8，覆盖配置、参数 profile/golden SHA、守恒、无部分写入、复核修订、非法枚举、station/camera 复合身份不碰撞、故障重启和并发；新增独立 MSVC C++14 test project并接入 Linux local gates。
- `OfflineInspectionWorker::publish` 现发送 frame/station/camera/cigarette/timestamps/parameter/defects/error；运行页从产品状态快照刷新计数，不再只信任 UI 回调中的累计数字。
- 复用现有 `btn_search` 和页面索引 4，新增沿用深色风格的最近 NG/错误表、复核人、备注、确认/需修正/误报操作。`docs/p7-ui-inventory.md` 记录所有顶栏/导航控件的消费者和保留决策；本切片没有删除或隐藏功能。
- 随后完成 P7-01B/P7-01C 本地源码：`btn_count` 展示运行/相机/类别统计，`btn_log` 展示结构化诊断；配置、状态、统计、最近结果、复核和诊断仅在启动、结束和复核时由 `QSaveFile` 原子写入 `product-session.json`；运行中品牌冻结，退出时先停止离线 worker。
- 首轮独立 reviewer/QA 发现默认 UI 仍初始化硬件、在线停机失败仍可退出、启动即停止可能丢失、活动 source 指针竞态、非法枚举、相机键碰撞、待复核计数、逐帧落盘和最终落盘失败门等问题。现已逐项修复：`main.cpp` 固定 local-only；在线停机或最终落盘失败取消退出；pending stop 与 active source 受保护；枚举、复合键、待复核统计和非逐帧落盘补回归/静态门。
- 修复后独立 reviewer 与 QA 均 PASS：C++14/C++17 strict、TSan、ASan/UBSan、offline/product 50/50、simulation 20/20 和启动窗口 BarrierSource stop 探针均通过；未发现新增 P0-P2。主代理最终全门为文档/P1、P5 100/100、P6 17/17、contracts/offline/simulation/product-state 7/7 + 7/7 + 15/15 + 7/7、Python/YAML/XML/diff 全通过。
- 当前只取得 SDK-free 编译/运行和 Qt 源码接线证据。Windows/MSVC 主程序构建、页面截图、键鼠流程和 Computer Use 尚未验证。
- P7 参数身份增量新增 `Sha256.h`、`ProductParameterProfile.h` 和 `docs/p7-parameter-profile.md`；TensorRT 配置升为严格 v2 并绑定完整 engine SHA-256、9 个逐类阈值与 profile 文件。fixture 运行明确保存 configured legacy profile 但标记未应用，避免把参数页值伪称为检测器参数。
- P8 当时新增严格 local-only preflight，历史快照 12/12；覆盖外部 manifest SHA、逐文件 size/hash、重复 JSON key、非有限数、`..`/绝对路径、symlink/reparse、case/NFC 冲突、`rejectEnabled=false`、品牌和输出隔离。配置根改为 executable 目录，修复 `%General`、默认品牌、空 QRC、缺失图标变体与 UI 开发机绝对路径。
- P8 SDK-free soak 9/9：多轮/重启使用新目录，记录 stdout/stderr、RSS、磁盘、输出量、超时/崩溃和进程残留；Windows 用 Toolhelp 跟踪父子 PID，First/Next API 失败与正常结束分离，枚举失败不能假绿；非有限 CLI 数值被拒绝；GPU 固定 `not-collected`。
- P8 fixture release 17/17：严格 manifest、单快照 hash/parse、最终复验、外部 source manifest 约束 staging 复制、路径/symlink/reparse/case/NFC 防护、原子 activate/rollback、失败保持 current 和 shared 隔离。该历史快照的 Windows wrapper 静态契约为 1/1。
- P8 Windows evidence v2 历史快照 24/24：覆盖外部顶层 SHA、可移植相对路径、Git/tool/gate provenance、子证据与 A→B→A 绑定、结束复扫、严格 run 守恒、真实 soak、receipt v2 和原子导入；该时点 P8 合计 70/70，目标 PowerShell/D 盘尚未运行。
- 第一轮 P8 documentation maintenance agent 曾在 earlier evidence v2 切片后修正阶段与 PASS 漂移，并固化“每个实现切片后、独立 review/QA 后、最终门禁前”三次文档维护节奏。
- cleanup 初审 FAIL 的 4 P1、3 P2 修复后，reviewer 返修复核又发现 2 P1、2 P2：gate/external 绑定、release 精确 schema、import 根白名单和文档边界；全部修复后 QA 再发现 1 P1，即 wrapper 不应提示执行待验 evidence 内 verifier。现已只提示独立可信仓库 verifier，并警告绝不能执行 provenance 代码。
- 主代理定向复核为 32/32。
- 最终 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c` PASS，P0/P1/P2=0/0/0，P8 70/70、文档/diff PASS；最终 QA `019f99d7-8a02-7da1-8641-2d533409d06d` PASS，P0/P1/P2=0/0/0，wrapper 1/1 PASS；该结论仅对应历史 70 项快照。
- 主代理 `./scripts/run_all_local_gates.sh --full` 最终 PASS：P5 100、P6 17、P8 70、C++17/C++14、repeat 20、ASan/UBSan、文档/P1/diff；`.ruff_cache` 已清除。
- 第二轮 documentation maintenance agent 在 cleanup 修复切片后同步了 70 项计数和 Windows 手册；后续维护轮次已同步当前 76 项、v2/v4/HMAC、文件锁、最终 full gate 与独立复核。最终 reviewer `019f9a55-8131-7423-96a7-44d934c28255` 和 QA `019f9a55-99a2-7011-887f-119893e3ec4f` 均 PASS，P0/P1/P2/P3=0/0/0/0。P8 整体仍等待 Windows/PowerShell/Qt/GPU/D 盘、真实数据/许可/硬件外部证据。

## 2026-07-25 - P5-02C7 受控输入就绪与 fresh-clone 复核

- 新增 `scripts/p5_input_readiness.py` 和 24 项定向测试。默认 CLI 固定模型与类别目录 SHA-256；对 reviewed-truth/fallback 检查整个受控目录路径、manifest schema、必需输出、非负大小、SHA-256、reviewed attestation 语义及 GT/approved-manifest/catalog 交叉绑定；`--output` 在写入前拒绝与受控文件、artifact 根或其祖先重叠，并保守覆盖大小写与 Unicode 等价拼写。
- exit 语义收口为：0=指定范围内部一致性通过；2=整个受控 reviewed/fallback 根目录未恢复；3=模型/类别目录缺失、artifact 目录内不完整、schema/hash/size/attestation 错配、`..`、目标/父目录/深层共同根 symlink 或其他非法输入。当前 strict fresh clone 为 exit 2，仅缺 `reviewed_truth` 与 `fallback_baseline`，invalid count 0。
- readiness 被接入 `validate_project_docs.sh` 和 Linux `P5 local gates`；文档门只允许上述两个外部根目录缺失，其他 missing/invalid/mismatch 均失败，未跟踪 whitespace 检查已覆盖新增 `tests/`。
- 独立 reviewer 在多轮对抗中发现并推动修复：缺失 `size_bytes` 假绿、断链 symlink 分类、base input/目录内缺失错误 exit、固定类别目录哈希、attestation 缺字段、父目录及 `symlink/..`/深层祖先绕过、非目录 artifact 祖先、case/NFC-NFD 等价别名、artifact 根祖先 output、output symlink loop、文档 pending 和 tests 未跟踪检查范围。最终代码复核 24/24、P5 100/100 及全门禁 PASS。
- 独立 QA 对最终树复跑 targeted/full、strict exit、临时 success/missing/blocked/tamper/attestation/path/symlink/alias/output-overlap 矩阵、actionlint/workflow 等价步骤和 artifact 前后快照；最终 PASS，未创建或修改仓库 artifact，也未留下 tracked cache/build/pyc。
- readiness 只证明 supplied manifest 的内部一致性，不认证 manifest 自身。受控恢复仍必须核对外部 evidence-manifest SHA-256；fallback 摘要已有历史记录，reviewed evidence-manifest 摘要当前未随 clone 提供。故工具切片通过，但 KI-040 与 P5 整体保持外部阻断、未关闭。

## 2026-07-25 - P6-01A 核心回放与模拟剔除安全边界

- 新增 `core/RealtimeSimulation.h`：可注入/可取消 replay pacer、逐帧 delay 的 `ReplayFrameSource`、正向 `SimulationClock`、只记录内存命令的 `SimulationRejectOutput` 和全链路 `SimulationRejectObserver`。
- 新增 `tests/CigVision.Simulation/SimulationTests.cpp` 8 项核心场景：元数据与节拍、空/非法输入、停止取消、4 帧端到端模拟剔除、unsafe/throwing output、零编号/时间溢出、重复 frame-id，以及 session stop 取消阻塞 source。
- 命令固定 `RejectMode::Simulation`；任何非法结果或非 Simulated 回执仅记录失败，不增加真实 IO 适配器，不接相机/DAQNavi。
- GCC/Clang `-std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread` 核心 8/8 PASS；后续 CLI 解析回归使 simulation executable 总计 10/10。

## 2026-07-25 - P6-01B Qt manifest 与 Simulation CLI 源码切片

- `QtOfflineInput` 增加可选 `stationId`、`cameraId`、`cigaretteNumber`、`delayBeforeMicros`；缺省时沿用 P3/P4 的 `offline`、文件名、顺序编号和零延迟语义，显式值先做类型/范围校验，local replay delay 上限为 60 秒。
- `QtImageListFrameSource` 支持注入 `SteadyReplayPacer`；停止会取消等待。simulation 入口使用 `--simulation-batch-manifest` / `--simulation-output`，并支持 `--simulation-reject-delay-micros`、`--simulation-queue-capacity`、`--simulation-target-output`。
- 新增无 SDK `core/BatchCommandLine.h`，保证 fixture/TensorRT/simulation 互斥并拒绝重复选项、负/溢出 delay、零/超限队列和空 target；Simulation 路径只构造 `SimulationRejectOutput`。
- 新增原子 `simulation-trace.json`：记录配置、运行统计、station/camera/cigarette、observed/scheduled/completed clock、Simulation command、回执和错误状态；`validateSimulationTrace` 拒绝计数、状态、clock 或 frame/receipt 绑定不一致。
- 本机验证（该源码切片当时的增量门）：strict C++ 编译后 contracts 7/7、offline 7/7、simulation 10/10；随后 P6-02 将 simulation 回归扩展到 15/15。Qt/Windows 编译、manifest 实际解析和 trace 文件运行证据尚未取得，不把代码检查写成 runtime PASS。

## 2026-07-25 - P6-01B manifest/trace 只读预检加固

- 新增 `scripts/p6_simulation_preflight.py`：不依赖 Qt/TensorRT，只读验证 manifest root/sample containment、SHA-256、重复/符号链接和 station/camera/cigarette/delay/expected 范围。
- 可选 trace 审计要求 `cigvision-simulation-trace-v1`、`realIoEnabled=false`、manifest/frame metadata 绑定、Simulation command、计划/回执时间、失败错误字段和计数守恒；输出 report 原子写且不能覆盖 manifest/trace。
- 新增 `tests/p6/test_p6_simulation_preflight.py` 9 项对抗回归，并接入 Linux local gates 和项目文档门。当前仓库 4 图 fixture 的 manifest-only 预检 PASS，缺产品 trace 时 `--require-trace` 按设计 exit 2。
- 该切片只提高目标机执行前后的输入/产物检查能力；不解码图片、不运行 Qt/MSVC/GPU，也不认证 manifest 来源。当前无独立代理门，保持 degraded review/QA 和 P6 进行中。

## 2026-07-25 - P6-02 多相机、异常与容量曲线 SDK-free 切片

- 新增 `core/RealtimeLoadSimulation.h`：用虚拟时钟模拟多相机输入进入单个有界检测 worker，逐帧记录 arrival/admit/start/complete、queue wait、end-to-end、queue/pipeline depth、drop/cancel/invalid disposition、相机和烟支编号。
- 序列审计覆盖 arrival 逆序、全局 frame-id 重复/前向跳号/回退、逐相机 cigarette 重复/跳号/回退；非法帧、processing overflow、零烟支号和 reject schedule overflow均显式失败。
- 容量矩阵固定为 20 帧、10 µs 到达间隔、50 µs 处理：RejectNewest 容量 1/4/32 的 processed 为 5/8/20、dropped 为 15/12/0、P95 queue wait 为 50/200/720 µs、P95 end-to-end 为 101/251/771 µs；该曲线只验证确定性模型，不代表真实吞吐。
- DropOldest 容量 2 为 processed 6、dropped 14、P95 queue/end-to-end 60/111 µs；逐帧 trace 可区分 newest reject 与 oldest eviction。
- stop boundary 分别验证 drain 和 cancel；同一 simulator 实例重跑后状态干净。trace validator 会拒绝 Real mode、未完成 disposition、早于计划时间的回执、命令烟支号篡改、逐相机统计和 P95 统计篡改。
- 本机 C++14/C++17 严格警告 simulation 15/15，连续 20/20，ASan/UBSan 15/15。当前会话受上层约束未启动新独立代理，故只记同代理审查/QA 式运行；Windows/Qt runtime 与独立 reviewer/QA 保持未验证。
- 文档同步后综合门最终通过：文档门（P5 readiness 预期 exit 2）、P1 静态、P5 100/100、C++17 contracts/offline/simulation 7/7 + 7/7 + 15/15、C++14/Clang simulation、ASan/UBSan、repeat 20/20、三个 SDK-free 头文件自包含编译、Python 编译、XML/YAML、`light_gate.py` 和 `git diff --check` 均 PASS。

## 2026-07-25 - P6 目标机证据编排器

- 新增 `scripts/p6_windows_simulation_evidence.py`：先调用 SDK-free manifest preflight，再执行目标 `CigVision.exe --simulation-batch-manifest`，核对 `input-manifest.json`、逐帧 JSON/PNG、`summary.json`、`simulation-trace.json` 和 trace preflight；拒绝 `rejectEnabled=true`，并把 real IO、TensorRT accuracy 和硬件能力固定标为不声明。
- 目标驱动为每次命令保存结构化参数、stdout/stderr、超时/退出码和 SHA-256；六类负路径覆盖缺少输出、fixture/Simulation 冲突、零队列、超限 delay、空 target 和残缺 manifest，并要求拒绝调用不创建输出目录。
- 新增 `scripts/run_windows_p6_simulation.ps1`：在目标 Windows 上调用 Release P1 构建、补齐运行时 DLL 搜索路径，再调用 Python 驱动并写 wrapper manifest；`-SkipBuild` 只用于已有 Release 产物的受控复跑，默认仍先构建。
- `tests/p6/test_p6_windows_simulation_evidence.py` 8/8 通过：成功路径、六类输出篡改/类型风险（含 PNG signature）、负路径错误退出、rejectEnabled、防覆盖、非法 manifest、非 Windows 默认拒绝和 PowerShell 接线均覆盖。该历史切片执行时 Mac 宿主机没有原生 PowerShell/Qt/MSVC；2026-07-26 的 Colima/Linux PowerShell 静态兼容门不替代目标 Windows。所有伪目标结果写为 `passed-test-only`，没有生成或声称 Windows runtime 证据。

## 2026-07-25 - P6-02 trace/validator 安全加固复核

- 修复模拟剔除正延迟时回执早于计划时间的问题：`SimulationRejectOutput` 现在先复核 command 契约，再以不早于 `scheduledAt` 的逻辑时间记录回执；失败回执同步保留 error code/message。
- `validateSimulationTrace` 现在核对命令烟支号、命令契约、NG 不得无回执、回执不得早于计划时间；`ReplayFrameSource` 和 `QtImageListFrameSource` 在非 Qt/Qt 两条入口都拒绝超过 60 秒的单帧延迟。
- `validateRealtimeSimulationSummary` 增强为重算逐相机 offered/accepted/processed/dropped/序列账目及 queue/end-to-end max/P95；现有 15 项测试内加入早回执、命令篡改、逐相机统计和 P95 统计篡改负路径。
- 修复 `RealtimeSimulation.h` 的隐式 include 依赖，三个 SDK-free 头文件均可单独 C++14 严格编译；GitHub Actions workflow 已把自包含头文件门纳入回归。

## 2026-07-25 - P5-02C6 工具跨平台回归与提交门硬化

- 修复 `p5_visual_disagreement_pack.py` 在 Pillow 11.3 FreeType/TrueType 加载失败时错误调用 `load_default()` 的问题，优先走 FreeType 无关的 PIL 位图字体；测试同时阻断 `truetype` 和 `load_default` 仍可生成完整视觉包。
- 新增 `requirements-p5.txt`（`Pillow==11.3.0`），并在 README、文档门和 Linux GitHub Actions 中固定安装/回归路径。
- `validate_project_docs.sh` 新增 requirements、重复 `KI` ID、详细 issue 对应、P5-02C4/C5 唯一编号、README/验收标准/证据矩阵当前状态和 stale pending 检查。
- 本机门禁：P5 76/76；文档/P1 静态、Python 编译、C++ contracts/offline 7/7+7/7、workflow YAML/等价结构检查、`git diff --check` 和 `light_gate.py` 全通过。
- 独立 reviewer 用 `git archive HEAD` 加最终 diff 建立 clean snapshot 后复跑上述门禁并 PASS；独立 QA 在新 Python 3.11 venv、完整字体失败注入和 workflow 等价命令下 PASS。该历史快照未直接执行 hosted Ubuntu，action/dependency 当时也未固定 SHA；后续提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 已完成 SHA 固定，并由运行 `30169095184` 在线验证。

## 2026-07-10 - P0 启动

- 项目已整理到 `/Users/c/Desktop/烟支检测项目_整理版`，Git 分支为 `main`，远端为私有仓库 `pow1035/cigarette-inspection`。
- 启动 `codex-long-task-architecture` 工作流，长期目标是按阶段完成新版 Qt 深度学习烟支检测闭环。
- Git 检查：工作树在 P0 编辑前干净，HEAD 为 `fde0eaa`。
- 本地体积检查：依赖包目录约 6.4G，继续保持 Git 忽略；现有 Git 目录约 321M。
- 三个独立只读审计已完成：新版 Qt、老版传统闭环、TensorRT/依赖。结果已汇总到 `docs/code-audit.md` 并映射到 `docs/known-issues.md`。
- 已创建 `AGENTS.md`、需求、架构、计划、验收、决定、问题、QA、证据、评审和可观测性文档。
- 首轮验证：`scripts/validate_project_docs.sh` 通过；`git diff --check` 通过；skill `light_gate.py` 返回 3 个评审包字段提示，正在补齐后复验。
- 独立代码审计只证明静态现状；Windows 构建、GPU、相机和 IO 仍未验证。
- 独立 P0 reviewer 发现两项阻断：错误的 CMake 文件存在性结论、提交门记录未收口；另发现未跟踪文件未被 `git diff --check` 覆盖。
- documentation maintenance 判定 P0 定向 cleanup 到期，大范围历史文档 cleanup 暂不到期。
- 已完成第一轮定向修复；增强后的文档验证、skill 严格门、脚本语法和 diff 检查均通过，业务目录限定状态为空。
- 原 reviewer 最终确认错误 CMake 结论和验证脚本覆盖问题已 resolved；documentation maintenance 确认 P0 定向 cleanup 已收口。
- AC-00-01 至 AC-00-07、QA、证据矩阵、评审包和评审结果已统一为通过，P0 关闭。
- 当前阶段切换到 P1：新版工程构建基线与硬化。P1 尚无 Windows 构建通过声明。

## 2026-07-10 - P1 静态基线与首批硬化

- 独立 explorer 完成 Windows 工具链静态审计：VS17/v143、Qt 5.9.9，Debug Halcon 25.05 与 Release Halcon 22.11 分裂。
- 新增 windows-build-baseline.md、P1 静态验证脚本和 Windows 环境检查 PowerShell。
- 修复 2-2 相机映射、回调帧元数据借用指针、数组释放路径和队列锁/容量。
- 相机改为 Run 显式启动、Stop 显式停止；析构停止相机和 IO 线程并清理资源。
- IO 编号改为互斥保护的一次性完整快照，组件编号统一使用 0-99 环绕计算。
- Release 补 MVS 路径/库；主工程增加 process ProjectReference，process x64 与主程序同目录输出。
- 陈旧 testWrite 从新版工程构建清单移除；config.ini 的 rejectEnabled 改为 false。
- 首次脚本运行发现 Bash 模板转义错误，未计入证据；修复后 P1 静态门、文档门、XML、shell 语法、diff check 和 skill 严格门均通过。
- 独立 reviewer 与故障场景 QA 已完成首轮，发现 process 父目录 include、回调退出竞态、编号快照、IO 初始化反馈和停止语义等问题。
- 已集中返修：process 父目录 include、活动回调屏障、Stop 后等待再清队列、IO 同步初始化/可重复启停、连续读取错误上限、完整编号快照、编号边界和 Stop 返回值记录。
- 返修后 P1 静态门、文档门、三个工程 XML、shell 语法和 diff check 再次通过；正在等待独立返修复核。
- 队列消费者属于 P3，轻量原始帧回调属于 P2/P3，模拟/真实剔除安全门属于 P5/P6，均保持开放，不计入 P1 已完成能力。
- 当前仍没有 Windows MSBuild、程序启动、相机或 IO 运行证据，P1 不能关闭。
- 新增 `run_windows_p1_build.ps1`，用于目标机自动执行两配置环境检查、MSBuild、关键输出核对和 SHA-256 证据清单；此条为静态切片历史记录，后续 Windows 取证尝试见下节。
- 原 reviewer 返修复核继续发现 shutdown 裸回调上下文和运行中工位参数竞争；已改为进程期回调守卫 + detach 等待，并在 Run 前校验/冻结 10-30、20-40 工位范围。
- 当前返修复核结果：P1 静态门、项目文档门、三个工程 XML、shell 语法、`git diff --check` 和 `light_gate.py --strict` 均通过；独立 QA 已复核回调屏障、IO 初始化握手、编号快照和停止失败处理。P1-03 静态切片关闭。
- P1 仍未关闭的唯一阶段证据是目标 Windows x64 的 Debug/Release MSBuild、输出文件哈希和程序启动记录；AC-01-02、KI-001 保持未验证，不能用 macOS 检查替代。

## 2026-07-10 - P1 Windows 验证尝试

- 按要求在 VS2022 Developer PowerShell 语境运行 `powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All`。
- 首次取证 `artifacts/p1-windows-20260710-155819` 失败于 Windows PowerShell 5.1 将 `.ps1` 中中文路径字面量误读为 mojibake，导致 `config.ini` 路径不存在；未进入依赖检查或 MSBuild。
- 已做最小 P1 脚本修复：`check_windows_build_env.ps1` 和 `run_windows_p1_build.ps1` 改为动态定位 `CigVision.sln`，避免中文路径字面量；`run_windows_p1_build.ps1` 在失败时也写出 `manifest.json`、环境日志和失败步骤。
- 最终取证目录：`artifacts/p1-windows-20260710-160725`。VS2022 Developer PowerShell v17.11.2 可启动，MSBuild 路径为 `E:\visual studio\\MSBuild\Current\Bin\amd64\MSBuild.exe`；manifest 记录 `OverallResult=failed-before-msbuild`、`OverallCommandExitCode=1`。
- Debug 环境检查 exit 1：`qmake.exe` 不在 PATH；`C:\Program Files\MVTec\HALCON-25.05-Progress` 不存在；MVS header/lib 不存在；`C:\Advantech\DAQNavi\Inc\bdaqctrl.h` 不存在。Release 未执行，因为 Debug 环境检查前置失败。
- 未进入 MSBuild，无 `msbuild-*.log`，无 `CigVision.exe/process.dll` 输出和 SHA-256 构建清单；未启动程序，未收集 UI/Computer Use 证据。
- 本机没有 `bash`/Git Bash，`validate_p1_static.sh` 和 `validate_project_docs.sh` 本轮无法执行，记录为 degraded；`git diff --check` 和 PowerShell 解析检查通过。
- 当前 P1 不能关闭，不能进入 P2；MVS 相机、DAQNavi IO、Halcon 运行时继续记录为未验证/阻断。

## 2026-07-10 - P1 Windows 依赖补齐与第二次取证

- Qt 5.9.9 msvc2017_64 已核验于 `D:\smokeqt\5.9.9\msvc2017_64`；QtMsBuild 3.2.0.47 位于 `D:\cigarette-inspection-deps\QtMsBuild\3.2.0-rev.47`，独立 Qt smoke exe 构建和启动 exit 0。
- 海康 MVS 5.0.1 / SDK 4.8.0.3 已安装；开发目录为 `D:\Hikrobot\MVS\MVS\Development`，共享 x64 runtime 由厂商安装到 Windows Common Files。未连接相机，不声明采集通过。
- 研华 XNavi 4.0.5.0 / DAQNavi 已安装到用户实际选择的 `D:\advantexh\DAQNavi`；`bdaqctrl.h` SHA-256 为 `37FFF4AEBA3C3777911F66A4CC718B1A83947EBA51AA4255337D041E9EECCA64`，系统 x64 `biodaq.dll` 版本 4.1.22.0。未枚举 PCIE-1730 或执行 IO 读取。
- 最小 P1 路径适配：新增 `CIGVISION_DAQNAVI_ROOT` 解析并将工程 DAQNavi include 改为该变量；同时沿用 `MVCAM_COMMON_RUNENV` 与 Qt 环境变量。未修改 IO、剔除、相机或 TensorRT 业务逻辑。
- `artifacts/p1-windows-20260710-212027` 首次证明 Qt/MVS/DAQNavi 已就绪，但 All 在 Debug 失败后提前终止，Release 仅有手工预检；该目录由后续取证替代为当前证据。
- 独立 reviewer/QA 指出 MVS 路径未显式传给 MSBuild、All 提前终止、HALCON 仅检查空目录、DAQ runtime 未固化进 manifest 和评审文档冲突。已做集中返修。
- `artifacts/p1-windows-20260710-213048` 完成首轮集中返修取证；随后补充 MVS/DAQNavi runtime 环境失败门和混合结果 `partial` 语义。
- 当前正式取证目录为 `artifacts/p1-windows-20260710-213607`。All exit 1，manifest 同时记录 Debug/Release 两个 environment-check failure；两配置分别检查 D 盘目标 HALCON 的 C++ header、x64 lib 和 runtime，均缺失。MVS/DAQNavi x64 runtime 在两份环境日志中均通过。
- manifest 已固化 MVS runtime 4.8.0.3、DAQNavi x64 `biodaq.dll` 4.1.22.0 及 SHA-256；MSBuild 参数显式传入 MVS、DAQNavi 和两套独立 HALCON 根。
- 未生成 msbuild 日志、`CigVision.exe`、`process.dll` 或输出哈希，程序未启动。MVTec 官方下载要求 MVLogin；两套 HALCON 不擅自统一，HALCON 12 不使用。
- P1 继续阻断，不能进入 P2。MVS 相机、DAQNavi 实际设备/IO、HALCON 运行时均保持未验证；真实剔除继续禁止。
- 独立 reviewer `019f4c33-ecab-7591-976f-52c7cc1fd243` 与 QA `019f4c34-157d-77c2-b504-f98ef223e776` 完成返修复核：此前高风险问题均已解决；P1 因 HALCON、MSBuild、输出和启动证据缺失继续阻断。

## 2026-07-10 - P1 依赖获取与正式复验

- MVTec Software Manager 1.7.2.1371 下载包 SHA-512 已与官网校验值一致；程序路径和数据/缓存路径分别设置为 `D:\MVTec`、`D:\MVTecData`，未在 C/E 盘安装 HALCON。
- 登录后的官方目录可识别 HALCON 22.11.4.0 Steady Windows x64 完整包和校验文件；自动下载多次中止且未产生完整文件，未安装、未计为依赖证据。当前官方目录不提供 25.05 Progress，不猜测、不用其他版本替代。
- 在 64 位 Windows 内核 `10.0.22631.0`、Visual Studio Community 2022 17.11.2（安装于 `E:\visual studio`）的 Developer PowerShell 中重新执行 All，正式证据更新为 `artifacts/p1-windows-20260710-235900`。目录含 `full-terminal.log`、`env-debug.log`、`env-release.log` 和 `manifest.json`。
- 顶层 exit 1，`OverallResult=failed-before-msbuild`；Debug/Release 均为 environment-check exit 1，仅各自 HALCON header/x64 lib/runtime 缺失。Qt、MVS、DAQNavi 和 MSBuild 检查通过。
- 未生成 `msbuild-*.log`、`CigVision.exe`、`process.dll` 或输出哈希，程序未启动。相机、DAQNavi 实际 IO、HALCON 运行时继续为未验证；真实剔除仍禁止，P1 不能关闭且不能进入 P2。
- 本轮独立 reviewer `019f4eec-cbd9-7263-bd2c-826506e81f10` 指出环境门主要验证文件存在，不能声明已证明版本组合兼容；AC-01-01 已降为进行中。其同时确认 artifact 含本机识别信息，只可本机留存、禁止提交、外发前需脱敏。
- 独立 QA `019f4eec-dfe0-74f0-8b1f-fcc6e0beae55` 首轮无新增 finding；返修复核要求将 artifact 规则统一为绝对禁止提交，修正后确认 finding 关闭。最终门禁为 FAIL/BLOCKED。Windows 名称已按 CIM/23H2/build 22631 修正为 Windows 11 家庭中文版。

## 2026-07-11 - HALCON Release 构建与安全启动

- HALCON 22.11.4.0 Windows x64 完整包下载到 `D:\download01`，精确大小 `6516267818` 字节，SHA-512 与官方值一致；离线仓库解压到 `D:\MVTecData\offline`。
- 通过包内 Software Manager 1.6.6 只安装 EULA、维护、Runtime、Development、HDevelop 和 VC++ 运行库，共 1.86 GB，目标 `D:\MVTec\HALCON-22.11-Steady`；未安装示例、HDevelop XL、Deep Learning、AI Accelerator 或接口包。安装器显示无 HALCON license。
- 首次 Release MSBuild 发现父目录和 process 本地的同内容 `paramStructs.h` 被同一编译单元引入，造成结构体重定义。两文件 SHA-256 相同；最小修复仅删除 `process/dllmain.cpp` 的冗余 include。
- 修复后 Release 定向构建 exit 0；正式 All 证据为 `artifacts/p1-windows-20260711-115033`，顶层 `partial`/exit 1，Debug 25.05 环境阻断，Release environment/MSBuild exit 0。
- manifest 记录 `CigVision.exe`、`process.dll` 及 SHA-256。CigVision 在 `rejectEnabled=false` 下成功显示主界面；`startup-window.png` 通过无交互窗口抓取生成，未点击任何应用控件。启动模块记录 Qt 5.9.9、HALCON 22.11.4.0、MVS 4.8.0.3 和 DAQNavi 4.1.22.0 已加载。
- 本轮 reviewer 发现证据未绑定 dirty 工作树及未捕获异常可能生成假 `passed`；脚本已记录 Git 状态和 `source-snapshot.diff` 哈希、增加 `unhandled-exception` failure，并以 `115033` 重新 Rebuild。QA 发现的旧 artifact 结论与 `process.dll` 加载措辞也已修正，等待同一 reviewer/QA 返修复核。
- HALCON 许可证、MVS 相机采集、DAQNavi 实际 IO、重复启停和剔除继续未验证，不随 P1 构建通过而升级声明。
- 同一 reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` 返修复核确认三项 finding resolved、reviewer gate PASS；同一 QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` 确认两项 finding 关闭、QA gate PASS。P1 提交门关闭，阶段指针切换到 P2 待开始；本轮未执行 P2 实现。

## 2026-07-11 - P2 数据契约与接口验收候选

- 新增 C++14 标准库 `core` 层：FramePacket 自持像素字节，Detection/DetectionBatch、InspectionResult、RejectCommand/RejectExecutionResult 带明确追踪、版本、错误和安全模式字段。
- 新增 IFrameSource、IDetector、IInspectionResultSink、IRejectOutput、IClock 可替换接口，以及有容量、溢出策略、丢弃计数、关闭和等待语义的线程安全 BoundedQueue。
- 当时的 P2 计划中，现有 picStruct、Halcon 图像队列和相机回调未替换，避免引入双路入队或越过后续阶段；未接 TensorRT、未增加检测消费者、未调用真实 IO。当前 P5-P8 路线以后文 2026-07-11 调整记录为准。
- 首次契约测试的 C++ Debug/Release 均 4/4 通过，但取证脚本在写 manifest 时因 PowerShell 5.1 中文路径字面量失败；改为动态定位 InspectionContracts.h 后重跑成功。
- 首轮正式契约证据 `artifacts/p2-contracts-20260711-120857`：Debug/Release MSBuild 和 test exit 0，Level4/WX，无 Qt/Halcon/MVS/DAQ/TensorRT 依赖。主程序回归 `artifacts/p2-main-regression-20260711-120926`：Release 环境检查和 Rebuild exit 0；原 P1 代码页/宏警告仍由 KI-028 跟踪。
- 独立 reviewer 首轮发现非法安全枚举可通过、队列拒绝提前消费 move 所有权、缺少真实并发/关闭测试及 artifact 未绑定脚本，gate 为 BLOCKED；独立 QA 技术 gate PASS，同时发现命令为空和脚本未哈希。
- 返修后正式证据更新为 `artifacts/p2-contracts-20260711-122211`：显式封闭 InspectionDecision/RejectMode，队列在接受后才复制/移动，并新增 move-only 拒绝、多等待者唤醒、关闭后排空和并发生产消费，Debug/Release 7/7 通过。manifest 记录确定命令和取证脚本自身 SHA-256，等待同一 reviewer/QA 复核。
- 同一 reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c` 确认四项 finding 全部 resolved、无新增 finding；同一 QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b` 复算 40/40 artifact 文件并独立连续运行两配置各 20 次，全部 7/7。两门均 PASS，AC-02-01/02 通过，P2 关闭。
- 阶段指针切换到 P3 待开始，本轮未实现离线回放、检测消费者、结果保存、UI 结果、TensorRT、相机适配或真实剔除。

## 2026-07-11 - P3 离线检测闭环验收候选

- 新增 C++14 `OfflineInspectionSession`：每次运行独立有界队列、图片源生产线程/检测消费线程、检测结果判定、统计不变量、显式停止和重复运行。
- 新增 Qt 图片列表 source、SHA-256 校验、深拷贝 FramePacket、`QSaveFile` 原子逐帧 JSON/PNG、汇总 JSON，以及独立 QThread worker。fixture detector 明确标注为非生产，只验证链路。
- 主程序新增 `--offline` 无硬件模式和 `--offline-batch-manifest/--offline-output` 批处理入口；前者跳过相机/IO 初始化，后者在构造主窗口前完成，不生成 RejectCommand，不触达真实 IO。
- 首次主工程编译发现 `QVariantMap` typedef 被错误前置声明，最小改为包含 Qt QVariant；Release Rebuild 随后通过。新适配器的非 ASCII 日志已改为英文，没有新增 C4819；旧 MyCamera/readIOTask 与 process 宏警告继续由 KI-028 跟踪。
- P3 取证脚本前两次分别发现 PowerShell 5 中文路径字面量和 stderr warning 误判问题；已动态定位 solution/config，并按子进程退出码判断。失败目录不作为正式证据。
- 首轮证据后 reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 判定 BLOCKED，指出停止竞态、worker 裸指针、协作者异常终止、不受控容量/停止排空和追踪字段不足；QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 限定 PASS 并要求补实际 UI 流程。
- 集中返修：state mutex 保证停止不丢失；QPointer 管理 worker；source/sink/archive/observer 异常转为统计/issue；固定容量 4 + 背压，UI 停止不排空；严格 manifest；输入清单与完整追踪 JSON。
- 返修正式证据 `artifacts/p3-offline-20260711-132348` 顶层 exit 0：Debug/Release 各 7/7；Release Rebuild exit 0；非法 manifest exit 2；8 图生成 input manifest、8 JSON、8 PNG、summary，processed=8、OK=4、NG=4、所有错误计数为 0。
- Computer Use 在 `--offline` 下实际选择一张固定原图和本地输出目录，预览显示，UI 为合格 1/缺陷 0/错误 0；`ui-qa-observation.json` 和三份输出文件哈希保存在返修证据目录。未初始化相机/DAQ，未调用剔除。
- `CigVision.exe --offline` 成功显示并响应，`startup/offline-window.png` 非空，未点击任何应用控件。该证据不声明 Halcon 许可算法、MVS 相机、DAQNavi IO、TensorRT、准确率或剔除通过。
- 当前为验收候选，等待独立 reviewer/QA；P4 尚未开始，未提交、未 push。
- 最终 reviewer 复核要求补 `source.start()` 异常边界和两处评审包陈旧文字；已新增 `ThrowingStartSource` 覆盖并以 `artifacts/p3-offline-20260711-132348` 最终重跑。
- reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 最终确认所有 findings resolved、gate PASS；QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 复算 37 项记录全部一致、gate PASS。
- P3 提交门关闭，阶段指针切换到 P4 待开始。本轮未接 TensorRT、未触碰真实相机/DAQ/剔除，未 commit、未 push。

## 已完成的历史工作

- 原始杂乱资料已整理为 `00` 至 `07` 和 `99` 目录。
- 缓存、重复内容和无必要构建产物已清理；安装包/运行依赖单独保留在本机目录。
- 本地 Git 初始化完成并推送到 GitHub 私有仓库。
- 大依赖包、超大图片压缩包和超大 PPT 已通过 `.gitignore` 排除普通 Git 历史。

历史条目只说明操作已经发生，不等于当前代码已通过构建或现场验收。

## 2026-07-11 - P4 TensorRT 正式集成验收候选

- 审计本机 RTX 4060 Laptop GPU、驱动 592.27、CUDA 13.1、TensorRT 10.15.1.29 和 OpenCV 4.9；模型本体确认输入 `images [1,3,992,992]`、输出 `output0 [1,300,6]`、9 个类别。
- 旧 `*_cache.engine` 由 TensorRT 8.6.1.6 生成，在 10.15 反序列化失败；非 cache `.engine` 无有效 plan tag。精确日志保存在 `artifacts/p4-audit-20260711-141545`。
- 由 `yanzhi20260120.onnx` 在本机生成 TRT 10.15 FP16/FP32 候选 engine；FP16 trtexec 随机输入基准 1391 查询、约 138.9 qps、host latency median 6.98 ms/p95 7.81 ms。该数字不是上位机端到端时延。
- 新增 TensorRT 10 `IDetector` 适配器、显式 detector JSON 配置和 `--tensorrt-batch-manifest` 离线入口；P3 fixture 入口保持不变。适配器校验 tensor 名称、模式、dtype、shape，检查 CUDA/TRT 返回，支持 Mono8/RGB8/BGR8 与 stride，串行保护共享 context。
- 首版自写双线性在模型 NMS 临界区域与 ONNX 参考差异偏大；改为 OpenCV 4.9 `INTER_LINEAR` 后，TensorRT FP32 与 ONNX 参考高度一致。12 图 FP16/FP32 检测数与类别顺序一致，最大置信度差约 0.02，因此选择 FP16 候选。
- 返修后正式命令 `scripts/run_windows_p4_tensorrt.ps1` exit 0，证据 `artifacts/p4-tensorrt-20260711-150510`：Release Rebuild 0、batch 0、缺失 engine/非法尺寸 exit 4、残缺/冲突 CLI exit 2；116 图全部处理，OK=27、NG=89、246 框、error/drop/save failure=0；116 JSON、116 原图、116 带框图，3 组重复输入结果一致；10 个 P4 源码/工程/脚本哈希已绑定且复算一致。
- 返修后 detector latency（mutex 等待、预处理、推理和后处理）：min 8.111 ms、median 33.593 ms、p95 95.783 ms、p99 112.084 ms、max 114.694 ms；不包含图片解码和保存。样本无人工 ground truth，以上 OK/NG 和类别分布不代表正确率。
- 视觉抽查 frame 1/3/6/12：框能落在明显破损/污点区域，但存在同一区域重叠框、超大框和文字遮挡；无标签条件下只记录为人工复核风险。
- P2 回归 `artifacts/p2-contracts-20260711-145147` Debug/Release 各 7/7；P3 回归 `artifacts/p3-offline-20260711-145147` 顶层 PASS。
- P4 实现阶段当时等待独立 reviewer 和 QA；其后续返修及最终结论见下两条。相机、DAQNavi、HALCON 许可、在线节拍、真实剔除均未验证或未触达。
- 独立 reviewer 首轮 FAIL 的 6 项已集中返修并逐项 resolved，技术 gate PASS；返修后独立 QA 在 `artifacts/p4-qa-independent-postfix-20260711` 最终 gate PASS，364 个证据文件和 10 个源码哈希全匹配。
- 一次 P3 回归 `artifacts/p3-offline-20260711-150846` 因与 QA 并发 Rebuild 默认输出目录发生 `process.dll` 竞争而失败；串行重跑 `artifacts/p3-offline-20260711-151220` 顶层 PASS。失败证据保留，不用于通过声明。
- P4 技术集成提交门关闭，阶段指针切换到 P5 待开始。本轮未开始后续实现；真实剔除继续禁止。准确率、中文业务映射和模型质量风险随后已按用户决定提前到 P5 本地标注与效果闭环处理。

## 2026-07-11 - 后续阶段改为全本地产品化路线

- 用户确认当前无法到现场，要求后续规划先在本地把商业项目做好：深度学习识别要有准确性证据、推理时间要短、效果要好，并允许优化 Qt 功能和 UI、清理无用功能、增加产品化能力。
- P5-P8 已重排为本地数据与算法效果闭环、本地实时流与模拟剔除、Qt 产品功能与 UI 重构、本地稳定性/部署/交付预验收。真实相机、DAQNavi、卷烟机同步和真实剔除移出当前排期并保持冻结。
- 老版 Qt/Halcon 传统算法只作业务规则和可选兜底参考；新版 Qt + TensorRT 深度学习继续作为唯一产品主线。功能移除必须先做清单、依赖分析和回归，不直接删除可能仍有业务价值的能力。

## 2026-07-11 - P5-01 数据与效果基线启动

- 检查点提交 `6e5f640`（P2-P4 离线闭环与 TensorRT 技术集成、README 和全本地路线）已推送 `origin/main`，随后才开始 P5，避免阶段成果混杂。
- 独立只读资料代理盘点了模型、116 图/113 唯一哈希、历史带框图、79.5 秒视频、传统算法规则、Qt UI 和技术报告；可用性与风险写入 `p5-source-inventory.md`。
- Qt 源码可支持前七类中文映射；`wuzi`、`jietou` 无可靠业务定义，已在机器目录中禁止正式标注。技术报告中的刺破、黄斑、长短、双层水松纸等九类外缺陷先进入 REVIEW。
- P5-01 采用 COCO JSON + 真值状态门：模型/P4 结果只能是 preannotated，只有人工 reviewed 且 `is_ground_truth=true` 才允许输出准确率。
- P5-01 实现自查后正式运行 `scripts/run_windows_p5_data_baseline.ps1`，最终证据 `artifacts/p5-data-20260711-164806` 顶层 exit 0：12/12 单测、116 文件/113 唯一/3 重复组/116 历史图配对，空真值 expected exit 2，P4 转换为 113 图/236 框 preannotation，1 个 0.000053 像素浮点边界框被保留原值并显式裁剪；评估只允许 manifest 中冻结的目标 split。
- 数据实际包含 60 张 992x300 和 56 张 1200x600，纠正了前置盘点“全部 992x300”的错误。236 个候选框中 99 个为未确认 `wuzi`，因此 P5 继续阻断准确率和自动阈值结论。
- 独立 reviewer 首轮 gate FAIL：预测 provenance 可被简单改状态绕过、逐类指标与混淆矩阵不守恒、报告缺模型/engine/配置/真值哈希、缺 REVIEW、类别目录绑定不全、FP-only 类宏平均遗漏、测试发现数/源码稳定性可能假绿、README 状态漂移。
- 返修增加 manifest/image 双重 approved 授权、不同标注/复核人、prediction 字段拒绝、双人复核 attestation 与 ground truth/prediction/manifest/catalog/model/engine/config/attestation 证据哈希绑定；混淆矩阵复用同类匹配并对剩余框做错类匹配；增加 REVIEW 排除计数、类别元数据/hash、active-union macro、测试数和运行前后源码哈希门。最终正式证据 `artifacts/p5-data-20260711-170640`：20/20、sourceFilesStable=true、failures=0。
- 同一 reviewer 返修复核确认首轮 8 项全部 resolved，reviewer gate PASS。最终独立 QA `artifacts/p5-qa-independent-20260711-170802`：主脚本 20/20、独立 20/20、对抗 12/12、证据门 12/12，116 张源图哈希不变，最终 gate PASS。
- P5-01 工具切片关闭；P5 整体保持进行中。下一切片仍需人工 20-30 图试标、业务确认 `wuzi/jietou`、数据授权和冻结 split，当前没有实际准确率。
- documentation maintenance agent 随后完整检查并维护 README、AGENTS 与 `docs/*.md`：清除 README 停在 P1、P5/P6 现场路线、P7/P8 归属漂移等当前计划误导；P0-P4 历史证据保持不变。
- 验证（当时的 P5 documentation-maintenance 记录）：长期任务 `light_gate.py` 无 warning；全工作树 `git diff --check` exit 0；唯一阶段标记为 P5；旧路线扫描无命中；PowerShell 等价执行文档结构、关键 ID、未跟踪文件空白和 P5 受限目录检查通过。当前阶段已在后续记录切换为 P8。
- 已通过 `D:\git\Git\bin\bash.exe` 启动 `scripts/validate_project_docs.sh`，但脚本的未跟踪文件 `git diff --no-index --check` 在当前 Windows CRLF 工作树中会把换行转换提示或 CR 字节判为 whitespace，不能形成可信的原命令通过证据；已用 PowerShell 等价复核并记录 degraded validator compatibility。该条记录发生在 P5 实现开始前；当前已进入 P5-02。

## 2026-07-11 - P5-02 本地试标集与人工复核入口

- `p5_dataset_tools.py select-pilot` 使用 `p5-pilot-greedy-cover-v1`：先按特征稀有度覆盖来源组、尺寸、预测判定和已出现类别，再按当前入选计数做确定性分层填充；平局由 SHA-256 和文件名裁定。
- 派生 `pilot-manifest.json` 只对入选 canonical 设置 `pilot`，重复别名继承 split 但不重复计数/复制；源 manifest 和 116 张源图保持只读。
- 首轮正式命令 exit 0，证据 `artifacts/p5-pilot-20260711-182436`；独立 reviewer 随后 gate FAIL，发现既有 split 覆盖、source_group 可伪造、canonical/alias 约束不足、代码/preview 未绑定、真值输入可混入和 manifest/CSV 门不足六项。
- 集中返修后 reviewer 确认原六项 resolved，又发现 frame `parameterVersion`、畸形标量和证据指针三项边界；继续补齐全局 detector version、`null bbox`/布尔 score 受控拒绝，并更新记录系统。
- reviewer 再次构造 `classId=true/confidence=true` 利用 Python `True == 1` 的严格类型绕过；已对 P4 classId/confidence/detectorVersion 增加非 bool、有限数、非空字符串门并补对抗测试。
- reviewer 进一步验证 COCO `category_id=true` 仍可与 P4 class 1 相等；现已把 category/image/annotation ID 和类别目录 ID 全部改为严格整数且非 bool，并在 preview 双向比较中严格校验 pilot category。
- reviewer 最后发现 class catalog 的 `false/true` 仍可借 Python 相等规则冒充 0/1；已在排序前增加 strict int/non-bool 校验和目录级对抗测试。
- 最终正式命令 `powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_pilot.ps1` exit 0，代码冻结证据 `artifacts/p5-pilot-20260711-192148`：29/29、30 canonical、30 原图、30 预览、71 个证据文件、6 个源码/输入文件绑定、30 个 preview binding、源图/源码前后稳定，catalog/COCO/P4 全链严格类型及 30 份逐框语义/frame detector version 一致。
- 原 reviewer `019f50b8-2d80-7e10-af4a-cdbd9b12cddc` 最终确认所有 finding resolved，无新 blocker，技术 gate PASS。
- 早期 QA 因多轮代码冻结更新失去时效；新 QA `019f50ef-db01-7cb0-b37a-f1af708b8cde` 从当前工作树独立生成 `artifacts/p5-pilot-20260711-192926`：29/29、fresh manifest passed，最终 gate PASS。代理还报告了额外对抗/哈希/视觉检查，但未单独持久化逐项 transcript，因此文档不声明额外检查数量。
- P5-02A/B 确定性选样与人工复核包技术切片关闭；P5-02C 人工授权、业务映射、双人标注/复核仍未开始，故 P5 保持进行中，KI-035/KI-036 保持开放，不声明准确率。
- 覆盖结果：source 1/21/22/unprefixed 为 8/4/10/8，992x300/1200x600 为 16/14，预测 OK/NG 为 12/18；class 0/1/2/3/4/5/7 为 3/10/1/8/6/6/11，稀有 class 2 唯一样本入选。
- 11 张含未确认 `wuzi` 的样本在派生 COCO 中强制为 REVIEW，并保留 `predicted_decision`；所有 image/annotation 继续为 `is_ground_truth=false`，不输出准确率。
- 视觉抽查确认标注包可读，同时仍见大范围框、低置信框、重叠框和疑似误报；已记 KI-035。独立 reviewer/QA 已完成，人工授权和双人复核仍未开始。

## 2026-07-11 - P5-02C1 localhost 首标工作台技术验收与返修

- 新增 Python 标准库 localhost 服务、显式 package/workspace Windows 启动器和沿用现有灰色工业风格的三栏标注界面；不改 Qt 产品 UI，不连接硬件。
- 服务端绑定 30 图文件名、SHA-256、尺寸、category/box ID 和 revision；原子保存草稿，拒绝遍历、畸形/超限 JSON、陈旧 revision、越界框、非法完成状态及含 7/8 未确认类别的 NG。
- 首轮导出统一为 `annotation_status=annotated`、`is_ground_truth=false` 并清除预测字段；`reviewed` 导出固定 409，要求授权和不同复核人。
- Browser QA 在一次性 workspace 只完成 1 张模型预测 OK 样本，验证保存、重载和 revision；29 张未完成时导出被拒绝，模型预览为只读，控制台 0 warning/error。该操作不是人工标注或真值。
- Windows 超限请求测试曾因未消费请求体触发 WinError 10053；服务端现只对不超过 2 MB 的超限体排空后返回 413，目标测试连续 10/10。
- 独立 reviewer 首轮 gate FAIL：`annotated + is_ground_truth=true` 可假绿、pass1 可继承伪造 reviewed/approved 字段且类别未绑定、持久化类别可篡改、预览/完成态编辑约束不足、证据未绑定源码。
- 返修建立 status/ground-truth 双向一致门；类别绑定本地 catalog/hash；状态类别严格只读；pass1 改为白名单重建且授权固定 unverified；预览禁用全部编辑；任何已完成框修改退回 pending；源码/dirty diff 纳入证据。
- 另一独立 reviewer/QA 对首轮快照还发现 package/workspace 未绑定、预览未校验、全局 operator 可整批重署名、导出未绑定 state/revision、REVIEW 空备注、Host rebinding 面和数据工具非原子输出。
- 二轮返修增加五个 package 文件指纹、30 preview provenance/hash/尺寸、workspace package_fingerprint、逐图服务端 annotated_by/completed_revision、防重署名、导出 state/package bindings、后端 REVIEW notes、Host allowlist，以及 JSON/CSV 原子替换/故障注入。
- reviewer 返修复核时发现预览态键盘 Delete 可绕过禁用按钮；现已在键盘分派和删除命令双层要求 original 模式，Browser 实际复现框数 4→4、无 console error。
- 深度 reviewer 又发现服务启动后同尺寸替换源图/preview 会造成显示内容与声明 SHA 不一致；现将 5 个 package 元文件、30 原图、30 preview 全部加入运行期绑定表，图片响应、save、export 均复核哈希，替换后受控 409；畸形 Host 受控 400。
- 三轮返修正式技术证据为 `artifacts/p5-review-workbench-20260711-203902`：48/48、超限 10/10、Browser console 0 warning/error、package 清单和 20 项 manifest。
- 独立 reviewer `019f4fd0-7aa5-7af3-b82b-7b7ebb14ed55` 最终复现运行期替换 GET/save/export 409、畸形 Host 400、48/48、65/65、20/20，gate PASS；独立 reviewer `019f511d-c5ed-7571-86e5-71372fd9d220` 复核预览键盘删除与最终 manifest，gate PASS。
- 独立 QA `019f4fd0-8ec7-7801-a493-e2d6797aea23` 用临时正式 package 复跑同类路径并确认最终 manifest hash，gate PASS。P5-02C1 技术切片关闭；P5-02C2/C3 仍未开始，P5 不关闭、不声明准确率。

## 2026-07-16 - P5 30图人工首标完成并按用户决定跳过独立复核

- 标注员肖朗完成 30/30：OK 10、NG 10、REVIEW 10；服务端状态 revision 13。
- 导出 `artifacts/p5-review-workbench-20260714-220827/pass1-annotations.coco.json`，SHA-256 `E09708A8B6AB989E01F68E5B854EB673B66C12CB51448D5F55AABC6D33CC5E43`。
- 导出含 30 images、71 annotations、9 categories；未发现 score/detector_version/source_bbox/predicted_decision 泄漏。
- `ground_truth_complete=false`、`accuracy_metrics_claimed=false`、stage=pass1；用户明确不执行独立人工复核。
- 因缺少独立复核和授权，该数据不得升级为 reviewed ground truth，不得用于正式准确率结论或训练/阈值调优；后续只允许进行类别分布、数据质量和模型分歧的探索性分析。

## 2026-07-18 - P5 单标注员参考的探索性分歧分析

- 从关机检查点恢复；当前 `review-state.json`、pass1 导出及两个暂停备份哈希一致，无遗留 Python 进程，上次失败的脚本/输出目录不存在。
- 新增 `scripts/p5_exploratory_consistency.py` 和 `tests/p5/test_p5_exploratory_consistency.py`；输入门强制两侧 `ground_truth_complete=false`、`accuracy_metrics_claimed=false`、逐图逐框 `is_ground_truth=false`，并绑定类别目录、图 ID/文件名/SHA-256/尺寸。
- 正式输出为 `artifacts/p5-exploratory-analysis-20260718-131039`；manifest SHA-256 `05697C73450DB607B30E811A6C3F89DD25B99C9CCCFFD3C721E1C800F952587F`。
- IoU 0.5、忽略类别后空间优先的确定性贪心匹配：70 个模型框与 71 个人工参考框形成 36 个同位置同类别、4 个同位置类别改变、30 个仅模型侧框、31 个仅人工参考侧框，共 101 条结果。
- 30 图决定描述：14 相同、6 不同；10 张人工 `REVIEW` 单列并从可比较集合排除。所有 REVIEW 备注非空。
- 6 个输出均有哈希绑定；30 image rows、10 REVIEW rows 和框账目复核通过；输出未使用正式效果指标或 FP/FN 等术语。
- P5 全部 Python 测试 55/55 通过，`git diff --check` exit 0。该结果只用于分歧排查；两侧都不是真值，不用于训练、阈值/NMS 调整或验收。
- 独立 reviewer `019f73a2-d454-7d91-82be-3ac4de258711`：PASS，无 P0-P2 finding；独立重算 101 条框结果一致。其 P3 测试加固建议已落实为预测侧非真值门、字段缺失、效果声明、pass1 阶段、IoU=0.5 和同 IoU 决胜顺序回归。
- 独立 QA `019f73a2-d5d2-74f2-9d7c-d9f4c39e53c4`：PASS；独立复算输入/输出哈希、30/10 图片账目、70/71 框账目与禁用表述扫描，并在返修前独立运行 53/53。测试加固后本机最终回归为 55/55。

## 2026-07-19 - P5 探索性分歧离线可视化包

- 新增 `scripts/p5_visual_disagreement_pack.py` 与 `tests/p5/test_p5_visual_disagreement_pack.py`，严格绑定既有探索性分析 manifest、模型输出、单标注员 pass1、类别目录和逐张源图 SHA-256/尺寸；全部源图在创建输出目录前校验；源图和分析输出路径必须解析在允许目录内。
- Windows 环境的 Pillow FreeType 扩展 `_imagingft` 被拒绝加载，脚本已安全降级到默认字体；JPEG 原子写同步改为 Windows 可用的 `rb+`。图片内动态文本在位图字体降级时转义为 ASCII，中文说明、筛选原因和图例完整保留在离线 HTML/README；整包在同父目录 staging 完成并校验后才原子重命名，失败不留下正式目录或 staging。
- 正式输出为 `artifacts/p5-exploratory-visual-pack-20260719-113656`，manifest SHA-256 `E0D64E78B073BD2403607F54E8E0537CD93B820C7D9E78E337D9E20A3E3A528F`。共 19 张唯一案例：6 张决定不同、10 张人工 REVIEW、3 张含 4 个同位置类别改变框；23 个输出文件全部有哈希绑定。
- 结构与身份复核：19/19 JPEG 可打开且尺寸符合三栏加标题区；CSV 19 行、HTML 19 个本地图片链接、无外网 URL/脚本标签；源图哈希全部不变；原图栏相对源 JPEG 的最大平均像素差 0.789/255（仅输出 JPEG 重编码）。代表案例 40/44/6 的蓝/橙绿/绿红框色像素可机械检出。
- 回归：可视化 targeted 12/12；P5 全量 67/67；`py_compile`、`node --check`、`git diff --check` 通过。当前 Codex 界面不支持直接加载本地图片，故不虚称本轮编排代理完成人工肉眼看图；首轮独立 reviewer 对候选目录 `artifacts/p5-exploratory-visual-pack-20260719-112152` 给出 FAIL：发现路径逃逸、包级原子性、中文字体降级和测试覆盖问题；上述问题已返修，最终 reviewer/QA 复核另记。
- 本包不需要新增人工标注，只用于分歧定位。模型输出与单标注员参考都不是真值；没有独立人工复核、业务类别确认和 approved 授权前，仍禁止训练、阈值/NMS 调整、正式效果指标和验收结论。P5 保持进行中。
- 首轮 reviewer `019f7866-87b9-7d33-a78f-ed4a97d13167` 的 1 项 P1、3 项 P2 已全部返修并复核为 RESOLVED；正式目录 `113656` 最终 reviewer PASS，无开放 finding。
- 独立 QA `019f7866-8956-7af3-b466-c578bce34bc6` 最终 PASS：P0/P1/P2 为 0，唯一 P3 是本 Windows 会话无创建符号链接权限，故链接逃逸子分支未取得运行态证据；该项非阻断。
- 候选目录 `112152` 保留为首轮失败审查证据；`113632` 因 PowerShell 输出管道被提前关闭导致命令退出状态有歧义，不作为正式证据；所有正式引用统一指向 `113656`。

## 2026-07-19 P5-02C3 双人复核事实补充与真值晋级

- 用户更正此前“跳过独立复核”的理解：肖朗逐页完成 30 图标注，小狼逐页检查；旧工作台只保存一个名字。用户以项目负责人身份批准这 30 图作为真实数据。
- 原始 `artifacts/p5-review-workbench-20260714-220827/pass1-annotations.coco.json` 保持不变，SHA-256 仍为 `E09708A8B6AB989E01F68E5B854EB673B66C12CB51448D5F55AABC6D33CC5E43`，仍明确是 pass1/non-ground-truth 历史证据。
- 新增 `scripts/p5_promote_reviewed_truth.py` 与 8 项定向测试。脚本严格绑定 pass1、manifest、预测与类别目录哈希，拒绝同名标注/复核、输入漂移、预测 provenance 污染和待确认类别进入可比较真值；同父目录 staging 完整验证后原子晋级，失败清理。
- 正式输出 `artifacts/p5-reviewed-truth-20260719-124537`：30 reviewed、20 comparable、10 REVIEW excluded、35 个正式 GT 框；REVIEW 图中的 36 个参考框仅留在原 pass1。标注人肖朗，复核人小狼，授权 approved。
- 输出包括 reviewed GT、approved pilot manifest、仅同步授权的 evaluation predictions、attestation、promotion summary 和 evidence manifest。预测 annotations 与源预测逐项完全一致，非 pilot 记录继续 unverified。
- 自查：正式 GT 与 evaluation predictions 验证均为 valid/error 0；定向 8/8、P5 全量 75/75；输入 4/4、实现 2/2、输出 5/5 哈希和大小匹配；AST、JavaScript 语法与 `git diff --check` 通过。
- 当前不运行训练或阈值/NMS 调优；冻结 pilot 不得参与这些活动。独立 reviewer/QA 已关门；下一步精确绑定 P4 模型、engine 与 detector config，运行小规模 pilot 基线；10 张 REVIEW 排除且多类零支持必须进入报告限制。
- 门禁返修：旧候选 `123517` 的数据内容通过 reviewer，但 QA 发现晋级脚本允许 `reviewed_at` 相对源 mtime 偏差 ±500ms；改为 datetime 精确相等，增加 250ms 漂移拒绝测试，并重新生成不可变候选 `artifacts/p5-reviewed-truth-20260719-124537`。
- 最终独立门：reviewer `019f789f-575c-7ed0-ab72-809d63f11c1f` PASS、QA `019f789f-7a77-7be3-a447-fc32133c0534` PASS；无开放 P0-P3 finding。P5-02C3 技术切片关闭，P5 整体继续进行且不声明商业效果。

## P5-02C5 provisional fallback pilot baseline (2026-07-19)

- Status: implementation, local verification, independent review, and QA PASS; formal TensorRT remains blocked.
- Artifact: `artifacts/p5-fallback-baseline-20260719-161114/` (local evidence only; never commit).
- Classification: ONNX Runtime 1.20.1 CPU fallback, **not** formal P4 TensorRT evidence.
- Frozen inputs: approved 30-image pilot, reviewed GT, model SHA-256 `956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4`, confidence threshold 0.25, IoU 0.50. No training or threshold/NMS tuning occurred.
- Evaluation population: 20 comparable images; 10 REVIEW images excluded.
- Box-level micro: TP=13, FP=24, FN=22, precision=0.351351, recall=0.371429, F1=0.361111.
- Cigarette-level: 10 GT NG / 10 GT OK; missed-NG=3 (0.30), false-NG=3 (0.30).
- Regression: `python -m unittest discover -s tests/p5 -p "test_*.py"` -> 76/76 PASS.
- TensorRT remains blocked: all available engines fail TensorRT 8.6.1 deserialization; rebuilding from ONNX fails because the `Mod` node plugin is unavailable.
- This pilot result is diagnostic only and is not a product-acceptance or commercial-performance claim.

## P5-02C5 final independent review (2026-07-19)

- Initial reviewer: FAIL because `runtime_contract` was hash-bound but not semantically validated.
- Repair: strict schema/backend/provider/version/scope validation; model and predictions cross-hash checks; source runtime manifest hash check; detector-config identity check; explicit report classification; negative tests.
- Independent reviewer after repair: PASS.
- Independent QA: PASS, limited to provisional ONNX Runtime CPU fallback baseline.
- Regression after repair: 76/76 PASS; `git diff --check` PASS (line-ending warnings only).
- Evidence report SHA-256: `2bc9b7115ba67563308fcb70fe3c4435c8849b89346962b8148d6487917b24d3`.
- Evidence manifest SHA-256: `9a9fd67471413eebcd8cd57179a8048bf7e868f313035921459046b6735968d6`.
- Formal TensorRT baseline remains BLOCKED / NOT VERIFIED under KI-039.
