# 评审包

## 当前评审入口

当前实现入口是 **P8 本地稳定性与部署工具闭环**。P7 本地源码已完成，Windows/Qt runtime 保留为外部目标机阻断；P5/P6 外部阻断仍保留。P8 继续禁止真实相机、DAQNavi 和真实剔除，fixture/local tooling 不得冒充产品包。

## P8 主机报告与 v4 证据返修（2026-07-26）

- 评审范围：`scripts/collect_windows_p8_host_reports.ps1`、`scripts/p8_preflight.py`、`scripts/run_windows_p8_preacceptance.ps1`、`scripts/p8_windows_evidence_verify.py`、相应 P8 测试、文档和计数门。
- 主机报告契约：v4 wrapper 每次生成 64 位小写十六进制 challenge，将 collector 复制到 provenance 后以 mandatory `-RepositoryRoot $repoRoot` 立即子进程采集 Windows/GPU v2 报告；RepositoryRoot 与 OutputDirectory 不得重叠，不能从 provenance 脚本的 `$PSScriptRoot` 推导。报告绑定 challenge、package manifest、32 位小写 capture ID、UTC 时间、host SHA 和 collector 路径/schema/SHA；Windows 精确 12 项、GPU 精确 6 项检查必须全部 passed。
- 禁止声明：`productAcceptance`、`windowsRuntimeAccepted`、`gpuRuntimeAccepted`、`realIoTested`、`realRejectTested` 均为 false；preflight 另记录 `productAcceptanceChecked=false`、`semanticAcceptanceChecked=false`。
- 采集安全：collector 不执行 PATH 工具；`Get-LockedFileSnapshot` 检查完整 reparse 祖先链，以 `FileShare.Read` 锁定文件并在锁内计算 SHA。wrapper 对 provenance collector 持 `FileShare.Read` 句柄贯穿执行，执行前后复算 SHA 并复查 reparse 链。
- preflight/verifier：离线 verifier 精确重验 preflight 顶层、claims、inputs、9 项成功检查、v2 reports、采集窗口和 7 个 provenance 受信源；未知 claim、缺项、窗口外采集和协调改写均拒绝。
- v4 认证：EvidenceKeyPath 必须是 exactly 32 bytes，位于 Package、EvidenceRoot、DeploymentRoot 和 bundle 外。wrapper manifest 先在内存形成固定 UTF-8 bytes，以 `CreateNew + Flush(true)` 写入；SHA/HMAC 对同一 bytes 计算并复读确认未漂移。verify/import 必须同时提供带外 SHA/HMAC/key，receipt 为 v4。
- 最终门：HEAD `6886856` 的 `./scripts/run_all_local_gates.sh --full` PASS，覆盖 P5 100、P6 17、P8 76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个 `.ps1` parser/analyzer 0 finding、CLI 7/7。
- 最终独立门：reviewer `019f9a55-8131-7423-96a7-44d934c28255` 与 QA `019f9a55-99a2-7011-887f-119893e3ec4f` 均 PASS，P0/P1/P2/P3=0/0/0/0。
- 声明边界：该结果是最终本地工具门，不是目标 Windows runtime、Qt/GPU/SDK、真实数据、许可或硬件验收；P5 受控 artifact 仍缺失。
- 外部缺口：未在真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV 环境执行，没有真实数据、许可或硬件证据。

## P8 PowerShell 静态门增量（2026-07-26）

