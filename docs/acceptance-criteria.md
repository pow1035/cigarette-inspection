# 验收标准

状态只使用：未开始、进行中、通过、未验证、阻断。

## P0 项目控制系统

| ID | 标准 | 状态 |
| --- | --- | --- |
| AC-00-01 | `AGENTS.md` 存在且能指向需求、架构、计划、问题和证据文档 | 通过 |
| AC-00-02 | 需求已拆分为功能、非功能、范围、假设和待确认决定 | 通过 |
| AC-00-03 | 架构声明产品主线、依赖方向、线程边界、数据契约和硬件安全门 | 通过 |
| AC-00-04 | 计划在 P0 执行和切换至 P1 后均只有一个当前阶段，后续阶段具有可观察退出条件 | 通过 |
| AC-00-05 | 已知问题包含代码风险、本地证据缺口、冻结的现场风险和验证边界 | 通过 |
| AC-00-06 | 文档验证、diff 检查、独立评审和证据矩阵均有记录 | 通过 |
| AC-00-07 | P0 未修改新版、老版、TensorRT 或硬件业务源码 | 通过 |

## P1 新版工程构建基线与硬化

| ID | 标准 | 状态 |
| --- | --- | --- |
| AC-01-01 | Windows x64 工具链和依赖版本有可执行检查清单 | 通过 |
| AC-01-02 | Debug/Release 至少一个配置在目标 Windows 机器构建成功 | 通过 |
| AC-01-03 | 相机映射、数组释放、回调元数据和编号快照生命周期问题有可复查证据 | 通过（静态） |
| AC-01-04 | P1 当前没有剔除输出实现，默认配置也不启用剔除 | 通过（静态） |
| AC-01-05 | Run/Stop 和析构对相机回调、队列、IO 线程有明确且可复查的生命周期 | 通过（静态） |
| AC-01-06 | Debug/Release 均声明 MVS 依赖，process DLL 与主程序有构建关系、父目录头文件路径和同目录输出 | 通过（静态） |
| AC-01-07 | 陈旧 testWrite 不进入新版工程编译 | 通过（静态） |

当前正式证据：`artifacts/p1-windows-20260711-115033`。同一次 VS2022 Developer PowerShell All 运行中 Release 环境检查和 MSBuild 均 exit 0，生成 `CigVision.exe`、`process.dll` 及 SHA-256；Debug 因 25.05 Progress 缺失保持 environment-check exit 1，故顶层结果为 `partial`、exit 1。Release 程序在 `rejectEnabled=false` 下仅启动并以 `startup-window.png` 记录主界面，未点击任何应用控件；HALCON 许可证、相机采集和实际 IO 仍未验证。

## P2-P5 核心与效果闭环

