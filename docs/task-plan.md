# 分阶段执行计划

<!-- CURRENT_PHASE:P8 -->

当前阶段：P8 本地稳定性、部署与交付预验收进行中。P7 的本地源码与 SDK-free 参数/状态契约已完成，本机无法补 Windows/Qt runtime，故转为外部目标机阻断；P5/P6 外部阻断继续保留。真实相机、真实 IO 和真实剔除继续禁止。一次只允许一个阶段处于“进行中”。

| 阶段 | 状态 | 目标 | 退出条件 |
| --- | --- | --- | --- |
| P0 项目控制系统 | 已完成 | 建立需求、架构、计划、验收、证据、评审和日志体系 | AC-00 系列已通过提交门 |
| P1 新版工程构建基线与硬化 | 已完成 | 固化 Windows 工具链，修复确定性阻断缺陷 | Release x64 已构建并启动；独立 reviewer/QA gate PASS |
| P2 运行数据契约与检测器边界 | 已完成 | 建立 Frame/Detection/Result/Reject 类型和可替换接口 | 两配置契约测试及独立 reviewer/QA gate PASS |
| P3 离线检测闭环 | 已完成 | 图片回放进入检测、判定、保存、统计和 UI 结果 | 固定样例端到端运行且结果可追踪；独立 reviewer/QA gate PASS |
| P4 TensorRT 正式集成与评估 | 已完成（技术集成） | 将现有 YOLO/TensorRT 实现接入统一接口 | 模型/参数可配置，效果审查与时延证据可复现；无人工真值时不声明准确率 |
| P5 本地数据与算法效果闭环 | 阻断（外部条件；未关闭） | 建立标注真值、量化基线并优化模型/阈值/NMS | 冻结测试集可复现，逐类及烟支级指标有证据，主要错误样本有闭环 |
| P6 本地实时流与模拟剔除 | 外部目标机阻断（本地切片完成） | 用文件/录制流模拟多相机、编号、节拍、队列和剔除时序 | 可配置节拍下无失控积压，结果与模拟剔除全链路可追踪 |
| P7 Qt 产品功能与 UI 重构 | 外部目标机阻断（本地源码完成） | 围绕深度学习主线精简旧功能并增强监控、复核和配置体验 | 核心工作流可用，废弃功能有清单和迁移依据，Computer Use/QA 通过 |
| P8 本地稳定性、部署与交付预验收 | 进行中（本地工具） | 长时运行、性能优化、故障恢复、部署包和本地商业预验收 | 已确认的效果/时延/稳定性阈值通过，交付包可在本机复现 |

## P0 完成内容

1. 核对 Git、目录和已有项目说明。
2. 独立审计新版 Qt、老版链路、TensorRT 原型。
3. 创建 `AGENTS.md` 和 `docs/` 记录体系。
4. 运行文档验证和 skill 的 `light_gate.py`。
5. 生成评审包，交给独立 reviewer 检查。
6. 修复阻断发现，更新证据和状态。

## P1 完成切片

P0、P1 已通过提交门；以下为 P1 完成记录，本轮不开始 P2 实现：

1. 建立 Windows 构建清单：Visual Studio、Qt、Halcon、MVS、DAQNavi、CUDA、TensorRT、OpenCV、GPU 驱动和模型哈希。
2. 审查 `.sln/.vcxproj/config.ini` 的配置矩阵和绝对路径。
3. 修复已确认的相机映射、数组释放、回调帧信息生命周期和陈旧 `testWrite` 引用。
4. 提供无真实剔除输出的 Debug/Release 构建方式。
5. 在 Windows x64 运行构建；若当前环境无法运行，阶段保持未完成并记录明确的人机 QA 交接。

| P1 子任务 | 状态 | 证据 |
| --- | --- | --- |
| P1-01 工具链和依赖事实清单 | 已完成（Release） | windows-build-baseline.md；`artifacts/p1-windows-20260711-115033/manifest.json`；Release 实际 MSBuild、启动模块版本与哈希 |
| P1-02 首批源码和工程硬化 | 已实现，QA 返修复核通过 | 当前 Git diff；validate_p1_static.sh；P1 reviewer/QA 发现与复核 |
| P1-03 静态验证与文档同步 | 已完成（静态） | `validate_p1_static.sh`、文档门、XML、shell 语法、diff check、skill 严格门均通过；独立 QA 返修复核通过 |
| P1-04 Windows Debug/Release 环境检查 | Release 通过；Debug 阻断 | `artifacts/p1-windows-20260711-115033`：Release 全部依赖通过；Debug 仅缺 25.05 Progress |
| P1-05 Windows MSBuild 和启动 | Release 通过 | Release MSBuild exit 0；`CigVision.exe/process.dll` 及 SHA-256；`startup-*.json` 和 `startup-window.png` 主界面证据；未点击 Run/IO/剔除 |

