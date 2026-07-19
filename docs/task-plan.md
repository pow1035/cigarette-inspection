# 分阶段执行计划

<!-- CURRENT_PHASE:P5 -->

当前阶段：P5 本地数据与算法效果闭环进行中。P4 已通过 TensorRT 10 离线技术集成并关闭，但无人工 ground truth，尚未达到商业效果验收。因当前无法到现场，P5-P8 全部调整为本地可执行阶段；真实相机、真实 IO 和真实剔除移出当前排期并继续禁止。Qt 产品化沿用新版工程现有 UI 风格。一次只允许一个阶段处于“进行中”。

| 阶段 | 状态 | 目标 | 退出条件 |
| --- | --- | --- | --- |
| P0 项目控制系统 | 已完成 | 建立需求、架构、计划、验收、证据、评审和日志体系 | AC-00 系列已通过提交门 |
| P1 新版工程构建基线与硬化 | 已完成 | 固化 Windows 工具链，修复确定性阻断缺陷 | Release x64 已构建并启动；独立 reviewer/QA gate PASS |
| P2 运行数据契约与检测器边界 | 已完成 | 建立 Frame/Detection/Result/Reject 类型和可替换接口 | 两配置契约测试及独立 reviewer/QA gate PASS |
| P3 离线检测闭环 | 已完成 | 图片回放进入检测、判定、保存、统计和 UI 结果 | 固定样例端到端运行且结果可追踪；独立 reviewer/QA gate PASS |
| P4 TensorRT 正式集成与评估 | 已完成（技术集成） | 将现有 YOLO/TensorRT 实现接入统一接口 | 模型/参数可配置，效果审查与时延证据可复现；无人工真值时不声明准确率 |
| P5 本地数据与算法效果闭环 | 进行中 | 建立标注真值、量化基线并优化模型/阈值/NMS | 冻结测试集可复现，逐类及烟支级指标有证据，主要错误样本有闭环 |
| P6 本地实时流与模拟剔除 | 未开始 | 用文件/录制流模拟多相机、编号、节拍、队列和剔除时序 | 可配置节拍下无失控积压，结果与模拟剔除全链路可追踪 |
| P7 Qt 产品功能与 UI 重构 | 未开始 | 围绕深度学习主线精简旧功能并增强监控、复核和配置体验 | 核心工作流可用，废弃功能有清单和迁移依据，Computer Use/QA 通过 |
| P8 本地稳定性、部署与交付预验收 | 未开始 | 长时运行、性能优化、故障恢复、部署包和本地商业预验收 | 已确认的效果/时延/稳定性阈值通过，交付包可在本机复现 |

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

P5-01、P5-02A/B、P5-02C1 技术切片已通过门禁。2026-07-19 用户澄清：30 图当时由肖朗逐页标注、小狼逐页检查，旧工作台仅保存一个名字；用户现批准其作为真实数据。原 pass1 不变，另行生成 approved/reviewed pilot：30 图均复核，10 张 REVIEW 排除指标分母，20 张可比较、35 个正式真值框。P5-02C3 独立门禁已通过；P5 整体仍进行中：下一步绑定 P4 模型/engine/config 形成小规模 pilot 基线；类别业务批准、完整 train/validation/test 划分和代表性覆盖仍未完成。冻结 pilot 不得用于训练或阈值/NMS 调优。

## 审查与 QA 节奏

- 文档阶段：独立 reviewer + 同代理验证 + 文档一致性检查。
- 代码阶段：实现代理自查后，至少一个独立 reviewer；涉及运行行为时增加 QA 和可观测性检查。
- UI 阶段：Windows 本地应用需要 Computer/人工运行证据；Mac 上的代码检查不能替代。
- 现场硬件阶段当前冻结且不排期；未来恢复时必须另立阶段，并由用户或现场人员报告人工 QA。
- P3、P5、P7 后评估一次定向清理，P8 前强制清理审查。

### P5-02C4 provisional fallback baseline

- [x] Preserve formal TensorRT/fallback identity separation.
- [x] Add mutually exclusive `--engine` / `--runtime-contract` evaluation bindings.
- [x] Run frozen pilot fallback evaluation (20 comparable, 10 REVIEW excluded).
- [x] Generate local immutable evidence manifest and baseline summary.
- [x] Run P5 regression (76/76 PASS).
- [ ] Independent reviewer and QA verdicts.
- [ ] Formal TensorRT rerun after compatible runtime/executable/engine is restored.
