# 可观测性设计

## 当前状态

P3/P4 已具备逐帧 JSON、汇总计数、输入/输出哈希和 detector latency 证据；P6/P7 已形成 SDK-free 仿真与产品状态指标。P8 v2 增加同一进程组 RSS/累计 CPU/P50/P95、持续 progress、磁盘、输出、verified runtime 工作时长、进程寿命、受控 build/tool provenance、ProductRuntimeState 分桶和严格 inventory。当前连续专项 5/5、P8 精确 81/81、公开 wrapper C++17 contract run/self-verify/独立 verify，以及 Mac/Colima/Linux 原始 core/full 均 PASS；修复提交 CI reviewer 与 QA/observability 均 PASS，P0/P1/P2/P3=0/0/0/0。正式 `artifacts/p8-continuous-local-20260727-final-v2` 两轮 evidence、主代理/reviewer 独立 verify 和正式 evidence QA/observability/cleanup 均 PASS；hosted run `30221330296`（job `89844182732`）SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空；该结论仅覆盖 P8 v2 本地 SDK-free 连续工具证据。

| 轮次 | 工作时长 / 进程寿命 | samples / warm-up 后 | CPU / RSS 增长 | progress / 最大 gap | sessions / frames | OK / NG / Error |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 300.046591 / 300.358913 秒 | 291 / 232 | 30.64 秒 / 16 KiB | 3786 / 0.085584 秒 | 3786 / 1,938,432 | 969,216 / 969,216 / 0 |
| 2 | 300.020852 / 300.027510 秒 | 292 / 233 | 23.51 秒 / 0 | 3623 / 0.098617 秒 | 3623 / 1,854,976 | 927,488 / 927,488 / 0 |

两轮 Offline 与 ProductRuntimeState 分桶逐项相等，error/drop/subsystem/save failure 为 0；29 个 inventory 文件精确匹配，manifest SHA-256 为 `2bd420fab44acbed98315ffac5cf6a2b22567dcae42d98e522be35de3a7bcdd4`。旧无 manifest 失败目录仅按本地失败审计策略隔离保留，不属于可观测性或验收证据。

## 统一事件字段

后续结构化日志至少包含：时间、级别、组件、事件名、run_id、frame_id、工位、相机、烟支编号、线程、耗时、结果和错误码。模型事件还需记录模型哈希/版本、输入尺寸和阈值版本；剔除事件还需记录模拟/真实模式和目标输出。

## 必需事件

| 组件 | 事件 |
| --- | --- |
| 启停 | 配置加载、依赖检查、启动完成、停止请求、线程退出、异常终止 |
| 本地帧源 | 文件/录制流打开、帧生成、模拟相机映射、队列满、源结束、故障注入 |
| 模拟同步 | 可控时钟、烟支编号更新、乱序/重复/跳号、节拍变更 |
| 检测 | 模型加载、预处理、推理、后处理、空结果、异常、逐帧时延 |
| 结果 | OK/NG、缺陷类型、规则版本、统计更新 |
| 保存 | 任务入队、保存成功、失败、磁盘空间告警、丢弃 |
| 模拟剔除 | 命令生成、计划时间、模拟执行、跳过、失败、真实 IO 安全拒绝 |

## 指标

- 帧接收数、处理数、丢弃数和积压深度。
- 预处理、推理、后处理、端到端时延的 P50/P95/P99。
- OK/NG 数、按缺陷类别计数、模拟剔除数、跳过数和失败数。
- 保存成功/失败数、磁盘剩余空间。
- 同一连续进程组的 RSS、累计 CPU 单调性、CPU interval P50/P95、进程数量、采样完整性、已验真 runtime 工作时长 `durationSeconds` 和冻结于真实退出点的 `processLifetimeSeconds`。Linux CPU 由 `ps` 枚举进程组后读取各 PID `/proc/<pid>/stat` 汇总；pure-sleep 不能冒充工作量。
- GPU 显存与 GPU 利用率；P8 v2 本地 SDK-free profile 不采集 GPU，也不得从 CPU/RSS evidence 推断 GPU 稳定性。

P5 先冻结效果数据集和指标口径，P6/P8 再冻结本地时延、队列、吞吐、资源和运行时长门槛。现场阈值保持未知，不由本地门槛推断。

## 阶段证据

