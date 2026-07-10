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
| Windows 环境检查脚本 | 未验证 | 当前 macOS 没有 pwsh；需目标 Windows 运行 |
| Debug/Release MSBuild | 未验证 | 尚无 Windows 构建日志 |
| 应用启动与重复启停 | 未验证 | 需要 Windows UI/运行证据 |
| 相机和 IO 硬件 QA | 未请求/不声明 | P1 不连接真实硬件 |
| 独立代码 reviewer | 返修复核中 | 首轮发现 process include 阻断、回调/Stop 竞态、编号快照和 IO 退出问题 |
| 独立故障场景 QA | 返修复核通过 | 最新工作树无确定性 P1 静态阻断；MVS 晚到回调保留为目标机风险 |

## 后续运行 QA 场景

以下项目在相应阶段开始前均为“未开始”，不代表已验证：

| 场景 | 最早阶段 | 必需证据 |
| --- | --- | --- |
| Windows Debug/Release 构建和启动 | P1 | 构建日志、退出码、依赖清单 |
| 离线图片正常推理 | P3/P4 | 输入清单、结果文件、日志、时延 |
| 无效/损坏/空图片 | P3 | 明确错误且无部分写入 |
| 队列满、停止中断、重复启停 | P3/P5 | 自动测试或运行转录 |
| 模型缺失、引擎不兼容、GPU 不可用 | P4 | 失败日志和恢复行为 |
| 相机断连、像素格式异常、丢帧 | P5/P6 | 运行日志和现场观察 |
| 模拟剔除编号正确 | P5 | frame_id 到 RejectCommand 的追踪记录 |
| 真实剔除输出 | P6 | 用户/现场人员安全确认和人工 QA 报告 |
| 磁盘满、保存失败、保留策略 | P7 | 故障注入与日志/指标 |
| 长时间运行和资源增长 | P7 | 时长、内存、GPU、磁盘、队列时间序列 |

真实硬件 QA 只能由用户或现场人员报告通过；Codex 不代填人工结果。
