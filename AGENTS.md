# 烟支检测项目工作入口

## 项目主线

最终产品位于 `01_上位机_QT_新版_CigVision/源码`。深度学习/TensorRT 是产品检测主线；老版 Qt/Halcon 工程只作为传统算法、业务规则和时序参考，不继续扩展。

## 每次开始前

1. 先读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`、`docs/known-issues.md` 和 `docs/code-audit.md`。
2. 只执行 `docs/task-plan.md` 标出的当前阶段，不跨阶段堆叠功能。
3. 按 `codex-long-task-architecture` 循环工作：实现、自查、验证、独立评审/QA（工具可用时）、更新证据、再过提交门。
4. 每阶段结束时更新 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-packet.md`、`docs/review-results.md` 和相关状态。
5. 未通过提交门时，不声称阶段完成，不提交、合并或发布。
6. 当前 `CURRENT_PHASE` 是 P8；P8 Python 清单为 81 项（历史 76 + 连续运行 5），修复提交 `cc7a3ab` 已实跑连续专项 5/5、P8 精确 81/81、公开 wrapper 的 C++17 contract run + self-verify + 独立二次 verify，并在 Mac 与 Colima/Linux（`CLK_TCK=100`）通过原始 `--full`：P5 100、P6 17、P8 81、C++17/C++14、20 次重复、ASan/UBSan 均 PASS。当前 CI 修复独立 reviewer 与 QA/observability 均 PASS，P0/P1/P2/P3=0/0/0/0；修复提交的 hosted `Local gates` run `30221330296`（job `89844182732`）已 SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空。正式 project `run` 在同一路径内用 `--compiler/--standard` 受控编译并快照 8 个源文件、compiler/compile contract 与 executable；外部 `--build-provenance` 仅允许 contract test helper，伪造 project provenance 必须 exit 2 且不创建 evidence。tool dependencies 现同时绑定 `p8_continuous_soak.py`、旧 `p8_soak_evidence.py` 的 path/size/SHA/snapshot，以及 POSIX 可信绝对路径 `ps` 的 canonical path/size/SHA/version probe/固定 argv/snapshot；helper 漂移、PATH shadow、dependency record/helper/ps snapshot/identity 篡改均拒绝。`durationSeconds` 是已验真的 runtime 工作时长，`processLifetimeSeconds` 是冻结在真实退出点的进程寿命；Linux 进程组 CPU 由固定 `ps` 枚举后读取各 PID 的 `/proc/<pid>/stat` 汇总，pure-sleep 伪 runtime 必须被 CPU 门拒绝。`contract-test-v1` 的 locked 最低时长仍为 0.12 秒，public wrapper 与 C++17/C++14 direct project contract 门实际运行 1.0 秒；CPU ≥0.01 秒、RSS 增长 ≤16 MiB。`local-sdkfree-v1` 固定 2×300 秒、60 秒 warm-up、每轮 ≥240 samples/progress、progress gap ≤5 秒、sessions ≥240、frames ≥122880、CPU ≥10 秒、RSS 增长 ≤64 MiB、磁盘 ≥1 GiB。runtime 必须逐轮对齐 Offline 与 ProductRuntimeState 的 OK/NG/Error；inventory 不忽略 `.tmp`；Windows clean gate 要求两次 fresh tree enumeration、no residual 与 enumeration success，并有二次枚举失败的回归。KI-048 独立 reviewer 最终 PASS，P0/P1/P2/P3=0/0/0/0。正式 `local-sdkfree-v1` evidence 已在 `artifacts/p8-continuous-local-20260727-final-v2` 完成 self-verify、主代理独立 verify 和 reviewer 两次独立 verify：两轮分别运行 300.046591/300.020852 秒，累计 3,793,408 帧，manifest SHA-256 为 `2bd420fab44acbed98315ffac5cf6a2b22567dcae42d98e522be35de3a7bcdd4`；正式 evidence QA/observability/cleanup PASS，reviewer P0/P1/P2/P3=0/0/0/0，QA 审计仅有一个非阻断 P3。旧 `artifacts/p8-continuous-local-20260727-invalid-dependency-gap` 无 manifest、被 Git 忽略，只隔离保留作失败审计，不得引用、混入 `final-v2` 或交付；删除仅按用户/留存策略另行执行。P8 v2 本地连续工具证据可标 PASS，但 P8/AC-08 整体仍因真实 Windows/Qt/GPU/TensorRT/SDK/D 盘、数据、许可和硬件保持进行中；不得外推为产品验收。HEAD `6886856` 的旧 76/76、历史 reviewer/QA 与 hosted run 只作为历史快照。
7. P8 v2 修复实现边界：提交 `cc7a3ab6fdeeb40dd7289a2962aae70abee8252b` 已 push；其 GitHub Actions `Local gates` run `30221330296`（job `89844182732`）SUCCESS，GitHub API 返回的 10 个已执行步骤（编号 1–7、13–15）全部成功，check-run annotations 为空。首轮提交 `5f6a06a`/run `30219159920`（job `89838475434`）FAIL 作为有效历史发现保留：Linux CPU tick 与 POSIX core-dump fixture 两项缺口均已修复；后续文档-only 收口不得改写该实现/evidence 边界。
8. 历史 CI 硬化证据保留：提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 的 GitHub Actions `Local gates` 运行 `30169095184`（job `89706968923`）SUCCESS，全部步骤通过、check-run annotations 为空、原 Node 20 warning 已消失，checkout/setup-python v6 均固定完整提交 SHA；最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` 与 QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS。它不覆盖当前 P8 v2 增量。

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
- 不提交 `artifacts/`，不新增或更新生成的模型/engine；仓库既有受控模型资产另行治理。账号、许可证、设备序列号、客户资料、密钥或未脱敏机器信息一律禁止提交。
- 不直接复制老版硬编码路径、裸指针生命周期问题和无边界队列实现。

更详细的规则、验收标准和证据记录都在 `docs/` 中。