- 评审范围：`scripts/validate_powershell_scripts.ps1`、`tests/powershell/test_validate_powershell_scripts.ps1`、`.github/workflows/p5-local-gates.yml`，以及 `run_windows_p3_offline.ps1`/`run_windows_p4_tensorrt.ps1` 的两处 analyzer finding 修复。
- 门禁行为：递归扫描 `scripts/` 与 `tests/powershell/` 的 `.ps1`，先用 PowerShell parser 收集语法错误，再用最低 1.25.0 的 PSScriptAnalyzer 检查默认规则，并显式启用 `PSUseCompatibleSyntax`、目标 Windows PowerShell 5.1；模块缺失或低于最低版本且要求 analyzer 时 exit 2，输入根非法或未发现脚本时 exit 3，finding 时 exit 1。
- 机器可读契约：`syntaxCompatibilityTargets=["5.1"]`、`scriptAnalyzerMinimumVersion="1.25.0"`、`scope=parser-and-static-analysis-only`、`windowsHost=false`、`windowsRuntimeClaimed=false`。
- 规则边界：透明排除 `PSAvoidUsingWriteHost`、`PSAvoidUsingPositionalParameters`、`PSUseSingularNouns` 三项与项目证据包装脚本约定冲突的低信号风格规则。
- 修复：P3 构建参数从自动变量 `$args` 改为 `$buildArguments`；P4 重复样本索引从闭包 `$i` 改为显式 `$sampleIndex` 循环。
- 本机证据：Colima/Linux arm64，PowerShell 7.6.3，PSScriptAnalyzer 1.25.0；当前 13 个 `.ps1` parser finding 0、analyzer finding 0；有效仓库、损坏语法、PowerShell 7 三元语法被 5.1 门拒绝、缺失根目录、缺失 scripts 目录、空 scripts 目录和缺 analyzer 共 7/7 CLI 契约 PASS，exit 2/3 均输出机器可读 JSON。
- 声明边界：5.1 兼容静态检查不等于 Windows PowerShell 5.1 实跑；该证据不证明 Windows、Qt、GPU、D 盘、真实数据、许可或硬件。
- 计数边界：P8 当前 Python/静态契约回归是 76/76；PowerShell 13 脚本零 finding 和 7/7 CLI 契约单独记录。
- CI 边界：workflow 已增加 `pwsh` 静态门与契约测试步骤；本轮没有 GitHub Actions 在线运行证据。
- 评审状态：此前 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c` 与 QA `019f99d7-8a02-7da1-8641-2d533409d06d` 只覆盖 2026-07-25 的 70 项范围，不自动覆盖本增量。增量 reviewer `019f9a17-7e58-7350-9ec3-7373f3151f4f` 在两项 P3 和一项文档计数 P2 修复后最终 PASS，P0/P1/P2/P3=0/0/0/0；增量 QA `019f9a17-98ad-7f23-9c36-6d62a7fa49ec` 最终 PASS，P0/P1/P2=0/0/0。P8 整体保持进行中。
- 文档维护验证：`./scripts/validate_project_docs.sh` PASS；`light_gate.py` 无 warning；`git diff --check` PASS。

## P8 历史 70 项本地工具闭环（2026-07-25）

- 严格 preflight：该时点 12/12；外部 manifest SHA、文件全集/大小/hash、JSON duplicate/NaN、路径/symlink/reparse/case/NFC、config/品牌与 local-only 安全门。
- soak：该时点 9/9；每轮独立输出、重启/超时/崩溃、RSS/磁盘/输出量、Windows 跟踪子进程清理和 evidence SHA。
- release：该时点 17/17；仅生成 fixture artifact，验证后原子 activate/rollback，失败保持 current，共享目录不覆盖。
- package manifest generator：该时点 7/7；package 外不可覆盖输出、完整文件集、双重稳定性复扫和路径安全门。
- Windows wrapper：该时点静态契约 1/1；固定 D 盘并串联 preflight/soak/release/可选 rollback。
- Windows evidence v2：该时点 24/24；外部顶层 SHA、结束复扫、严格 run 守恒、真实 soak、provenance、receipt v2 和无覆盖原子 import。
- 该时点 P8 本地回归合计 70/70。cleanup 初审 4 P1/3 P2、reviewer 追加 2 P1/2 P2（gate/external 绑定、release 精确 schema、import 根白名单、文档）和 QA 追加 1 P1（wrapper 不得提示执行待验 provenance verifier）均已修复。
- 主代理定向复核为 32/32。
- 最终 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c` PASS，P0/P1/P2=0/0/0，P8 70/70、文档/diff PASS；最终 QA `019f99d7-8a02-7da1-8641-2d533409d06d` PASS，P0/P1/P2=0/0/0，wrapper 1/1 PASS；这是历史快照，不覆盖当前新增 6 项。
- 主代理 `./scripts/run_all_local_gates.sh --full` 最终 PASS：P5 100、P6 17、P8 70、C++17/C++14、repeat 20、ASan/UBSan、文档/P1/diff；`.ruff_cache` 已清除。
- 静态部署修复：有效 `[General]`、executable-relative 配置根、默认品牌、嵌入图标和无开发机 UI 路径。
- 目标机执行入口：`docs/windows-target-execution.md`，顺序固定为 package manifest→外部 SHA→PowerShell wrapper→跨机 verify→可选原子 import。
- 历史 70 项本地工具独立门 PASS；运行结果声明仍只能是 `passed-local-tooling`。Windows/PowerShell 产品包、D 盘、Qt/GPU 长稳、真实数据/许可/硬件和商业交付未验证，P8 整体不关闭。

## P7-01A 产品运行状态与最近结果复核（2026-07-25）

- 变更：新增 SDK-free 产品状态、SHA-256 与 typed parameter profile；离线 worker 增补产品展示元数据；运行/复核/统计/诊断页消费同一快照；`product-session.json` 原子保存 configured/applied profile；深度学习七阈值使用独立页面并持久化到品牌 `para.ini`。
- 安全边界：本地 run configuration 固定拒绝 `realIoEnabled=true`；当前 UI 构造固定 local-only，不初始化相机或 DAQNavi；TensorRT offline 必须绑定 64 hex model SHA-256；状态模型不依赖 Qt、TensorRT、相机或硬件输出。
- 验证：产品状态当前 8/8，SHA 标准向量与 profile golden 固定，GCC C++14/C++17 strict 已通过；独立 reviewer 已确认 detector 实际参数身份回传链和漂移门 resolved，此前产品状态门覆盖 TSan、ASan/UBSan、重复压力和启动窗口 stop 探针。
- 覆盖：运行配置、状态转换、OK/NG/error/逐相机/逐类别守恒、有界结果/诊断/重复窗口、非法输入与非法枚举无部分统计写入、station/camera 复合身份、具名复核修订、故障重启及 4 线程 400 帧。
- 未验证：Qt/Windows 主程序编译与运行、页面布局/交互、Computer Use、session/profile JSON 实际产物和 TensorRT/GPU runtime。
- 当前结论：首轮独立 reviewer/QA 为 FAIL；所列 8 项实现问题修复后，独立 reviewer 与 QA 均 PASS 且未发现新增 P0-P2。P7 本地源码切片已完成并转为外部目标机阻断，不声明目标机 UI PASS。