| ID | 标准 | 状态 |
| --- | --- | --- |
| AC-02-01 | 四类核心数据契约可脱离 SDK 构造并测试 | 通过 |
| AC-02-02 | 检测器、采集源、保存和剔除输出有明确接口边界 | 通过 |
| AC-03-01 | 离线图片可完成采集到结果保存的端到端流程 | 通过 |
| AC-03-02 | 无效图片、空结果、保存失败、队列满和停止中断有明确行为 | 通过 |
| AC-04-01 | TensorRT 模型路径、输入尺寸、阈值和类别映射可配置 | 通过（技术集成） |
| AC-04-02 | 固定测试清单记录逐图结果、汇总指标和推理时延 | 通过（不声明准确率） |
| AC-05-01 | 标注规则、数据来源、重复清理和训练/验证/冻结测试划分可审计 | 进行中（30 图 approved/reviewed pilot 已形成；完整 train/validation/test 划分与代表性覆盖未完成） |
| AC-05-02 | 逐类及烟支级指标可复现，错误样本可追踪到模型、配置和输入哈希 | 进行中（P5-02C5 已对 reviewed pilot 运行 ONNX Runtime CPU 诊断基线；正式 TensorRT 仍由 KI-039 阻断，完整数据与商业效果尚未验证） |
| AC-05-03 | 阈值、NMS、异常框过滤或模型改进均有前后对照，不以主观截图代替指标 | 未开始 |
| AC-05-04 | 试标样本按唯一哈希确定性选择，覆盖来源/尺寸/判定/已出现类别，并提供只读原图、预览、COCO/CSV 和源图哈希证据 | 通过（技术复核包；双人真值已由 P5-02C3 独立门确认） |
| AC-05-05 | 本地首标工作台严格绑定 30 图身份，支持安全保存/恢复和人工首轮导出，并阻止未完成、未确认类别及未经独立复核的真值导出 | 通过（技术切片；原 pass1 保持非真值，双人复核事实经独立 attestation 晋级，不绕过工作台真值门） |
| AC-05-06 | 受控 P5 评估开始前校验模型、类别目录、artifact manifest 内部 schema/输出大小/SHA-256 与 reviewed attestation；缺失或错配时不得开始指定范围 pilot，并要求外部 evidence-manifest 摘要核对 | 进行中（工具门通过；当前 fresh clone 缺少受控 reviewed-truth/fallback artifacts，manifest 自身认证需走受控恢复渠道） |

P2 正式证据：`artifacts/p2-contracts-20260711-122211` 的 Debug/Release 纯 C++ 契约工程均 MSBuild/test exit 0，7/7 测试通过；`artifacts/p2-main-regression-20260711-120926` 的 CigVision Release 回归 Rebuild exit 0。独立 reviewer/QA 返修复核均 PASS，AC-02 已关闭；不据此声明相机、算法、离线闭环或剔除通过。

P3 正式证据：`artifacts/p3-offline-20260711-132348` 顶层 exit 0。离线核心 Debug/Release 各 7/7；Release 主程序 Rebuild exit 0；固定 8 图生成输入清单、8 份完整逐帧 JSON、8 份 PNG 和汇总 JSON，统计为 OK=4、NG=4、error/dropped/saveFailures=0；非法 manifest exit 2。Computer Use 在 `--offline` 下完成单图选择、保存、预览和统计刷新。独立 reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 与 QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 最终 PASS，AC-03 关闭。该证据只证明确定性链路测试检测器，不证明 TensorRT、传统 Halcon 算法准确率、相机、DAQNavi 或剔除。

P4 正式证据：`artifacts/p4-tensorrt-20260711-150510` 顶层 exit 0，Release Rebuild、116 图 TensorRT 批处理、无效 engine/尺寸和残缺/冲突 CLI 拒绝均通过；逐图生成 116 JSON、116 原图 PNG、116 带框 PNG，processed=116、error/drop/save failure=0，3 组重复输入结果一致，10 个 P4 源码/工程/脚本哈希已绑定。独立 reviewer 六项 finding 全部 resolved、技术 gate PASS；返修后独立 QA 证据 `artifacts/p4-qa-independent-postfix-20260711` gate PASS。该样本没有人工 ground truth，只支持运行完整性、输出分布、视觉抽查和 detector latency 声明，不支持准确率、误检率或漏检率声明。

## P6-P8 本地产品化与交付预验收