## P2 完成切片

| P2 子任务 | 状态 | 证据 |
| --- | --- | --- |
| P2-01 四类核心数据契约 | 已完成 | `core/InspectionContracts.h`；FramePacket 自持字节；Detection/InspectionResult/RejectCommand 校验 |
| P2-02 可替换运行接口 | 已完成 | `core/InspectionInterfaces.h`；IFrameSource/IDetector/IInspectionResultSink/IRejectOutput/IClock |
| P2-03 线程安全有界队列 | 已完成 | `core/BoundedQueue.h`；容量、溢出结果、丢弃计数、关闭和等待语义 |
| P2-04 无硬件契约测试 | Debug/Release 通过 | `artifacts/p2-contracts-20260711-122211`；两配置 MSBuild/test exit 0，7/7 测试通过 |
| P2-05 主程序回归构建 | Release 通过 | `artifacts/p2-main-regression-20260711-120926`；环境检查和 Rebuild exit 0 |

P2 不替换现有 `picStruct`、相机回调或灰度队列，不增加消费者。离线消费链属于 P3；P6 只新增文件/录制流消费者，真实相机原始帧适配冻结；真实剔除仍禁止。

## P3 验收候选切片

| P3 子任务 | 状态 | 证据 |
| --- | --- | --- |
| P3-01 离线编排与有界队列 | 已实现并自查 | `core/OfflineInspection.h`；独立生产/消费线程；显式停止、溢出和统计不变量 |
| P3-02 Qt 图片源与原子保存 | 已实现并自查 | `adapters/qt/QtOfflineInspection.*`；SHA-256 输入校验；`QSaveFile` JSON/PNG |
| P3-03 UI 与批处理入口 | 已实现并启动 | `--offline` 跳过相机/IO 初始化；`--offline-batch-manifest` 固定样本命令；界面 signal/slot 更新 |
| P3-04 故障与重复运行测试 | Debug/Release 通过 | `artifacts/p3-offline-20260711-132348`；正常、空输入、损坏/检测错误、保存失败、队列满、启动即停止/重跑、协作者异常，共 7/7 |
| P3-05 固定样本闭环 | 通过（链路测试） | 8 张输入哈希匹配；8 JSON + 8 PNG；processed=8、OK=4、NG=4、error/drop/save failure=0 |
| P3-06 Windows 主程序与无硬件启动 | 通过 | Release Rebuild exit 0；`startup/offline-window.png`；进程响应；未点击应用控件 |
| P3-07 独立评审与 QA | 通过 | reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f`、QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 最终 gate PASS |

P3 使用明确标注的 `DeterministicFixtureDetector` 验证编排，不声明缺陷识别准确率，也不替代 P4 TensorRT。`--offline` 和批处理入口不初始化相机或 DAQNavi，不生成 `RejectCommand`，真实剔除继续禁止。

## P4 验收候选切片

| P4 子任务 | 状态 | 证据 |
| --- | --- | --- |
| P4-01 模型与环境契约审计 | 已完成 | ONNX/PT 元数据；RTX 4060、CUDA 13.1、TRT 10.15.1.29、OpenCV 4.9；两名独立 explorer |
| P4-02 TensorRT 10 engine 构建与基准 | 已完成 | `artifacts/p4-audit-20260711-141545`；旧 8.6 engine 精确失败；FP16/FP32 新 engine exit 0；trtexec 基准 |
| P4-03 正式 `IDetector` 接入 | 已实现并自查 | `adapters/tensorrt/TensorRtDetector.*`；显式 JSON config；名称 I/O API；错误返回；离线批处理入口 |
| P4-04 固定样本与效果审查 | 运行通过，效果风险开放 | `artifacts/p4-tensorrt-20260711-150510`；116/116、246 框、232 PNG、3 组重复一致；无 ground truth |
| P4-05 P2/P3 回归 | 通过 | `artifacts/p2-contracts-20260711-145147`；串行最终 P3 `artifacts/p3-offline-20260711-151220` |
| P4-06 独立评审与 QA | 通过 | reviewer `019f4ff6-8554-7700-992b-ab2773108818` 六项 resolved、技术 gate PASS；QA `019f4ff6-9972-7b73-83a5-a288e6714ab5` 返修后 gate PASS |

P4 不接相机队列，不生成 `RejectCommand`，不触达 DAQNavi 或真实剔除。模型 ASCII 类别已从本体读取，但正式中文业务映射、逐类阈值、人工 ground truth 和生产准确率门槛仍未确认；这些缺口不会被运行成功替代。

## P5-P8 本地路线

1. P5 先定义九类缺陷中文业务语义、标注规则和数据授权清单，清理重复数据并建立互斥的训练/验证/冻结测试集；实现逐类 Precision/Recall/F1、混淆矩阵、烟支级漏检/误报和困难样本报告。基于证据优化逐类阈值、NMS、异常框过滤；数据充分且后处理无法解决时，允许进入可复现的模型训练/微调与回归。
2. P6 使用文件、录制序列和可控时钟构造本地实时流，模拟多相机、烟支编号、生产节拍、队列积压、丢帧、乱序、停止和重启；剔除仅生成模拟命令和日志，禁止 DAQNavi 写输出。
3. P7 先审计新版 Qt 现有页面与控件，沿用当前配色、布局密度和控件风格，再保留检测、结果复核、统计、配置和故障诊断主流程；传统算法保留接口/参考，不再扩展老版工程。移除或隐藏功能前必须有使用证据、依赖分析和回归检查，不凭观感直接删除。
4. P8 优化 CPU/GPU/内存/磁盘与检测时延，完成长时运行、故障注入、日志指标、D 盘部署结构、配置/模型哈希、升级回滚和本地商业预验收。准确率、吞吐、P95 时延和运行时长必须先形成数值门槛再裁定通过。

真实相机、DAQNavi 输入输出、卷烟机同步和真实剔除属于“现场条件恢复后的后续阶段”，当前不编号、不排期、不作为 P5-P8 退出条件，也不得用本地仿真替代现场验收声明。

## P5 当前切片

| 子任务 | 状态 | 预期证据 |
| --- | --- | --- |
| P5-01A 全仓资料与商业风险清单 | 已完成（只读盘点） | `docs/p5-source-inventory.md`；模型、数据、传统规则、UI 和许可风险来源 |
| P5-01B 九类目录与标注规范 | 通过（工具切片） | `config/p5-class-catalog.json`；`docs/p5-labeling-guide.md`；独立 reviewer/QA 最终 PASS |
| P5-01C 数据审计与 COCO 模板 | 通过（工具切片） | `artifacts/p5-data-20260711-170640`：116 文件、113 唯一哈希、3 重复组、尺寸/来源/授权状态和空真值模板 |
| P5-01D 预标注与真值校验 | 通过（工具切片） | 113 图/236 框 preannotated；预测 provenance、授权、双人复核声明和哈希门；空真值 expected exit 2 |
| P5-01E 效果评估 CLI | 通过（工具切片） | 20/20 正式单测；独立 QA 对抗测试 12/12；冻结 split、8 项绑定、守恒混淆矩阵和烟支级指标 |
| P5-02A 30 图确定性试标集 | 通过（技术切片） | 正式 `artifacts/p5-pilot-20260711-192148`；独立 QA `artifacts/p5-pilot-20260711-192926`；29/29；reviewer/QA PASS |
| P5-02B 可复核标注包 | 通过（技术切片） | 30 原图、30 带框预览、COCO 预标注、派生 pilot manifest 和 UTF-8 CSV；11 个含未确认类别样本强制 REVIEW |
| P5-02C1 本地首标工作台 | 通过（技术切片） | 48/48、超限 10/10、Browser QA；运行期 65 文件哈希、逐图署名、state/package provenance；独立 reviewer/QA 最终 PASS |
| P5-02C2 真实人工标注与逐页复核 | 已完成（人工事实） | 肖朗完成 30/30、小狼逐页检查；原工作台只保存肖朗，原始 71 框 pass1 保持不变 |
| P5-02C3 双人复核与授权真值 | 通过（复核晋级切片） | 肖朗标注、小狼逐页检查、项目负责人批准；正式证据 `artifacts/p5-reviewed-truth-20260719-124537`；reviewer/QA 最终 PASS，8/8 定向、75/75 P5 全量 |
| P5-02C4 探索性分歧可视化 | 通过（技术切片） | `artifacts/p5-exploratory-visual-pack-20260719-113656`：19 张并集案例、中文离线 HTML、CSV、README、23 个输出哈希；独立 reviewer/QA 最终 PASS；两侧均非真值 |
| P5-02C5 临时 ONNX Runtime CPU pilot 基线 | 通过（诊断切片） | `artifacts/p5-fallback-baseline-20260719-161114`：20 张 comparable、10 张 REVIEW 排除；独立 reviewer/QA 最终 PASS；正式 TensorRT 仍按 KI-039 阻断 |
| P5-02C6 工具跨平台回归与提交门硬化 | 通过（本地维护切片） | Pillow 11.3 位图降级、`requirements-p5.txt`、当前状态/重复 issue ID 门、P5 Linux local gates；独立 reviewer/QA 最终 PASS |
| P5-02C7 受控输入就绪与 fresh-clone 复核 | 通过（工具门）；输入仍阻断 | `scripts/p5_input_readiness.py`；readiness 24/24、P5 100/100；缺失/错配/attestation/路径/symlink/文件系统等价别名/output-overlap 定向门；fresh clone 严格检查报告两项外部 artifact 缺失；独立 reviewer/QA PASS |

P5-01、P5-02A/B、P5-02C1 技术切片已通过门禁。2026-07-19 用户澄清：30 图当时由肖朗逐页标注、小狼逐页检查，旧工作台仅保存一个名字；用户现批准其作为真实数据。原 pass1 不变，另行生成 approved/reviewed pilot：30 图均复核，10 张 REVIEW 排除指标分母，20 张可比较、35 个正式真值框。P5-02C3、P5-02C5 和 P5-02C7 的独立门禁均已通过；P5 整体明确保持“阻断、未关闭”：正式 TensorRT 基线、类别业务批准、完整 train/validation/test 划分、代表性覆盖和受控 artifact 恢复仍未完成。冻结 pilot 不得用于训练或阈值/NMS 调优。

当前 fresh clone 不携带 `.gitignore` 排除的 `artifacts/p5-reviewed-truth-20260719-124537` 与 `artifacts/p5-fallback-baseline-20260719-161114`。P5-02C7 的严格命令会以 exit 2 报告这两个外部输入缺失；只有在受控渠道恢复并通过 manifest、大小、SHA-256 和 attestation 绑定后，才可开始指定范围的 pilot 评估。

## P6 当前切片

| 子任务 | 状态 | 预期证据 |
| --- | --- | --- |
| P6-01A 核心回放与模拟剔除安全边界 | 通过（核心切片） | `core/RealtimeSimulation.h`；`tests/CigVision.Simulation/SimulationTests.cpp`：8/8；录制帧节拍、元数据保留、source/session 停止取消、模拟命令/回执、异常与重复编号负路径 |
| P6-01B Qt 图片/录制 manifest 接入 | 已实现（目标机包装已就绪；Qt/Windows runtime 待验证） | `QtOfflineInspection.*` 解析可选 station/camera/cigarette/delay 字段并保持旧 manifest 兼容；`--simulation-batch-manifest` + `--simulation-output` 使用 `SteadyReplayPacer`、可配 reject delay/queue capacity/target output，并原子写 `simulation-trace.json`；`scripts/p6_simulation_preflight.py` 做只读输入/产物核对，`scripts/p6_windows_simulation_evidence.py` + `scripts/run_windows_p6_simulation.ps1` 固化目标机 Release/运行/负路径/哈希证据流程；Windows/Qt runtime 尚未取得证据 |
| P6-02 多相机/乱序/丢帧容量曲线 | 已实现（SDK-free 核心切片已通过本机回归；Qt/Windows runtime 待验证） | `RealtimeLoadSimulation.h` 的虚拟时钟模型覆盖多相机、乱序/重复/跳号、队列溢出、P95 时延、停止/重启；本机 C++14/17、Clang、ASan/UBSan 和重复运行均通过，目标机/Qt runtime 仍待验证 |

### P6-01B Qt manifest 与 Simulation CLI

- [x] Preserve old P3/P4 manifests when station/camera/cigarette/delay fields are absent.
- [x] Validate optional metadata types and safe numeric bounds (local replay delay capped at 60 seconds) before starting a source.
- [x] Keep fixture, TensorRT and simulation batch modes mutually exclusive.
- [x] Validate simulation reject delay, queue capacity and target output in a SDK-free parser.
- [x] Add a standalone C++14 MSVC test project for the SDK-free simulation regression.
- [x] Instantiate only `SimulationRejectOutput` in the simulation entry path; no camera/DAQNavi/real IO object is created.
- [x] Write configuration, per-frame metadata, command, receipt, clock and error status to an atomic `simulation-trace.json`, with a count/status/clock consistency validator.
- [x] Keep simulated receipts at or after their scheduled time, bind failure error fields, and reject unsafe command/receipt mutations in the trace validator.
- [x] Enforce the 60-second local replay-delay safety cap in both the core replay source and the Qt image source.
- [x] Add a read-only, SDK-free manifest/trace preflight with path containment, SHA-256, metadata bounds, Simulation-only command/receipt and count checks; keep it separate from Qt runtime claims.
- [x] Add a target-machine evidence driver that preflights inputs, runs the Simulation batch, audits input/frame/summary/trace bindings, exercises parser rejection cases, and records raw logs plus hashes; permit non-Windows execution only as explicitly labelled test-only coverage.
- [x] Add a thin Windows PowerShell wrapper that performs the Release build, resolves runtime DLL search paths, invokes the evidence driver, and preserves a wrapper manifest without enabling real reject output.
- [ ] Run the Qt/Windows manifest and trace scenario on the target machine and retain raw output evidence.

### P6-02 多相机/乱序/丢帧容量曲线

- [x] Model several camera streams feeding one bounded inspection worker with a virtual clock.
- [x] Record arrival order, frame-id forward gaps/backward order, per-camera cigarette gaps/order, duplicate frame IDs and invalid frames without silently discarding the reason.
- [x] Exercise `RejectNewest` and `DropOldest` queue policies and retain per-frame drop dispositions.
- [x] Report maximum queue/pipeline depth, queue-wait and end-to-end P95 latency, processed/dropped/cancelled conservation and per-camera totals.
- [x] Generate only `RejectMode::Simulation` commands for processed NG candidates; reject zero cigarette numbers and clock overflow before command creation.
- [x] Exercise a stop boundary with both drain and cancel behavior, then rerun the same simulator instance to prove restart state is fresh.
- [x] Add strict C++14/C++17 regression coverage, exact deterministic capacity matrix and a trace validator that rejects Real commands or unfinished traces.
- [x] Recompute per-camera accounting and queue/end-to-end max/P95 latency in the validator; compile the three SDK-free headers standalone.
- [ ] Connect the capacity model to a Qt/Windows runtime recording and retain raw target-machine output; this SDK-free slice does not close the product/runtime gate.

## P7 当前切片

| 子任务 | 状态 | 预期证据 |
| --- | --- | --- |
| P7-01A 产品运行状态与最近结果复核 | 已实现（SDK-free 核心通过；Qt/Windows runtime 待验证） | `core/ProductRuntimeState.h`、`tests/CigVision.ProductState/ProductStateTests.cpp` 8/8；离线 worker 元数据接入；现有“缺陷查询”按钮进入深色复核页 |
| P7-01B 统计、诊断与会话证据 | 已实现（Qt/Windows runtime 待验证） | `btn_count`/`btn_log` 使用同一状态快照；启动、结束、复核时原子写 `product-session.json` |
| P7-01C 配置身份与安全退出 | 已实现本地源码，runtime 待验证 | typed configured/applied profile、canonical SHA-256、model/parameter/frame 绑定；七阈值独立页面与品牌持久化；当前 UI 固定 local-only；安全退出；目标机 QA 待完成 |

### P7-01A 产品状态与复核

- [x] 建立线程安全、内存有界的 SDK-free 产品状态模型。
- [x] 显式记录 run/mode/detector/parameter/model identity，并拒绝本地模式启用真实 IO。
- [x] 聚合 OK/NG/error、逐相机、逐类别、耗时和最大队列深度。
- [x] 对非法、参数版本漂移、窗口内重复帧和停止后的结果执行无部分统计写入拒绝。
- [x] 保存有界最近结果与诊断事件，支持具名复核、修正/误报标记和重新启动清零。
- [x] 将离线 worker 的 frame/station/camera/cigarette/defect/error 元数据接入产品状态。
- [x] 复用现有 `btn_search`，新增沿用深色样式的最近 NG/错误复核页；不删除任何旧控件。
- [x] 建立 `docs/p7-ui-inventory.md` 控件消费者和保留/后续处理清单。
- [ ] 在 Windows/Qt 目标机运行并用 Computer Use 验证页面布局、表格选择、复核操作和重复启停。

### P7-01B/P7-01C 统计、诊断、证据与退出

- [x] 将 `btn_count` 接到运行身份、总计、逐相机和逐类别统计页。
- [x] 将 `btn_log` 接到有界结构化诊断页。
- [x] 在启动、结束和人工复核时原子写 `product-session.json`，包含配置身份、状态、统计、最近结果、复核和诊断；逐帧 UI 回调不做磁盘写。
- [x] 运行中冻结品牌展示；本地配置继续固定 `realIoEnabled=false`。
- [x] 当前 UI 入口固定 local-only，不初始化相机或 DAQNavi。
- [x] 退出按钮在离线 worker 存活时先请求停止，线程结束后再关闭；在线停止或最终 session 落盘失败则取消退出。
- [x] 离线运行时阻止进入参数、品牌和系统设置页，并阻止同时启动在线运行；run snapshot 保留品牌/检测器/参数/模型身份。
- [x] 修复后独立 reviewer 与 QA 均 PASS；首轮 8 项发现全部关闭，TSan、ASan/UBSan、重复压力和启动窗口 stop 探针通过。
- [x] 将实际检测器字段纳入独立 schema/canonical SHA-256；分别记录 configured/applied profile，未被 fixture 消费的 legacy 七阈值明确标记未应用。
- [x] TensorRT v2 配置绑定完整 engine SHA-256、tensor/shape/preprocess、9 类逐类阈值与禁用状态，并原子写 `parameter-profile.json`。
- [x] 修复品牌深度学习参数页错接、七阈值范围校验、显式保存、`para.ini` 持久化和品牌切换刷新。
- [ ] 在 Qt/Windows 运行并验证写入失败、页面切换、退出和 JSON 产物。

## P8 当前切片

- [x] 建立 local-only 严格 package preflight：重复键/NaN、路径、symlink、case/NFC、size/SHA、配置、品牌和输出隔离。
- [x] 修复 `[General]`、应用目录、默认品牌、QRC 图标与开发机 UI 路径等静态部署阻断。
- [x] 完成 SDK-free soak 证据编排和故障矩阵（9/9）。
- [x] 完成 fixture release package/verify/activate/rollback 及本地 A→B→A 契约（17/17）。
- [x] 增加 Windows P8 wrapper 和 D 盘/完整依赖清单输入契约；wrapper/collector 静态契约当前 2/2。
- [x] 增加严格 package manifest generator：完整文件集、双重稳定性复扫、遍历 fail-closed、祖先 link/reparse 与 case/NFC 防护（7/7）。
- [x] 跨机证据返修为 v4：wrapper 生成 64-lowercase-hex challenge，复制 provenance collector 后以 mandatory `-RepositoryRoot $repoRoot` 立即现场采集，不再接受历史 `WindowsInput`/`GpuInput`；RepositoryRoot 与 OutputDirectory 不得重叠，v2 reports 绑定 challenge、package manifest、capture/host/time/collector。
- [x] collector 不执行 PATH 中的 python/git/qmake/nvidia-smi，只读应用版本、依赖文件大小/SHA 和 CIM GPU；拒绝空文件/reparse。collector 与 wrapper 均使用 ownership marker 和重复 reparse 检查，失败不递归删除目录。
- [x] v4 verifier/import 同时要求外部 manifest SHA、HMAC 和 exactly 32-byte key；key 必须位于 Package、EvidenceRoot、DeploymentRoot 与待验 bundle 外。verifier 精确复验 preflight 顶层/claims/inputs/9 项成功检查、v2 host input、采集窗口和 7 个 provenance 受信源；receipt v4 绑定 `verifierSha256` 与 manifest HMAC。
- [x] 更新 `docs/windows-target-execution.md`，固化 manifest→外置 key→v4 wrapper 现场采集→带外 SHA/HMAC/key→拷回→verify/import 交接顺序，并提供 Windows PowerShell 5.1 `CreateNew` 生成 32-byte key 的命令。
- [x] 新增独立 PowerShell parser/PSScriptAnalyzer 静态门和 7 个 CLI 契约用例；本机 Colima/Linux arm64 使用 PowerShell 7.6.3、PSScriptAnalyzer 1.25.0 扫描 13 个 `.ps1`，显式启用 `PSUseCompatibleSyntax` 目标 Windows PowerShell 5.1，parser/analyzer finding 均为 0；CLI 契约 7/7 PASS，覆盖成功、损坏语法、PowerShell 7 三元语法被 5.1 门拒绝、缺根、缺/空 `scripts/` 目录和缺 analyzer，exit 2/3 均输出机器可读 JSON。
- [ ] 在目标 Windows 取得完整依赖清单、PowerShell 实跑和 D 盘 A→B→A 证据。
- [x] 早期 39 项本地工具范围已完成独立 reviewer/QA；该 PASS 不覆盖后续 evidence v2。
- [x] cleanup 初审 4 P1/3 P2、reviewer 追加 2 P1/2 P2（gate/external 绑定、release 精确 schema、import 根白名单、文档）及 QA 追加 1 P1（不得提示执行待验 provenance verifier）均已修复。
- [x] 历史 70 项范围最终独立 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c` 与 QA `019f99d7-8a02-7da1-8641-2d533409d06d` 均 PASS，P0/P1/P2=0/0/0；该结论不覆盖后续主机报告 6 项增量。
- [x] 当前 P8 测试集总数保持 76：preflight 17、soak 9、release 17、wrapper/collector 2、package manifest 7、evidence verify/import 24；PowerShell 仍为 13 个脚本和 7 个 CLI 契约。
- [ ] 对 challenge/HMAC/v2/v4 返修完成独立 reviewer/QA；重点检查新鲜性、package/time 绑定、ownership/reparse 竞态、非递归失败、preflight 精确 9 项、key 隔离和 HMAC false-green。
- [x] 第二轮 documentation maintenance agent 已在 cleanup 修复切片后同步计数、状态和 Windows 交接手册。
- [x] 第三轮/最终 documentation maintenance agent 已同步最终评审状态；`.ruff_cache` 已清除。
- [x] 历史主代理 `./scripts/run_all_local_gates.sh --full` PASS：P5 100、P6 17、P8 70、C++17/C++14、repeat 20、ASan/UBSan、文档/P1/diff。
- [x] 第四轮 documentation maintenance agent 已在 PowerShell 静态门切片后同步独立计数、5.1 兼容目标、运行环境、证据边界和评审范围；工作流已接线但未声称 GitHub Actions 在线运行。
- [x] PowerShell 增量独立 reviewer `019f9a17-7e58-7350-9ec3-7373f3151f4f` 在两项 P3 和一项文档计数 P2 修复后最终 PASS，P0/P1/P2/P3=0/0/0/0；确认 12 脚本零 finding、CLI 7/7、exit 2/3 JSON、5.1 语法拒绝、文档防回退及文档门/`light_gate.py`/diff 全通过。
- [x] 同一 PowerShell 增量 reviewer 已完成最终复核，无开放 P0-P3 finding。
- [x] PowerShell 增量独立 QA `019f9a17-98ad-7f23-9c36-6d62a7fa49ec` 最终 PASS，P0/P1/P2=0/0/0；7/7 CLI、exit 2/3 JSON 与仓库前后哈希一致性均通过。
- [x] 最终 documentation maintenance 已统一 PowerShell 7/7 计数、reviewer/QA 状态和外部阻断边界；文档门 PASS、`light_gate.py` 无 warning、`git diff --check` PASS。
- [x] 第五轮 documentation maintenance 已同步当时的 P8 v1/v3 增量；该记录已被后续安全返修取代，仅保留时间线。
- [x] 第六轮/最终 documentation maintenance 已同步 v2 host report、v4 wrapper/receipt、现场 challenge 采集、ownership/reparse 非递归失败边界、外置 32-byte key 与 SHA+HMAC 三项认证，并更新防回退文档门。
- [ ] 运行返修后的 `./scripts/run_all_local_gates.sh --full`。最近一次 full gate 发生在本轮返修前，不得作为当前提交门。
- [ ] 目标机与外部阻断关闭前保持 P8 进行中：Windows/PowerShell/Qt/GPU/D 盘、真实数据/正式 TensorRT、许可和硬件均不得由本地门替代。