## P6 目标机证据编排器（2026-07-25）

- 范围：新增 `scripts/p6_windows_simulation_evidence.py`、`scripts/run_windows_p6_simulation.ps1`、伪目标运行时和 8 项 Python 编排测试；PowerShell 默认先调用 P1 Release 构建，随后由 Python 驱动执行 Simulation batch、产物绑定检查、trace preflight 和六类 CLI 负路径。
- 证据契约：新目录保存命令参数、stdout/stderr、退出码/超时、输入/逐帧/summary/trace 校验、源码/输入/输出 SHA-256 和 wrapper/driver manifest；`realIoEnabled=false`、`rejectEnabled=false`、`accuracyMetricsClaimed=false` 和 `tensorRtEvidenceClaimed=false` 固定写入报告。
- 本机验证：`tests/p6/test_p6_windows_simulation_evidence.py` 8/8，P6 全量 17/17；成功路径、输出篡改、Real IO 标记篡改、错误负路径退出、配置安全门、已有输出根、非法 manifest、非 Windows 默认拒绝和 PowerShell 接线均覆盖。
- 目标机命令：`powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p6_simulation.ps1 -Manifest .\tests\fixtures\p6-simulation-samples.json -EvidenceRoot .\artifacts\p6-windows-simulation-YYYYMMDD-HHMMSS`。
- 限制：该历史切片执行时 Mac 宿主机没有原生 PowerShell/Qt/MSVC，因此没有目标机 runtime 结果；2026-07-26 的 Colima/Linux PowerShell 静态兼容门不改变该结论。伪目标使用必须带隐藏 `--allow-non-windows-test` 且报告为 `passed-test-only`。该切片不关闭 AC-06、不声明 Windows、GPU、真实相机、DAQNavi 或真实剔除通过。
- 该 P6 历史切片当时仅完成同代理检查；当前 P8 70 项本地工具范围已由最终独立 reviewer/QA 复核并 PASS，但该结论不反向升级 P6 或目标机声明。目标 Windows 运行证据仍须在可用环境中补采。

## P6-01B manifest/trace 只读预检加固（2026-07-25）

- 范围：新增 `scripts/p6_simulation_preflight.py`、`tests/p6/test_p6_simulation_preflight.py`，并接入文档门和 Linux local gates；不修改 Qt 运行图、TensorRT、相机、DAQNavi 或真实输出。
- manifest 门：拒绝缺失/重复 JSON key、绝对或父目录 sample、路径逃逸、符号链接、重复文件、SHA-256 错配、空 station/camera、非法 cigarette/delay/expected；当前 4 图 fixture 只读预检 PASS。
- trace 门：要求 schema/mode/simulation/`realIoEnabled=false`、manifest 绑定、配置范围、完整 trace、逐帧 metadata/expected decision、Simulation command、计划/回执时间、失败错误字段和统计守恒一致；原子 report 不能覆盖 manifest/trace。
- 本机定向验证：P6 preflight 9/9，覆盖成功、缺失 exit 2、hash/path/metadata/duplicate-key/symlink、Real IO、命令烟支号、早回执、失败错误绑定和 output overlap；Python 编译与 `git diff --check` 通过。
- 限制：工具不解码图片、不执行 Qt/TensorRT，不认证 manifest 来源，也没有目标 Windows/Qt 产物；当前仍只算 SDK-free 预检证据。当前会话受上层约束未启动新代理，因此评审/QA 仍为同代理降级检查，不计独立 PASS。

## P6-01A 核心回放与模拟剔除安全边界（2026-07-25）

- 范围：新增 `core/RealtimeSimulation.h` 和 `tests/CigVision.Simulation/SimulationTests.cpp`；workflow 的 SDK-free C++ 门加入 simulation executable。
- 行为：逐帧 delay 由可注入 pacer 控制，保留 station/camera/cigarette metadata；停止会取消等待；空序列正常 EOF，非法帧/负 delay 启动失败。
- 安全：模拟 observer 只生成 `RejectMode::Simulation`；frame-id 错配、非法结果、零烟支编号、时间溢出、无效 command、output 异常或非 Simulated 回执均记录失败，不产生真实 IO 路径；模拟 output 自身也复核 command 契约。
- 本机验证：GCC/Clang 严格警告构建，P6-01A 核心 8/8、CLI 负路径 2/2（simulation executable 总计 10/10）PASS；其中 session stop 可取消 active source 的阻塞 replay wait。P5 24/24、100/100 与既有 C++ 7/7+7/7 仍需在综合门复跑。
- 该核心切片未覆盖（后续源码切片另记）：Qt 图片/录制 manifest runtime、持久化 trace 文件运行产物、多相机容量曲线、Windows/MSVC、GPU、现场硬件。

## P6-01B Qt manifest 与 Simulation CLI 源码切片（2026-07-25）

