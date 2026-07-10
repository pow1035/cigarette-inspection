# 证据矩阵

证据状态：待收集、通过、失败、未验证、未请求/不声明。

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-00-01 | 项目入口可导航到记录系统 | `AGENTS.md`；`scripts/validate_project_docs.sh` 输出；独立 reviewer 结论 | 通过 | 仅证明入口与文件存在 |
| AC-00-02 | 需求被拆分并记录 | `docs/requirements.md`；独立 reviewer 结论 | 通过 | 现场参数仍需用户确认 |
| AC-00-03 | 架构边界和安全门被记录 | `docs/architecture.md`；独立 reviewer 结论 | 通过 | 尚未通过代码或运行验证 |
| AC-00-04 | 当前阶段唯一 | `docs/task-plan.md` 唯一 CURRENT_PHASE 标记（现为 P1）；验证脚本输出；独立 reviewer 结论 | 通过 | 只覆盖文档状态 |
| AC-00-05 | 已知问题和验证边界可见且事实经复核 | `docs/known-issues.md`；`docs/code-audit.md`；三名 explorer 审计；独立 reviewer 首轮发现与最终复核 | 通过 | 只证明静态代码现状，不证明运行行为 |
| AC-00-06 | P0 有验证和独立评审 | `docs/review-packet.md`、`docs/review-results.md`；reviewer `019f49b6-62f7-7320-8f43-228c21bfca93` 最终复核；documentation maintenance `019f49b6-62b5-7511-9b1c-93f055de83bc` 复核 | 通过 | 不包含 Windows/硬件 QA |
| AC-00-07 | P0 未修改业务源码 | 增强后的 `scripts/validate_project_docs.sh` exit 0；限定业务目录的 `git status` 输出为空 | 通过 | P0 新增项仅为 `AGENTS.md`、`docs/`、`scripts/` |

| AC-01-01 | Windows 工具链清单可执行 | docs/windows-build-baseline.md；scripts/check_windows_build_env.ps1 | 通过 | PowerShell 尚未在 Windows 实跑 |
| AC-01-02 | 目标 Windows 构建成功 | `run_windows_p1_build.ps1` 已就绪；尚无 MSBuild 输出或 manifest | 未验证 | macOS 不能替代目标构建 |
| AC-01-03 | 映射、内存、帧元数据和编号快照问题已返修 | scripts/validate_p1_static.sh exit 0；源码 diff；独立 QA 返修复核 | 通过（静态） | 不证明相机运行；轻量回调留在 P2/P3 |
| AC-01-04 | P1 无剔除输出实现且默认关闭 | config.ini 的 rejectEnabled=false；新版工程无 DO 输出实现；P1 静态门 | 通过（静态） | 不是运行时硬件安全门，KI-024 继续追踪 |
| AC-01-05 | 采集生命周期有进程期回调守卫、双层屏障、故障锁定、IO 故障安全停止和清理路径 | CigVision/MyCamera/readIOTask diff；P1 静态门；独立 QA 返修复核 | 通过（静态） | 晚到帧污染、重复启停、Stop 失败和断连仍需 Windows/MVS QA |
| AC-01-06 | 工程依赖关系和父目录 include 已声明 | 三个 vcxproj 通过 xmllint；process 的 `$(ProjectDir)..`；P1 静态门；独立 QA 返修复核 | 通过（静态） | 仍需 MSBuild 证明路径及依赖真实存在 |
| AC-01-07 | testWrite 不参与新版构建 | vcxproj/filters 无 testWrite；P1 静态门 | 通过（静态） | 历史文件仍保留在目录 |

P2 及后续验收项在对应阶段开始前保持未验证。
