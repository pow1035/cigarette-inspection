# QA 检查表

## P0 文档阶段

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 必需文档存在且非空 | 通过 | `scripts/validate_project_docs.sh`；`evidence-matrix.md` |
| 当前阶段唯一且状态一致 | 通过 | `scripts/validate_project_docs.sh`；独立 reviewer 结论 |
| 已跟踪/未跟踪 P0 文件无空白错误，业务源码未改 | 通过 | 增强后的验证脚本 exit 0；限定业务目录的 `git status` 输出为空 |
| 独立 reviewer 检查需求、架构和证据完整性 | 通过 | `review-results.md`；最终复核四项均 resolved |
| 浏览器/Computer UI 检查 | 未请求/不声明 | P0 不改变用户界面或运行行为 |
| 人工 QA | 未请求/不声明 | P0 不需要用户操作 |

## P1 构建基线与硬化

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| P1 静态不变量 | 通过 | validate_p1_static.sh exit 0 |
| 工程 XML 和 diff | 通过 | xmllint、git diff --check exit 0 |
| Windows 环境检查脚本 | Release 通过；Debug 阻断 | `artifacts/p1-windows-20260711-115033`：Release 全部通过；Debug 仅缺 HALCON 25.05 Progress |
| Debug/Release MSBuild | Release 通过 | `msbuild-release.log` exit 0；Debug 未进入 MSBuild |
| 应用启动与重复启停 | 启动通过；重复启停未验证 | `startup-observation.json`、`startup-window-capture.json`、`startup-window.png`；进程响应正常，未点击 Run/Stop |
| 相机和 IO 硬件 QA | 未请求/不声明 | P1 不连接真实硬件 |
| 独立代码 reviewer | 通过 | `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0`：三项 finding 返修复核均 resolved，reviewer gate PASS |
| 独立故障场景 QA | 通过 | `019f4f47-5f24-7fa0-9632-960d0d0d5b36`：两项 finding 已关闭，12 项证据哈希复算一致，QA gate PASS |

## P2 数据契约与接口

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| C++14 SDK 无关契约编译 | Debug/Release 通过 | `artifacts/p2-contracts-20260711-122211`；Level4/WX；MSBuild exit 0 |
| 四类契约构造与校验 | 通过（实现自查） | FramePacket 深拷贝/尺寸校验；Detection/InspectionResult/RejectCommand 测试 |
| 接口可替换性 | 通过（实现自查） | 五类 fake 实现；不链接 Qt/Halcon/MVS/DAQ/TensorRT |
| 有界队列溢出和关闭 | 通过（实现自查） | DropOldest、拒绝保留 move-only 所有权、多等待者唤醒、关闭后排空、并发生产消费 |
| CigVision Release 回归 | 通过 | `artifacts/p2-main-regression-20260711-120926`；Rebuild exit 0 |
| 独立 reviewer | 通过 | `019f4f64-2c4c-7650-b78b-ca60cc35643c`：四项 finding 全部 resolved，无新增 finding，reviewer gate PASS |
| 独立 QA | 通过 | `019f4f64-4069-7011-9a9a-0e5cdb865d6b`：finding resolved，两配置各连续 20 次 7/7，QA gate PASS |

## P5-P8 本地运行 QA 场景

## P3 离线检测闭环

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 离线核心 Debug/Release 构建与测试 | 通过（返修自查） | `artifacts/p3-offline-20260711-132348/msbuild-offline-*.log`、`test-offline-*.log`，各 7/7 |
| 固定 8 图端到端与输入哈希 | 通过（实现自查） | `tests/fixtures/p3-samples.json`；`batch-output/summary.json`；8 JSON + 8 PNG |
| 空输入、错误、保存失败、队列满、停止/重跑 | 通过（实现自查） | `OfflineTests.cpp` 六组测试；两配置 exit 0 |
| 主程序 Release 回归 | 通过 | `main-build/manifest.json`、`msbuild-release.log`，exit 0 |
| 无硬件 UI 启动及单图流程 | 通过 | Computer Use；`ui-qa-observation.json`、`ui-qa-output`；预览可见、合格 1、错误 0 |
| 独立 reviewer | 通过 | `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f`；首轮 findings 全部 resolved，最终 reviewer gate PASS |
| 独立 QA 复跑 | 通过 | `019f4f8b-f75a-7a53-8de6-25206dfa2526`；最终 37 项文件记录哈希一致，两配置 7/7，QA gate PASS |
| 人工硬件 QA | 未请求/不声明 | P3 不连接相机，不读写 DAQNavi，不生成真实剔除 |