- 范围：`QtOfflineInspection.*`、`main.cpp`、`core/BatchCommandLine.h`、`core/RealtimeSimulation.h`、`tests/CigVision.Simulation/CigVision.Simulation.vcxproj`、工程头文件清单和 simulation regression。
- manifest：可选 `stationId`、`cameraId`、`cigaretteNumber`、`delayBeforeMicros`；字段缺失时保留旧 P3/P4 默认语义，显式值先做类型、非空和安全范围校验。
- CLI：`--simulation-batch-manifest` 与 `--simulation-output`，可选 reject delay、queue capacity、target output；fixture/TensorRT/simulation 三种 batch mode 互斥，模拟路径不创建真实 IO 对象。
- trace：`QSaveFile` 原子写 `simulation-trace.json`，包含 `realIoEnabled=false`、配置、每帧 station/camera/cigarette、观察/排程/完成时钟、Simulation command、回执、状态和错误。
- 本机验证（该源码切片的增量门）：strict C++ contracts 7/7、offline 7/7、simulation 10/10；其中 P6-01A 核心 8/8、CLI 负路径 2/2，随后 P6-02 将同一 executable 扩展并回归到 15/15。
- 未验证：Qt/Windows manifest 实际解析、trace 文件运行产物、MSVC/GPU、目标机容量曲线和现场硬件；P6-02 的 SDK-free 曲线不能替代这些证据。

## P6-02 多相机、异常与容量曲线 SDK-free 切片（2026-07-25）

- 范围：新增 `core/RealtimeLoadSimulation.h`，扩展 `SimulationTests.cpp` 和 `CigVision.Simulation.vcxproj`；不修改相机回调、DAQNavi、真实剔除或 TensorRT 运行路径。
- 模型：多个 station/camera 录制帧按虚拟 arrival time 进入单 worker/有界队列；支持 RejectNewest/DropOldest，记录逐帧 queue/pipeline、drop/cancel/invalid、开始/完成/P95 时延和逐相机统计。
- 异常：arrival/frame/cigarette 乱序、frame-id 重复、frame/cigarette 前向跳号、非法帧、processing overflow、零烟支编号、reject clock overflow均可观察；NG 只允许 Simulation command。
- 固定容量矩阵：20 帧、10 µs 到达间隔、50 µs 处理。RejectNewest capacity 1/4/32 -> processed 5/8/20，dropped 15/12/0，max queue 1/4/16，P95 wait 50/200/720 µs，P95 end-to-end 101/251/771 µs；DropOldest capacity 2 -> processed 6、dropped 14、P95 60/111 µs。
- 停止/重启：stop boundary 分别覆盖 drain 与 cancel；同一 simulator 实例随后重跑且计数归零。summary validator 对 Real command、unfinished disposition、早于计划时间的回执、命令烟支号篡改、逐相机统计和 P95 时延统计篡改返回失败；核心回放和 Qt source 入口均拒绝超过 60 秒的单帧延迟。
- 本机原始验证：C++14/C++17 `-Wall -Wextra -Wpedantic -Werror -pthread` simulation 15/15；优化构建连续 20/20；ASan/UBSan 15/15。
- 文档同步后的综合门：`validate_project_docs.sh` PASS（受控 P5 输入按预期 exit 2）、`validate_p1_static.sh` PASS、P5 Python 100/100、C++17 contracts/offline/simulation 7/7 + 7/7 + 15/15、C++14 simulation 15/15、Clang C++14/C++17 simulation 15/15、ASan/UBSan 15/15、simulation repeat 20/20、三个 SDK-free 头文件自包含编译、`py_compile`、simulation vcxproj XML、workflow YAML、`light_gate.py` 和 `git diff --check` 全部 PASS。
- 评审状态：当前会话受上层约束未启动新代理；同代理实现审查与 QA 式运行已完成，但不计独立 reviewer/QA。Qt/Windows 录制流、产品 trace、MSVC 和真实吞吐保持未验证。

## P5-02 试标集与复核包评审包（2026-07-11）

状态：P5-02A/B、P5-02C1、P5-02C3、P5-02C4、P5-02C5、P5-02C6 和 P5-02C7 的独立 reviewer/QA 均 gate PASS，技术/工具切片关闭。完整数据集、受控 artifact 恢复、正式 TensorRT 基线和商业准确率仍未完成，P5 整体不关闭。

### 评审范围

- `scripts/p5_dataset_tools.py` 新增 `select-pilot`，生成确定性 30 图选择、派生 split manifest、COCO 预标注和复核 CSV。
- `scripts/run_windows_p5_pilot.ps1` 复跑单测、生成 30 原图/30 预览、校验源图哈希和证据 manifest。
- `tests/p5/test_p5_dataset_tools.py` 新增确定性、覆盖、边界、canonical 和真值安全门测试。
- 不在范围：人工标签正确性、实际准确率、阈值/NMS 优化、模型训练、Qt UI、相机、DAQNavi、真实剔除。

### 正式验证

- 命令：`powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_pilot.ps1`
- 最终返修结果：exit 0；代码冻结证据 `artifacts/p5-pilot-20260711-192148`；29/29；30 canonical；30 原图；30 预览；71 个证据文件；源图/源码稳定；0 uncovered feature。
- 新证据门：拒绝既有 split、伪造 source_group、重复 canonical hash、悬空 alias、真值输入、catalog/COCO/P4 布尔标量和畸形 bbox；30 份 P4 JSON 与 COCO 的双向严格类型、尺寸/判定/缺陷数量/类别/bbox/置信度/defect detector version/frame parameterVersion 匹配；frame/preview 源及副本、工具/脚本/测试/目录/输入均哈希绑定。
- 覆盖：4 来源组、2 尺寸、12 OK/18 NG、class 0/1/2/3/4/5/7；11 个含未确认类别样本强制 REVIEW。
- 自查：`python -m py_compile` PASS；PowerShell parser PASS；`git diff --check` PASS（仅 CRLF 转换提示）。