PowerShell 静态门输出固定 `syntaxCompatibilityTargets=["5.1"]`、`scriptAnalyzerMinimumVersion="1.25.0"` 和 `windowsRuntimeClaimed=false`。P8 测试集仍为 76 项；PowerShell 门仍为 13 个脚本与 7 个 CLI 契约。此前 reviewer/QA/full gate 结论均不能自动覆盖当前 challenge/HMAC/v2/v4 返修，也不扩大 Windows/Qt/GPU/D 盘或硬件 runtime 声明。

## 审查与 QA 节奏

- 文档阶段：独立 reviewer + 同代理验证 + 文档一致性检查。
- 代码阶段：实现代理自查后，至少一个独立 reviewer；涉及运行行为时增加 QA 和可观测性检查。
- UI 阶段：Windows 本地应用需要 Computer/人工运行证据；Mac 上的代码检查不能替代。
- documentation maintenance agent 固定在每个实现切片后、独立 review/QA 结果返回后、最终门禁前运行，核对记录系统后再允许进入下一门。
- 现场硬件阶段当前冻结且不排期；未来恢复时必须另立阶段，并由用户或现场人员报告人工 QA。
- P3、P5、P7 后评估一次定向清理。P8 v2/v4 返修已处理递归清理、历史重放、协调伪造和 PATH 执行风险；最终 reviewer/QA 仍须复核 ownership/reparse 竞态、key 隔离和 HMAC 信任边界。collector 保持单文件便于目标机部署，不因体量机械拆分。

