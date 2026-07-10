# 进度日志

## 2026-07-10 - P0 启动

- 项目已整理到 `/Users/c/Desktop/烟支检测项目_整理版`，Git 分支为 `main`，远端为私有仓库 `pow1035/cigarette-inspection`。
- 启动 `codex-long-task-architecture` 工作流，长期目标是按阶段完成新版 Qt 深度学习烟支检测闭环。
- Git 检查：工作树在 P0 编辑前干净，HEAD 为 `fde0eaa`。
- 本地体积检查：依赖包目录约 6.4G，继续保持 Git 忽略；现有 Git 目录约 321M。
- 三个独立只读审计已完成：新版 Qt、老版传统闭环、TensorRT/依赖。结果已汇总到 `docs/code-audit.md` 并映射到 `docs/known-issues.md`。
- 已创建 `AGENTS.md`、需求、架构、计划、验收、决定、问题、QA、证据、评审和可观测性文档。
- 首轮验证：`scripts/validate_project_docs.sh` 通过；`git diff --check` 通过；skill `light_gate.py` 返回 3 个评审包字段提示，正在补齐后复验。
- 独立代码审计只证明静态现状；Windows 构建、GPU、相机和 IO 仍未验证。
- 独立 P0 reviewer 发现两项阻断：错误的 CMake 文件存在性结论、提交门记录未收口；另发现未跟踪文件未被 `git diff --check` 覆盖。
- documentation maintenance 判定 P0 定向 cleanup 到期，大范围历史文档 cleanup 暂不到期。
- 已完成第一轮定向修复；增强后的文档验证、skill 严格门、脚本语法和 diff 检查均通过，业务目录限定状态为空。
- 原 reviewer 最终确认错误 CMake 结论和验证脚本覆盖问题已 resolved；documentation maintenance 确认 P0 定向 cleanup 已收口。
- AC-00-01 至 AC-00-07、QA、证据矩阵、评审包和评审结果已统一为通过，P0 关闭。
- 当前阶段切换到 P1：新版工程构建基线与硬化。P1 尚无 Windows 构建通过声明。

## 2026-07-10 - P1 静态基线与首批硬化

- 独立 explorer 完成 Windows 工具链静态审计：VS17/v143、Qt 5.9.9，Debug Halcon 25.05 与 Release Halcon 22.11 分裂。
- 新增 windows-build-baseline.md、P1 静态验证脚本和 Windows 环境检查 PowerShell。
- 修复 2-2 相机映射、回调帧元数据借用指针、数组释放路径和队列锁/容量。
- 相机改为 Run 显式启动、Stop 显式停止；析构停止相机和 IO 线程并清理资源。
- IO 编号改为互斥保护的一次性完整快照，组件编号统一使用 0-99 环绕计算。
- Release 补 MVS 路径/库；主工程增加 process ProjectReference，process x64 与主程序同目录输出。
- 陈旧 testWrite 从新版工程构建清单移除；config.ini 的 rejectEnabled 改为 false。
- 首次脚本运行发现 Bash 模板转义错误，未计入证据；修复后 P1 静态门、文档门、XML、shell 语法、diff check 和 skill 严格门均通过。
- 独立 reviewer 与故障场景 QA 已完成首轮，发现 process 父目录 include、回调退出竞态、编号快照、IO 初始化反馈和停止语义等问题。
- 已集中返修：process 父目录 include、活动回调屏障、Stop 后等待再清队列、IO 同步初始化/可重复启停、连续读取错误上限、完整编号快照、编号边界和 Stop 返回值记录。
- 返修后 P1 静态门、文档门、三个工程 XML、shell 语法和 diff check 再次通过；正在等待独立返修复核。
- 队列消费者属于 P3，轻量原始帧回调属于 P2/P3，模拟/真实剔除安全门属于 P5/P6，均保持开放，不计入 P1 已完成能力。
- 当前仍没有 Windows MSBuild、程序启动、相机或 IO 运行证据，P1 不能关闭。
- 新增 `run_windows_p1_build.ps1`，用于目标机自动执行两配置环境检查、MSBuild、关键输出核对和 SHA-256 证据清单；当前 macOS 未运行该 PowerShell。
- 原 reviewer 返修复核继续发现 shutdown 裸回调上下文和运行中工位参数竞争；已改为进程期回调守卫 + detach 等待，并在 Run 前校验/冻结 10-30、20-40 工位范围。
- 当前返修复核结果：P1 静态门、项目文档门、三个工程 XML、shell 语法、`git diff --check` 和 `light_gate.py --strict` 均通过；独立 QA 已复核回调屏障、IO 初始化握手、编号快照和停止失败处理。P1-03 静态切片关闭。
- P1 仍未关闭的唯一阶段证据是目标 Windows x64 的 Debug/Release MSBuild、输出文件哈希和程序启动记录；AC-01-02、KI-001 保持未验证，不能用 macOS 检查替代。

## 已完成的历史工作

- 原始杂乱资料已整理为 `00` 至 `07` 和 `99` 目录。
- 缓存、重复内容和无必要构建产物已清理；安装包/运行依赖单独保留在本机目录。
- 本地 Git 初始化完成并推送到 GitHub 私有仓库。
- 大依赖包、超大图片压缩包和超大 PPT 已通过 `.gitignore` 排除普通 Git 历史。

历史条目只说明操作已经发生，不等于当前代码已通过构建或现场验收。