| ID | 标准 | 状态 |
| --- | --- | --- |
| AC-06-01 | 文件/录制流可按可配置节拍模拟多相机、编号、队列和异常 | 进行中（P6 simulation 15/15；P6-02 已用虚拟时钟覆盖多相机、乱序/重复/跳号、队列溢出和停止重启；`p6_simulation_preflight.py` 与 `p6_windows_simulation_evidence.py` 另核对 manifest/产物路径、哈希和元数据；目标 Qt/Windows runtime 尚未执行） |
| AC-06-02 | 模拟剔除可由 frame_id 追踪到烟支编号、判定、时钟和配置，且无真实 IO | 进行中（SDK-free trace validator、只读 preflight 和目标机证据驱动只接受 Simulation command，核对命令烟支号、计划/回执时间、错误绑定、summary 账目及 CLI 负路径；P6-01B 已原子写 `simulation-trace.json`，但目标机产品运行产物尚未取得） |
| AC-06-03 | 固定负载矩阵可复现队列深度、丢弃数、P95 排队/端到端时延及容量取舍 | 进行中（20 帧、10 µs 到达间隔、50 µs 处理的确定性矩阵已通过；这些数值仅验证模型，不是生产节拍门槛） |
| AC-07-01 | Qt 主流程围绕深度学习检测、复核、统计、配置和诊断可用 | 进行中（产品状态 8/8；configured/applied typed profile、canonical SHA-256、帧级绑定、七阈值页面/品牌持久化、统计/复核/诊断和原子 session JSON 源码已接入；当前 UI 固定 local-only；Windows UI/JSON runtime 未完成） |
| AC-07-02 | 沿用现有 UI 风格；删除/隐藏旧功能前有清单、依赖分析、回归和 UI 运行证据 | 进行中（`docs/p7-ui-inventory.md` 已建立；本切片沿用深色样式且未删除控件，Computer Use/人工 UI 证据仍未取得） |
| AC-08-01 | 连续运行时长、CPU/GPU/内存/磁盘、队列和时延满足已确认阈值 | 进行中（P8 v2 本地连续工具证据通过：连续专项 5/5、P8 精确 81/81、公开 wrapper contract、`--core`/`--full` 均 PASS；`artifacts/p8-continuous-local-20260727-final-v2` 两轮为 300.046591/300.020852 秒，291/292 samples，30.64/23.51 CPU 秒，RSS 增长 16 KiB/0，progress gap 0.085584/0.098617 秒，共 3,793,408 帧。self-verify、主代理独立 verify、reviewer 两次独立 verify 及正式 evidence QA/observability/cleanup 均 PASS；KI-048 已独立复核关闭。GPU、Windows 产品 runtime、真实数据和现场指标仍未验证，因此 AC-08-01 整体不标通过） |
| AC-08-02 | D 盘部署、配置/模型哈希、升级回滚、review、QA、清理和本地提交门通过 | 进行中（当前 P8 v2 本地连续切片的独立 reviewer PASS，P0/P1/P2/P3=0/0/0/0；正式 evidence QA/observability/cleanup PASS。唯一非阻断 P3 是旧无 manifest 失败目录按隔离留存策略保留，不引用、不交付。HEAD `6886856` 的 P8 76/76 与历史门仅作旧范围快照；真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV、D 盘产品运行/回滚、数据、许可和硬件仍未验证，且未授权提交） |
| AC-08-03 | 目标机证据可通过独立保存的摘要与认证码跨机验真，并以无覆盖事务导入本地证据库 | 进行中（本地工具子门 PASS：v4 verifier/import 24/24，HEAD `6886856` 最终 full gate 已覆盖；外部 manifest SHA/HMAC/key、精确 preflight/host/provenance 复验保持不变。尚无真实 Windows bundle，`productAcceptanceClaimed=false`） |

真实相机、DAQNavi 和真实剔除验收因无现场条件而冻结，不属于当前 P5-P8 通过声明。未来恢复时必须新增阶段和独立验收标准，不能复用本地仿真通过状态。

最新推送 `6d492528c7f9c0b0b2e3cc70e1c60a74cb62cede` 的 hosted `Local gates` run `30188084112`（job `89756233568`）SUCCESS；该在线门只覆盖本地/静态回归，不改变上述进行中、未验证和外部阻断状态。

## 宽泛词检查

本文件不把“全部缺陷”“永不丢帧”“始终实时”等不可证明表述作为验收。准确率、吞吐、时延、运行时长和数据保留范围必须先给出数值与测试集，再允许标记通过。