- P3：离线回放日志、逐帧追踪和队列行为。
- P4：模型加载、输出解析、视觉观察和推理时延；无 ground truth 时不汇总准确率。
- P5：数据版本、标注版本、模型/engine/配置哈希、逐类及烟支级指标、错误样本和优化前后对照。
- P6：文件/录制流、模拟相机与编号、队列深度、丢弃/乱序、端到端时延、模拟剔除命令及停止/恢复行为；P6-01B 的 `simulation-trace.json` 绑定配置、每帧 clock、command、receipt 和 error，模拟回执不早于计划时间；P6-02 的 `RealtimeSimulationSummary` 绑定逐帧 disposition、最大 queue/pipeline depth、P95 queue wait/end-to-end、逐相机守恒和可重算的序列账目；`scripts/p6_simulation_preflight.py` 在目标机运行前只读核对 manifest 的路径/哈希/元数据和 trace 的 Simulation-only/账目一致性；`scripts/p6_windows_simulation_evidence.py` 另保存命令、stdout/stderr、输入/逐帧/summary/trace 复核、六类 CLI rejection 和 SHA-256 evidence manifest。以上仍是 SDK-free/源码、输入预检和可测试编排证据，不替代 Qt/Windows runtime 或现场指标。
- P7：Qt 用户操作、页面状态、模型状态、统计/复核/配置流程、废弃功能清单和 Computer Use 截图/观察。
- P8 v2 本地连续运行：短 profile 的 locked 最低时长为 0.12 秒，public wrapper 与 C++17/C++14 direct project contract 门实际运行 1.0 秒；CPU ≥0.01 秒、progress ≥2/gap ≤0.5 秒、RSS ≤16 MiB。正式门为两轮各 ≥300 秒、60 秒 warm-up、samples/progress ≥240、gap ≤5 秒、sessions ≥240、frames ≥122880、CPU ≥10 秒、RSS ≤64 MiB、磁盘 ≥1 GiB。`local-soak-summary.json` 与 NDJSON 逐轮证明 Offline/ProductRuntimeState OK/NG/Error、sink/archive 和 processed 守恒。正式 project `run` 内部受控编译并快照 compiler/compile/8 source/executable；toolDependencies 另快照新/旧 Python tool source 与 POSIX `ps` identity/binary；外部 provenance 只允许 contract helper。inventory 不忽略 `.tmp`；Windows clean gate 需两次 fresh tree enumeration、no residual 与 enumeration success，并覆盖二次枚举失败。real IO/reject/product acceptance 均为 false。
- P8 Windows/部署：长时运行资源曲线、故障注入、恢复、部署、升级/回滚、本地预验收报告，以及 package manifest SHA、本次 wrapper challenge、现场 Windows/GPU v2 主机报告、显式 RepositoryRoot、collector SHA、wrapper 采集窗口、外部 wrapper manifest SHA/HMAC、结束复扫、严格 argv/profile/阈值/gate/run 守恒、Windows soak 语义和带 `verifierSha256`/manifest HMAC 的 import receipt v4。32-byte evidence key 必须在 Package、EvidenceRoot、DeploymentRoot 外单独保管，不进入 bundle。P8 v2 不修改或替换这条 Windows v1/v4 证据链；采集/交接步骤见 `docs/windows-target-execution.md`。

2026-07-27 当前增量证据：连续运行专项 5/5、P8 精确 81/81、公开 wrapper C++17 contract run + self-verify + 独立 verify，以及 Mac/Colima/Linux core/full 均 PASS；伪造 project provenance、helper drift、PATH shadow、dependency record/helper/ps snapshot/identity 篡改和 pure-sleep CPU 负路径均按预期失败。修复提交 CI reviewer 与 QA/observability 均 PASS；正式两轮 evidence 的既有 QA/observability/cleanup 仍 PASS。hosted run `30221330296`（job `89844182732`）SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空；本地 manifest/verify 没有带外 HMAC，只证明包内一致性和 snapshot/源码绑定，不能外推 Windows/GPU/产品验收。