## P4 TensorRT 正式集成

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| TRT 10.15 Release 构建与 engine 加载 | 通过 | `artifacts/p4-tensorrt-20260711-150510`；Release Rebuild/batch exit 0 |
| 固定 116 图逐图结果与带框图 | 通过（运行完整性） | 116 JSON + 116 原图 + 116 带框图；processed=116、错误 0、3 组重复一致 |
| engine、尺寸和 CLI 负路径 | 通过 | invalid engine/shape exit 4；missing/conflicting CLI exit 2；不构造业务 UI/硬件对象 |
| 模型效果视觉抽查 | 通过（观察已记录） | 正式与独立 QA frame 1/3/6/12；重叠框、大框和标签遮挡记录到 KI-030 |
| 准确率/误检率/漏检率 | 未验证/不声明 | 无人工 ground truth、授权和训练集重叠证明 |
| P2/P3 回归 | 通过 | `artifacts/p2-contracts-20260711-145147`；`artifacts/p3-offline-20260711-151220` |
| 独立 reviewer | 通过 | `019f4ff6-8554-7700-992b-ab2773108818`；首轮 6 项全部 resolved，技术 gate PASS |
| 独立 QA | 通过 | `019f4ff6-9972-7b73-83a5-a288e6714ab5`；返修后独立证据目录 gate PASS |
| 相机/DAQ/剔除 QA | 未请求/不声明 | P4 入口在主窗口前运行，不接硬件，不生成 RejectCommand |

以下项目在相应阶段开始前均为“未开始”，不代表已验证：

| 场景 | 最早阶段 | 必需证据 |
| --- | --- | --- |
| Windows Debug/Release 构建和启动 | P1 | 构建日志、退出码、依赖清单 |
| 离线图片正常推理 | P3/P4 | 输入清单、结果文件、日志、时延 |
| 无效/损坏/空图片 | P3 | 明确错误且无部分写入 |
| 标注清单、数据划分、重复/泄漏和指标复算 | P5 | 数据 manifest、标注审计、评估报告、哈希 |
| 阈值/NMS/异常框/模型优化前后对照 | P5 | 同一冻结测试集的逐类、烟支级指标和错误样本 |
| 队列满、停止中断、重复启停 | P3/P6 | 自动测试或运行转录 |
| 模型缺失、引擎不兼容、GPU 不可用 | P4 | 失败日志和恢复行为 |
| 文件/录制流中断、格式异常、丢帧、乱序 | P6 | 本地故障注入、运行日志和汇总 |
| 模拟剔除编号正确且无真实 IO | P6 | frame_id 到 RejectCommand/回执的追踪记录及安全负路径 |
| Qt 深度学习主流程与现有风格一致 | P7 | Computer Use 截图、操作转录、控件清单和回归结果 |
| 删除/隐藏旧功能不破坏有效工作流 | P7 | 使用/依赖审计、迁移说明、回归和 UI QA |
| 磁盘满、保存失败、保留策略 | P8 | 故障注入与日志/指标 |
| 长时间运行和资源增长 | P8 | 时长、内存、GPU、磁盘、队列时间序列 |
| D 盘部署、升级与回滚 | P8 | 干净部署清单、哈希、启动/恢复转录 |

真实相机、DAQNavi 和真实剔除 QA 当前冻结。未来恢复时只能由用户或现场人员报告通过；Codex 不代填人工结果，也不使用上述本地 QA 替代。