### 独立检查结论

- reviewer：算法确定性、特征覆盖、重复/split、防泄漏、预测 provenance、错误路径、证据假绿与文档一致性。
- QA：从当前工作树独立复跑、复算输入/输出哈希、攻击无效 size/不完整 COCO、抽查原图/预览，并确认无真实硬件/剔除/准确率声明。

### 当前限制

- 30 图 pilot 已由责任人员标注、不同复核人逐页检查并获项目负责人批准；完整数据集的授权、类别业务确认和覆盖仍待补齐。
- `wuzi/jietou` 业务映射未确认；含 `wuzi` 的 11 图保持 REVIEW。
- 视觉抽查仍见大框、低置信框、重叠框和疑似误报，记录为 KI-035，不转换为误检率。

### 最终独立门禁

- reviewer `019f50b8-2d80-7e10-af4a-cdbd9b12cddc`：多轮可复现 finding 全部 resolved；正式 `192148` 的 29/29、6 个源码/输入哈希、71 个证据哈希、30 个 binding 和文档指针一致，最终 PASS。
- QA `019f50ef-db01-7cb0-b37a-f1af708b8cde`：新建 `artifacts/p5-pilot-20260711-192926`，29/29、fresh manifest passed，最终 PASS；代理额外报告对抗/哈希/视觉检查，但无单独逐项 transcript，故本包不声明额外检查数量。
- 这两个 PASS 只覆盖工具、选样与复核包证据完整性，不覆盖人工标签正确性、授权、准确率或硬件。

## P5-01 数据与效果基线评审包（2026-07-11）

状态：实现自查、独立 reviewer 返修复核和独立 QA 最终 gate 均 PASS；P5-01 工具切片关闭。P5 整体仍进行中，不声明实际准确率。

### 范围

- `config/p5-class-catalog.json`：九类机器目录，前七类来源支持但待业务批准，`wuzi/jietou` 禁止正式标注。
- `docs/p5-source-inventory.md`、`docs/p5-labeling-guide.md`：全仓资料用途、许可风险、COCO 扩展和标注规则。
- `scripts/p5_dataset_tools.py`：audit、preannotate、validate-annotations、evaluate。
- `scripts/run_windows_p5_data_baseline.ps1`：单测、真实数据审计、假真值负门、P4 预标注、源码/证据哈希。
- `tests/p5/test_p5_dataset_tools.py`：重复、边界、未确认类、假真值、框匹配、错类、FP/FN、烟支级误报漏报。

### 正式证据

```text
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p5_data_baseline.ps1 -P4Results .\artifacts\p4-tensorrt-20260711-150510\batch-output

exit 0；20/20 tests；failures=0；sourceFilesStable=true
116 source files；113 unique SHA-256；3 duplicate groups；116 legacy pairs
dimensions: 992x300=60, 1200x600=56
unreviewed ground truth: expected exit 2
preannotations: 113 images, 236 boxes, is_ground_truth=false
boundary normalization: 1 sub-millipixel float overrun, original bbox retained
unconfirmed class boxes: wuzi=99；jietou=0
manifest SHA-256: FD6EEFC9D4BD60C888F491B5108950FD7FFF5500BFD19CAD7AE781A7E64B733F
independent QA: artifacts/p5-qa-independent-20260711-170802
QA manifest SHA-256: 2CD0872196017385765C53EB2A5C7B32EEF740706D0AE4037AFF93AC3C0F7F87
```

### 评审重点

1. 类别目录、预测 provenance、授权、双人复核声明和哈希绑定是否能拒绝未经复核的数据直接进入准确率；不得把本地声明说成不可伪造签名。
2. 重复图 canonical/split 规则、尺寸/哈希/授权字段是否完整。
3. 框级逐类匹配、含背景混淆矩阵和烟支级 missed-NG/false-NG 是否数学正确。
4. 浮点边界只在 0.001 像素内显式裁剪，实质越界是否继续拒绝。
5. 脚本是否只读源图、无硬件、无真实剔除，并绑定当前源码哈希。

## P5 路线文档维护评审包（2026-07-11）

状态：该轮文档维护发生在 P5 实现开始前，当时没有实现 P5 代码、没有修改脚本、没有提交或推送；当前状态以上方 P5-02 评审包为准。

### 范围与结论

- 维护 `README.md`、`AGENTS.md`、需求、架构、计划、验收、问题、QA、可观测性、证据和评审记录。
- 本节是 P5 documentation-maintenance 的历史记录；当时唯一阶段为 P5。当前唯一阶段已切换为 P8；P0-P4 历史结论保留，P4 仅声明 TensorRT 技术集成通过，不声明商业准确率。
- P5-P8 统一为本地数据效果、本地实时流与模拟剔除、沿用现有风格的 Qt 产品化、本地稳定性/部署/交付预验收。
- 真实相机、MVS 采集、DAQNavi 输入输出、卷烟机同步和真实剔除冻结，不是 P5-P8 的待补验收项。
- 老版 Qt/Halcon 与仓库历史资料保留为业务、算法和时序参考；历史结论不能覆盖当前 `docs/` 状态或直接进入产品构建。

