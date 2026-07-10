# 烟支检测项目工作入口

## 项目主线

最终产品位于 `01_上位机_QT_新版_CigVision/源码`。老版 Qt/Halcon 工程只作为生产闭环参考，YOLO/TensorRT 代码作为深度学习检测器来源。

## 每次开始前

1. 先读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`、`docs/known-issues.md` 和 `docs/code-audit.md`。
2. 只执行 `docs/task-plan.md` 标出的当前阶段，不跨阶段堆叠功能。
3. 按 `codex-long-task-architecture` 循环工作：实现、自查、验证、独立评审/QA（工具可用时）、更新证据、再过提交门。
4. 每阶段结束时更新 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-packet.md`、`docs/review-results.md` 和相关状态。
5. 未通过提交门时，不声称阶段完成，不提交、合并或发布。

文档导航：需求与边界见 `requirements.md`、`architecture.md`、`decisions.md`；执行与风险见 `task-plan.md`、`known-issues.md`、`code-audit.md`；当前 Windows 基线见 `windows-build-baseline.md`；验收与证据见 `acceptance-criteria.md`、`qa-checklist.md`、`evidence-matrix.md`、`review-packet.md`、`review-results.md`；一致性与运行证据见 `golden-principles.md`、`observability.md`、`progress-log.md`。以上文件均位于 `docs/`。

## 不可破坏的边界

- 现场剔除输出默认关闭；只有显式进入硬件联调阶段并完成安全检查后才能写 IO。
- 相机回调只做轻量采集和入队，推理、保存、统计、剔除不得阻塞回调线程。
- 不提交 `07_运行环境与依赖包/`、构建产物、缓存或超大测试压缩包。
- 不直接复制老版硬编码路径、裸指针生命周期问题和无边界队列实现。

更详细的规则、验收标准和证据记录都在 `docs/` 中。
