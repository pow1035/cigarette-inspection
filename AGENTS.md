# 烟支检测项目工作入口

## 项目主线

最终产品位于 `01_上位机_QT_新版_CigVision/源码`。深度学习/TensorRT 是产品检测主线；老版 Qt/Halcon 工程只作为传统算法、业务规则和时序参考，不继续扩展。

## 每次开始前

1. 先读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`、`docs/known-issues.md` 和 `docs/code-audit.md`。
2. 只执行 `docs/task-plan.md` 标出的当前阶段，不跨阶段堆叠功能。
3. 按 `codex-long-task-architecture` 循环工作：实现、自查、验证、独立评审/QA（工具可用时）、更新证据、再过提交门。
4. 每阶段结束时更新 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-packet.md`、`docs/review-results.md` 和相关状态。
5. 未通过提交门时，不声称阶段完成，不提交、合并或发布。
6. 当前 `CURRENT_PHASE` 是 P8；测试集保持 P8 76 项，PowerShell 静态门保持 13 个脚本和 7 个 CLI 契约。主机证据已返修为现场 challenge 采集、v2 host report、v4 wrapper/receipt 与外置 HMAC 密钥；HEAD `6886856` 的最终 `--full` 门及最终 reviewer/QA 均已 PASS。提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 已 push；2026-07-26（Asia/Shanghai）的 GitHub Actions `Local gates` 运行 `30169095184`（job `89706968923`）SUCCESS，全部步骤通过、check-run annotations 为空，Node 20 warning 已消失，checkout/setup-python v6 均固定完整提交 SHA。最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` PASS（P0/P1/P2/P3=0/0/0/0，对抗 18/18），QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS（P0/P1/P2/P3=0/0/0/0，对抗 17/17）。P8 整体因 P5/P6/P7 外部输入，以及真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV 环境、数据、许可和硬件阻断保持进行中。P5-P8 只做本地数据效果、实时仿真、Qt 产品化和交付预验收，不得擅自恢复现场硬件路线。
7. 最新推送核验：提交 `6d492528c7f9c0b0b2e3cc70e1c60a74cb62cede` 已 push；对应 GitHub Actions `Local gates` run `30188084112`（job `89756233568`）SUCCESS。该运行覆盖当前树的 core 门和 PowerShell 静态门，仍不扩大 Windows/MSVC、Qt、GPU、TensorRT、D 盘或硬件声明。

## 文档维护节奏

- 编排代理必须在每个实现切片后、独立 review/QA 结果返回后、最终门禁前各启动一次 documentation maintenance agent。
- documentation maintenance agent 只按当时工作树和本轮授权范围更新 `README.md`、交接说明、`AGENTS.md` 与 `docs/`：核对阶段、测试计数、验收状态、证据边界、评审状态和交叉链接；运行相关只读文档检查，不提前写 PASS。
- documentation maintenance agent 同时判断 cleanup 是否到期；到期时在 `docs/task-plan.md` 和 `docs/progress-log.md` 记录原因，并请求独立 cleanup agent 执行，不能把文档整理冒充代码 cleanup。

文档导航：需求与边界见 `requirements.md`、`architecture.md`、`decisions.md`；执行与风险见 `task-plan.md`、`known-issues.md`、`code-audit.md`；P5 资料与真值规则见 `p5-source-inventory.md`、`p5-labeling-guide.md`；Windows 构建与 P8 目标机操作见 `windows-build-baseline.md`、`windows-target-execution.md`；验收与证据见 `acceptance-criteria.md`、`qa-checklist.md`、`evidence-matrix.md`、`review-packet.md`、`review-results.md`；一致性与运行证据见 `golden-principles.md`、`observability.md`、`progress-log.md`。以上文件均位于 `docs/`。

## 不可破坏的边界

- 真实相机、DAQNavi 输入输出、卷烟机同步和真实剔除均冻结；当前阶段不得写真实 IO，也不得把本地仿真表述为现场验证。
- 相机回调只做轻量采集和入队，推理、保存、统计、剔除不得阻塞回调线程。
- Qt 产品化沿用新版工程现有 UI 风格；删除或隐藏功能前必须完成使用清单、依赖分析和回归验证。
- 不提交 `07_运行环境与依赖包/`、构建产物、缓存或超大测试压缩包。
- 不提交 `artifacts/`、模型/engine、账号、许可证、设备序列号、客户资料、密钥或未脱敏机器信息。
- 不直接复制老版硬编码路径、裸指针生命周期问题和无边界队列实现。

更详细的规则、验收标准和证据记录都在 `docs/` 中。