### 验证

```text
python C:\Users\hp\.codex\skills\codex-long-task-architecture\scripts\light_gate.py .
PASS: no obvious doc/evidence warnings

git diff --check
PASS: exit 0（仅 CRLF 转换提示）

历史取证时阶段标记检查：CURRENT_PHASE 仅 1 处，为 P5；当前 `docs/task-plan.md` 唯一标记为 P8
旧路线扫描：无“P5 接相机/P6 真实硬件”等当前计划命中
PowerShell 等价文档结构检查：必需文件、关键 ID、未跟踪文件空白、P5 受限目录全部 PASS
```

`scripts/validate_project_docs.sh` 可由 `D:\git\Git\bin\bash.exe` 启动，但其未跟踪文件 `git diff --no-index --check` 在当前 Windows CRLF 工作树中会把换行转换提示或 CR 字节判为 whitespace；已读取脚本并逐项用 PowerShell 等价复核。此项记录为 degraded validator compatibility，不冒充原命令已通过。

## P3 归档评审

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

## P2 归档评审

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

## P5-02C1 localhost 首标工作台评审包

### 范围与安全边界

- 新增 `tools/p5_review_workbench/`、`scripts/run_windows_p5_review_workbench.ps1` 和对应测试；只服务显式 30 图 package 与独立 workspace。
- 只允许 loopback，不接相机、DAQNavi、真实剔除或 Qt 产品主流程；`rejectEnabled=false` 未改变。
- 首轮导出只能是 `annotated`/`is_ground_truth=false`；reviewed 导出固定 409，人工首标、复核和授权不在本切片完成声明内。

### 验证与运行证据

- `python -m unittest discover -s tests/p5 -p "test_*.py" -v`：三轮返修 48/48；超限拒绝目标测试连续 10/10。
- 超限 JSON Windows 拒绝路径返修后目标测试连续 10/10；`node --check`、`py_compile`、PowerShell parser 和 `git diff --check` 通过。
- Browser：30 图加载；模型预览禁用编辑；1 张 OK 保存后 revision=1，重载保持；剩余 29 张时导出拒绝；console warning/error=0；1280x720 三栏无页面滚动溢出。
- 二轮返修正式技术证据：`artifacts/p5-review-workbench-20260711-203902`，包含测试日志、超限重复日志、health/state、package 清单、Browser QA、两张截图、QA 草稿、9 个源码快照、dirty diff 和 20 项 SHA-256 manifest；预览键盘 Delete 实测框数 4→4。

### 请求独立门禁

- reviewer 检查身份/路径/状态/原子性/revision/导出 provenance、超限体处理、loopback 边界和文档声明。
- QA 从当前工作树独立运行测试/API/浏览器或等价用户流程，确认没有把 Codex QA 草稿、`annotated` 或预测框提升为 ground truth。
- 最终状态：三轮针对性返修后，独立 reviewer/QA 均 PASS；AC-05-05/P5-02C1 技术切片提交门关闭。P5-02C2/C3 仍未开始，不建议把 P5 整体作为完成提交。

## 2026-07-18 P5 探索性分歧分析评审包

### 范围与限制

- 新增 `scripts/p5_exploratory_consistency.py` 和对应测试，只比较 P4 模型输出与 30 图单标注员 pass1 参考。
- 两侧都必须显式保持 `ground_truth_complete=false`、`accuracy_metrics_claimed=false` 和逐图逐框 `is_ground_truth=false`；单标注员侧必须为 `annotation_stage=pass1`。
- 输出只允许 same/different、空间匹配/类别改变和单侧框数量；人工 `REVIEW` 从图片决定比较中排除。
- 不训练、不调阈值/NMS、不形成正式效果、FP/FN 或验收声明；不连接相机、DAQNavi 或真实剔除。

### 评审证据

- 输出：`artifacts/p5-exploratory-analysis-20260718-131039`；manifest SHA-256 `05697C73450DB607B30E811A6C3F89DD25B99C9CCCFFD3C721E1C800F952587F`。
- 30 图：14 same、6 different、10 REVIEW excluded。
- 70 个模型框、71 个人工参考框：36 同位置同类别、4 同位置类别改变、30 仅模型侧、31 仅人工参考侧；101 行账目闭合。
- 最终 targeted 6/6、P5 全量 55/55；`py_compile`、`node --check`、`git diff --check` 通过。
- 独立 reviewer `019f73a2-d454-7d91-82be-3ac4de258711` 和独立 QA `019f73a2-d5d2-74f2-9d7c-d9f4c39e53c4` 均 PASS。reviewer 唯一 P3 加固建议已转为正式回归测试。

## 2026-07-19 P5 探索性分歧可视化评审包

### 范围与硬边界

- 审查 `scripts/p5_visual_disagreement_pack.py`、`tests/p5/test_p5_visual_disagreement_pack.py` 和正式目录 `artifacts/p5-exploratory-visual-pack-20260719-113656`。
- 只把既有描述性分歧分析转换成三栏案例图、中文离线 HTML、CSV、README、summary 和 manifest；不重新匹配出另一套统计，不接受未绑定输入。
- 模型侧与单标注员 pass1 都必须保持非真值；产物只用于分歧定位，不用于训练、调阈值/NMS、正式效果或验收。