历史快照：HEAD `6886856` 最终 full gate 已通过 P5 100、P6 17、P8 76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个脚本零 finding、CLI 7/7。collector 的锁定文件快照记录完整 reparse 祖先链安全与 `FileShare.Read` 锁内 SHA；wrapper 对 provenance collector 的读锁贯穿执行并前后复算 SHA/复查 reparse。preflight 与 v4 wrapper/receipt 仍固定不声明产品验收。提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 的在线 Actions `Local gates` 运行 `30169095184`（job `89706968923`）SUCCESS，全部步骤通过、check-run annotations 为空、原 Node 20 warning 已消失，checkout/setup-python v6 均固定完整提交 SHA；最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` 与 QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS。该运行只覆盖当时范围，不覆盖当前 P8 v2 连续运行增量。真实 Windows/Qt/GPU/TensorRT/SDK、D 盘资源曲线、P5 受控数据、许可、硬件和真实 bundle 尚未采集。

上一成功 hosted 基线为提交 `6d492528c7f9c0b0b2e3cc70e1c60a74cb62cede` 的 `Local gates` run `30188084112`（job `89756233568`）SUCCESS。首轮 `5f6a06a` 的 run `30219159920`（job `89838475434`）FAIL；修复提交 `cc7a3ab` 的 run `30221330296`（job `89844182732`）SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空；边界仍为 `windowsRuntimeClaimed=false`、`productAcceptanceClaimed=false`。

真实相机、MVS 采集、DAQNavi 输入输出和真实剔除日志属于冻结的未来阶段，不作为 P5-P8 待补事件。

若完整日志框架在早期过重，先使用带固定字段的 UTF-8 滚动文件日志；限制必须写入 `known-issues.md`，不能用零散 `qDebug()` 作为最终证据。

## P1 构建证据

P1 需保存 Windows 环境检查输出、MSBuild 完整日志、退出码、生成文件清单、实际依赖版本和启动错误。当前正式证据为 `artifacts/p1-windows-20260711-115033`：同一次 All 运行记录 Release 构建成功与 Debug 25.05 缺失，含完整终端、MSBuild 日志、manifest、源码快照哈希、产物哈希和启动元数据。`startup-window.png` 记录了主界面，进程模块证明 Qt/HALCON/MVS/DAQ runtime 已加载；不声明许可证算子、相机采集、实际 IO、性能或剔除已验证。

## P2 契约证据

`artifacts/p2-contracts-20260711-122211` 保存 Debug/Release 契约测试的 MSBuild 日志、测试输出、exe、六个源/脚本文件 SHA-256、确定性命令行、manifest 及证据文件哈希。`artifacts/p2-main-regression-20260711-120926` 保存主程序 Release 回归构建。P2 不产生相机、算法、保存、UI 或剔除运行事件；这些事件仍从 P3 起逐阶段验证。

## P3 离线闭环证据

`artifacts/p3-offline-20260711-132348` 保存完整终端、Debug/Release 7/7 测试、Release 主程序构建、严格 manifest 负路径、输入清单、逐帧完整 JSON/PNG、汇总 JSON、源/输出 SHA-256、无硬件启动截图和 Computer Use 单图 UI 观察。汇总字段覆盖 received、processed、OK、NG、error、sourceErrors、detectorErrors、observerErrors、saveFailures 和 dropped。当前逐帧 `elapsedMicros` 来自 detector 契约，但 fixture detector 没有性能意义；P4 才要求模型级时延分布，P8 才要求资源和长时运行指标。

## P4 TensorRT 证据

`artifacts/p4-audit-20260711-141545` 保存模型/engine 哈希、旧 TRT 8.6 engine 反序列化错误、TRT 10.15 FP16/FP32 engine 构建日志、层信息、随机输入 trtexec 基准、ONNX 参考图和精度模式对照。返修后正式证据 `artifacts/p4-tensorrt-20260711-150510` 保存 Release 构建、116 图输入哈希、显式 detector config、逐图 JSON/原图/带框图、类别/置信度/时延汇总、3 组重复图确定性、engine/尺寸/CLI 负路径、10 个 P4 输入源码哈希和完整 SHA-256 manifest。当前 `elapsedMicros` 是 detector latency，包含 mutex 等待、OpenCV 预处理、H2D、TensorRT、D2H 和后处理，不包含图片读取/解码或 JSON/PNG 保存，也不等同于纯 GPU latency。当前没有 ground truth，准确率指标明确不声明。

## P5-01 数据与评估工具证据

`artifacts/p5-data-20260711-170640` 保存 20 项单测、116 图 manifest、113 个 canonical、3 个重复组、尺寸/来源/授权统计、空 COCO 模板、预期失败的未复核真值门、113 图/236 框 preannotation、运行前后 7 个源码/规范哈希和完整 manifest。`artifacts/p5-qa-independent-20260711-170802` 保存独立复跑、12 项对抗场景、证据哈希和 116 张源图前后哈希。两者只证明工具和数据基线，不产生实际准确率；真实评估必须绑定 approved manifest、双人 reviewed 真值、attestation、模型、engine、detector config 和 prediction 哈希。

## P5-02 试标复核包证据

最终返修证据 `artifacts/p5-pilot-20260711-192148` 保存选样命令、29 项单测、选样特征/原因、派生 manifest、COCO 预标注、UTF-8 CSV、30 张原图、30 张带框预览、catalog/COCO/P4 全链严格标量类型、30 份 P4 frame JSON 与 COCO 的尺寸/判定/框/类别/置信度/defect detector version 及 frame parameterVersion 核验，以及源码、输入、frame、preview 源/副本 SHA-256。该证据支持“复核入口可重复生成”，不支持“预测正确”；大框、低置信框、重叠框和疑似误报作为困难样本观察进入 KI-035，必须由人工真值和后续指标裁定。

## P5-02C1 首标工作台证据

三轮返修冻结 `artifacts/p5-review-workbench-20260711-203902` 保存 48 项单测、超限拒绝 10 次重复、localhost health/state、完整 package 文件清单、Browser 交互记录、一次性 QA 草稿、两张界面截图、9 个源码快照、dirty diff 和 20 项 SHA-256 manifest。Browser 证据覆盖 30 图加载、模型预览四类编辑控件只读、预览态键盘 Delete 后框数 4→4、Alice 完成 1 张 OK 后 Bob 修改全局操作员仍保留逐图 Alice/revision 1、剩余 29 张时导出拒绝、控制台 0 warning/error 和三栏视口边界；自动测试还覆盖服务启动后的源图/preview 替换对 GET/save/export 受控 409。该 QA 草稿只验证软件行为，不是责任人员首标、人工 QA 或 ground truth；reviewed 导出仍固定拒绝。