### P5-02C5 provisional fallback baseline

- [x] Preserve formal TensorRT/fallback identity separation.
- [x] Add mutually exclusive `--engine` / `--runtime-contract` evaluation bindings.
- [x] Run frozen pilot fallback evaluation (20 comparable, 10 REVIEW excluded).
- [x] Generate local immutable evidence manifest and baseline summary.
- [x] Run P5 regression (76/76 PASS).
- [x] Independent reviewer and QA verdicts.
- [ ] Formal TensorRT rerun after compatible runtime/executable/engine is restored.

### P5-02C6 tooling portability and gate hardening

- [x] Make Pillow font degradation independent of the FreeType-backed `load_default()` path.
- [x] Pin the P5 visual-pack dependency and document the reproducible Python test command.
- [x] Add duplicate-issue/current-status/document-drift checks to `validate_project_docs.sh`.
- [x] Add Linux P5 local gates without implying Windows, GPU, TensorRT, or hardware verification.
- [x] Run local and independent reviewer/QA regression; retain only non-blocking notes for older Pillow compatibility and action/dependency pinning.

### P5-02C7 controlled P5 input readiness

- [x] Check model and class-catalog presence and SHA-256 before evaluation.
- [x] Check reviewed-truth/fallback manifest schemas, required outputs, sizes and SHA-256 bindings without modifying artifacts.
- [x] Reject malformed attestation catalog bindings, path escapes and symbolic-link evidence paths.
- [x] Report fresh-clone missing external inputs as an explicit non-ready result (exit 2), not as a successful or failed model evaluation.
- [x] Run independent reviewer and QA checks for the readiness tool and gate wiring.
- [x] Run readiness 24/24 and full P5 100/100 regression; preserve C6's historical 76/76 record.
- [x] Reject `--output` overlap with model, class catalog, either controlled artifact root or an artifact-root ancestor before any write, including case/Unicode filesystem-equivalent aliases.
- [x] State that manifest checks prove internal consistency only; external evidence-manifest SHA-256 remains a controlled recovery prerequisite.
- [ ] Restore the controlled reviewed-truth and fallback artifacts, then rerun the requested pilot scope.

### P6-01A replay and simulation reject safety core

- [x] Preserve frame station/camera/cigarette metadata while replaying a scripted sequence.
- [x] Support per-frame cadence through an injectable, cancellable pacer; stop must cancel a pending wait.
- [x] Generate only `RejectMode::Simulation` commands and record frame/number/scheduled-time/config bindings.
- [x] Reject result frame-id mismatches, invalid results, zero cigarette numbers, schedule overflow, unsafe output status and output exceptions without creating a real-IO path.
- [x] Run the SDK-free simulation regression and retain P5/P6 boundary evidence.
- [x] Connect the core source to the Qt image/recording manifest and persist a simulation trace from the product entry point (Qt/Windows runtime evidence remains a separate unchecked item in P6-01B).