### 要求 reviewer/QA 核对

- 分析 manifest 与三输入哈希绑定、分析重新计算一致性、源图 SHA-256/尺寸、全部输出哈希和 HTML 本地路径。
- 选图并集必须为 19：6 张决定不同、10 张人工 REVIEW、3 张类别改变图；类别改变框必须为 4。
- 颜色含义：绿=同位置同类别、橙=同位置类别改变、红=仅模型侧、蓝=仅人工参考侧；三栏为原图/模型输出/单标注员参考。
- Windows FreeType 不可用时应降级而非失败；源图身份错误必须在输出目录创建前拒绝；路径逃逸、链接逃逸和分析输出逃逸必须拒绝，中途失败不得留下正式目录或 staging。
- 扫描正式效果术语和错误真值/验收声明；确认无外部网络依赖。

### 本机验证证据

- 正式目录 manifest SHA-256：`E0D64E78B073BD2403607F54E8E0537CD93B820C7D9E78E337D9E20A3E3A528F`。
- targeted 12/12，P5 全量 67/67；`py_compile`、`node --check`、`git diff --check` 通过。
- 19 JPEG、19 CSV 行、19 HTML 本地图片链接、23 个 manifest 输出绑定、19 个源图绑定全部复核；正式效果术语边界扫描 0，HTML 外部 URL 0、script 标签 0。
- 原图栏最大平均 JPEG 像素差 0.789/255；代表案例 40、44、6 的预期框色可机械检出。当前编排界面不支持本地图片输入，因此这项机械检查不冒充人工肉眼业务复核。
### 独立门禁结果

- 首轮 reviewer `019f7866-87b9-7d33-a78f-ed4a97d13167` 对候选目录 `112152` 判定 FAIL：`../` 源图路径逃逸、包级非原子、默认字体中文动态文本风险及关键失败路径测试不足。
- 返修后同一 reviewer 对正式目录 `113656` 判定 PASS；首轮 finding 全部 RESOLVED，无开放 P0-P3 finding。
- 独立 QA `019f7866-8956-7af3-b466-c578bce34bc6` 判定 PASS：24 个文件清单、23 个输出哈希、19 个源图绑定、19 个案例和 HTML/CSV 链接全部一致；路径逃逸拒绝、中途失败清理和默认字体降级均通过。
- QA 唯一 P3 环境限制：当前 Windows 会话不能创建符号链接，因此链接逃逸分支未取得运行态证据；应在启用开发者模式、具备相应权限的环境或 CI 中补充，不阻断当前技术切片。
- 最终结论：P5-02C4 技术切片 PASS；P5 整体仍进行中，不能据此声明模型效果、准确率、真值完成或商业验收。

## 2026-07-19 P5-02C3 双人复核真值晋级评审包

### 人工事实与范围

- 项目负责人确认：肖朗逐页标注，小狼逐页检查；旧工作台只保存肖朗。项目负责人批准 30 图作为真实数据。
- 原 pass1 保持不变且不改写为真值；本切片只新增独立晋级脚本、测试和本地 ignored artifact，不进入 P6/P7，不训练、不调阈值/NMS。
- 10 张 `REVIEW` 是已复核 split 成员，但排除评估分母且正式 GT 零框；其 36 个参考框仅保留在原 pass1。

### 候选证据

- 脚本：`scripts/p5_promote_reviewed_truth.py`
- 测试：`tests/p5/test_p5_promote_reviewed_truth.py`
- 正式目录：`artifacts/p5-reviewed-truth-20260719-124537`
- 原 pass1 SHA-256：`E09708A8B6AB989E01F68E5B854EB673B66C12CB51448D5F55AABC6D33CC5E43`
- 计数：30 reviewed；20 comparable；10 REVIEW excluded；35 GT boxes；36 REVIEW reference boxes only in pass1。

### 最终验证

- 定向晋级测试：8/8 PASS。
- P5 全量：75/75 PASS。
- 外部真值门：`valid=true`、`error_count=0`、`require_reviewed=true`。
- 独立结构审计：输入 4/4、实现 2/2、输出 5/5 哈希/size 匹配；预测 annotations 完全不变；仅 pilot 30 条改 approved；非 pilot 保持 unverified。
- AST：2/2 PASS；`git diff --check` exit 0（仅现有 CRLF 提示）。

### 独立检查结论

- reviewer `019f789f-575c-7ed0-ab72-809d63f11c1f`：最终 PASS；新候选无开放 P0-P3 finding。
- QA `019f789f-7a77-7be3-a447-fc32133c0534`：最终 PASS；独立 250ms 漂移探针被正确拒绝，8/8 与 75/75 复跑通过。
- P5-02C3 复核晋级切片提交门已关闭；其后续临时 CPU pilot 基线已由下方 P5-02C5 完成，P5 整体仍因正式 TensorRT、完整数据和类别覆盖而进行中。

## P5-02C5 provisional fallback pilot baseline (2026-07-19)

