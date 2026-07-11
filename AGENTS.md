# 烟支检测项目工作入口

## 项目主线

最终产品位于 `01_上位机_QT_新版_CigVision/源码`。深度学习/TensorRT 是产品检测主线；老版 Qt/Halcon 工程只作为传统算法、业务规则和时序参考，不继续扩展。

## 每次开始前

1. 先读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`、`docs/known-issues.md` 和 `docs/code-audit.md`。
2. 只执行 `docs/task-plan.md` 标出的当前阶段，不跨阶段堆叠功能。
3. 按 `codex-long-task-architecture` 循环工作：实现、自查、验证、独立评审/QA（工具可用时）、更新证据、再过提交门。
4. 每阶段结束时更新 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-packet.md`、`docs/review-results.md` 和相关状态。
5. 未通过提交门时，不声称阶段完成，不提交、合并或发布。
6. 当前 `CURRENT_PHASE` 是 P5；P5-P8 只做本地数据效果、实时仿真、Qt 产品化和交付预验收。不得擅自恢复现场硬件路线。

文档导航：需求与边界见 `requirements.md`、`architecture.md`、`decisions.md`；执行与风险见 `task-plan.md`、`known-issues.md`、`code-audit.md`；当前 Windows 基线见 `windows-build-baseline.md`；验收与证据见 `acceptance-criteria.md`、`qa-checklist.md`、`evidence-matrix.md`、`review-packet.md`、`review-results.md`；一致性与运行证据见 `golden-principles.md`、`observability.md`、`progress-log.md`。以上文件均位于 `docs/`。

## 不可破坏的边界

- 真实相机、DAQNavi 输入输出、卷烟机同步和真实剔除均冻结；当前阶段不得写真实 IO，也不得把本地仿真表述为现场验证。
- 相机回调只做轻量采集和入队，推理、保存、统计、剔除不得阻塞回调线程。
- Qt 产品化沿用新版工程现有 UI 风格；删除或隐藏功能前必须完成使用清单、依赖分析和回归验证。
- 不提交 `07_运行环境与依赖包/`、构建产物、缓存或超大测试压缩包。
- 不提交 `artifacts/`、模型/engine、账号、许可证、设备序列号、客户资料、密钥或未脱敏机器信息。
- 不直接复制老版硬编码路径、裸指针生命周期问题和无边界队列实现。

更详细的规则、验收标准和证据记录都在 `docs/` 中。