- Status: implementation, local verification, independent review, and QA PASS; formal TensorRT remains blocked under KI-039.
- Artifact: `artifacts/p5-fallback-baseline-20260719-161114/` (local evidence only; never commit).
- Classification: ONNX Runtime 1.20.1 CPU fallback, **not** formal P4 TensorRT evidence.
- Frozen inputs: approved 30-image pilot, reviewed GT, model SHA-256 `956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4`, confidence threshold 0.25, IoU 0.50. No training or threshold/NMS tuning occurred.
- Evaluation population: 20 comparable images; 10 REVIEW images excluded.
- Box-level micro: TP=13, FP=24, FN=22, precision=0.351351, recall=0.371429, F1=0.361111.
- Cigarette-level: 10 GT NG / 10 GT OK; missed-NG=3 (0.30), false-NG=3 (0.30).
- Regression: `python -m unittest discover -s tests/p5 -p "test_*.py"` -> 76/76 PASS.
- TensorRT remains blocked: all available engines fail TensorRT 8.6.1 deserialization; rebuilding from ONNX fails because the `Mod` node plugin is unavailable.
- This pilot result is diagnostic only and is not a product-acceptance or commercial-performance claim.

## P5-02C6 tooling portability and gate hardening (2026-07-25)

- Scope: FreeType-independent Pillow fallback, exact P5 dependency declaration, document/issue-ID/stale-status gates, and a Linux-only local-gates workflow.
- Independent reviewer: PASS after rebuilding a clean snapshot from `git archive HEAD` plus the final diff; no P0/P1/P2 findings.
- Independent QA: PASS after a fresh Python 3.11 dependency install, 76/76 P5 regression, full font-failure injection, C++ 7/7+7/7, and workflow YAML/equivalent-structure commands. actionlint was not independently verified in this snapshot.
- Limits: CI does not prove a hosted Ubuntu run, Windows/MSVC, TensorRT/GPU, or hardware behavior; action/dependency SHA pinning is a later hardening option.

## P5-02C7 controlled input readiness (2026-07-25)

### Scope and changed files

- Add `scripts/p5_input_readiness.py` and `tests/p5/test_p5_input_readiness.py`.
- Wire the read-only input check into `scripts/validate_project_docs.sh` and `.github/workflows/p5-local-gates.yml`.
- Update README, handoff, task plan, acceptance/evidence/QA/known-issue/progress/review records for the fresh-clone external-input state.
- Do not restore, generate, modify or commit `artifacts/`; do not run a pilot evaluation, TensorRT, camera, DAQNavi or reject output.

### Acceptance mapping and behavior

- AC-05-06: fixed model/class-catalog identity; controlled directory and manifest schema; required output size/SHA-256; reviewed attestation identities/timestamps/approval/source hash and GT/approved-manifest/catalog cross-bindings.
- Exit 0 means the requested scope is internally consistent with the supplied manifest. Exit 2 is reserved for cleanly absent controlled reviewed/fallback roots. Exit 3 covers missing base files, non-directory artifact ancestors, incomplete artifact directories, malformed/mismatched bindings, parent references, symbolic-link paths/loops and unsafe output aliases.
- `manifest_trust` explicitly states that readiness does not authenticate a coordinated replacement of manifest plus outputs. External evidence-manifest SHA-256 remains a controlled recovery prerequisite.

### Validation evidence

```text
python3 -m unittest tests/p5/test_p5_input_readiness.py -v
24/24 PASS

python3 -m unittest discover -s tests/p5 -p 'test_*.py'
100/100 PASS

python3 scripts/p5_input_readiness.py --require-reviewed --require-fallback
exit 2; ready=false; missing_required=[reviewed_truth,fallback_baseline]; invalid count=0

./scripts/validate_project_docs.sh
PASS (with informational external-input blocker)

./scripts/validate_p1_static.sh
PASS

SDK-free C++ contracts/offline
7/7 + 7/7 PASS

py_compile; workflow YAML/equivalent readiness step; actionlint v1.7.12;
git diff --check; light_gate.py
PASS
```

### Review findings and disposition

- Independent reviewer `/root/p5_independent_review` found false-green or misclassification paths for missing `size_bytes`, broken symlinks, missing base inputs, incomplete artifact directories, unpinned default class catalog, incomplete attestation, parent/deep-ancestor symlinks, `symlink/..` lexical folding, non-directory artifact ancestors, case-insensitive/Unicode-equivalent aliases, artifact-root ancestor output, output symlink loops, tests omitted from the untracked whitespace gate, and stale review docs. Each implementation finding received a targeted regression; final code review reproduced 24/24 and 100/100.
- Independent QA `/root/p5_independent_qa` exercised success, absent-root, blocked-root, tampered output, attestation/catalog mismatch, path escape, valid/broken/deep/loop symlink, case/NFC-NFD alias, bidirectional output-overlap and workflow failure semantics; actionlint and equivalent workflow execution passed. Artifact snapshots were unchanged and no tracked generated/cache files appeared.
- Remaining input limitations are external, not hidden as a PASS: KI-040 remains open because both ignored artifact roots and the reviewed evidence-manifest digest are absent; KI-039 remains open for formal TensorRT.

### Git state and submission boundary

- Worktree contains the cumulative uncommitted P5-02C6/C7 implementation and documentation diff; no commit or push was requested or performed.
- `artifacts/`, models/engines, dependencies, credentials, licenses and device/customer information remain excluded from submission.
- C7 tool gate PASS does not close P5 and does not authorize P6/P7, training, threshold/NMS tuning, commercial accuracy or hardware claims.
