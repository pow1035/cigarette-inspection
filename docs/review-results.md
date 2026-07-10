# 评审结果

## P1

状态：独立 reviewer 和故障场景 QA 已完成首轮；P1 阻断项已返修。QA 已对最新工作树完成返修复核，P1 静态切片收口；Windows/MSBuild、SDK 和硬件运行证据仍未收集。实现代理自查和静态门不计为独立复核。

- reviewer：独立代理 `019f49ea-1bf9-7f60-971e-fd69f6b0e57c`。
- QA：独立代理 `019f49ea-1ba5-7693-a15c-8036c6003ae7`。

| 严重度 | 首轮发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | process 工程缺少父目录 include，`<paramStructs.h>` 无法解析 | 已给 Debug/Release x64 增加 `$(ProjectDir)..`；静态复核通过，仍待 MSBuild |
| 阻断 | Stop/析构与活动相机回调存在竞态 | 已增加活动回调计数、进程期守卫和等待屏障；静态复核通过，仍待 Windows/MVS QA |
| 高 | 多个 atomic 编号字段不能形成同一触发周期快照 | 已改为互斥保护的单次结构体提交/读取；静态复核通过 |
| 高 | IO 初始化失败仍被上层报告成功，用户 Stop 不结束读取任务 | 已改同步 initialize、Run 时 prepareStart、Stop 时 requestStop + 等待；静态复核通过 |
| 高 | 组件 1 可产生 100-255，组件位置无效时静默按 0 处理 | 原始编号大于 99 明确丢弃；派生编号使用 0-99 环绕；静态复核通过 |
| 高 | Stop 和启动回滚忽略 SDK 返回值 | 已记录返回码；Close 返回 Stop/Close/Destroy 首个失败；仍待 Windows QA |
| 高 | 临时灰度缓冲的 Halcon 所有权缺少证明 | 增加 `CopyImage` 形成队列对象副本并捕获回调异常；仍待目标 Halcon 验证 |
| 高 | Stop 失败后 shutdown 仍可能释放裸回调 `pUser`，且运行中读取可变工位参数 | 回调改用进程期守卫并在 Stop/shutdown detach；Run 前校验并冻结 10-30/20-40 工位参数；静态复核通过，晚到帧仍待 Windows QA |
| 中 | 静态脚本遗漏 process include、回调屏障、快照及构建基线文档 | 已扩充两个验证脚本，返修后运行通过 |
| 后续阶段 | 队列无消费者、回调仍做整帧转换、真实剔除门不存在 | 分别由 KI-015、KI-023、KI-024 跟踪到 P2/P3/P5/P6，不作为 P1 已实现能力 |

首轮 review 明确指出：没有 Windows/MSBuild、Qt、Halcon、MVS 或 DAQNavi 运行证据。即使返修复核通过，AC-01-02 和 P1 阶段仍必须保持未验证，直到目标机完成构建与启动。

## P1 QA 返修复核

独立 QA 重新读取最终工作树并实跑 P1 静态门、文档门、`git diff --check`、`xmllint` 和 skill 严格门，全部 exit 0。未发现确定的成员引用、信号槽签名、C++ 语法或必现死锁问题；确认 Stop 失败锁定再次 Run、IO 故障 queued signal 安全停止、完整编号快照和 process 双配置 include 路径均存在。

返修期间又发现 Stop 失败 shutdown 的裸回调上下文风险和运行中工位参数竞争。第二轮返修把 SDK `pUser` 改为进程期存活的回调守卫，Stop/shutdown 会先 detach 窗口并等待活动回调；IO 任务在 Run 前校验范围并冻结工位参数。MVS 是否在 Stop 后产生会污染下一次 Run 的晚到帧仍由 KI-026 追踪，不能以静态通过替代运行证据。

## P0

状态：P0 独立评审与返修复核已通过。

源码审计结论已写入 `docs/code-audit.md`，覆盖新版 Qt、老版闭环和 TensorRT 原型。审计均为静态只读检查，不作为 Windows、GPU、相机或 IO 的运行通过证据。

## 独立评审记录

- reviewer：独立代理 `019f49b6-62f7-7320-8f43-228c21bfca93`。
- documentation maintenance：独立代理 `019f49b6-62b5-7511-9b1c-93f055de83bc`。
- 范围：P0 入口、需求、架构、计划、验收、证据、代码审计、验证脚本和提交门。

| 严重度 | 发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | 错误声称 `test_yolo_trt_v2.cpp` 不存在 | 已纠正、加入反证说明并经 reviewer 复核通过 |
| 阻断 | 评审包和证据矩阵未记录本次评审/最新门禁 | 已同步并经 reviewer 复核通过 |
| 高 | `git diff --check` 不检查未跟踪 P0 文件 | 验证脚本已增加未跟踪文件检查和业务目录变更检查，返修后运行通过 |
| 高 | 历史 TensorRT 文档的完成/性能声明缺少当前证据 | 已在 `code-audit.md` 明确历史资料边界，KI-022 追踪后续整理 |
| 中 | AC 状态和 QA/证据记录不同步 | 已同步为最终通过状态 |
| 中 | `AGENTS.md` 对深层文档导航不完整 | 已补充紧凑导航行 |
| 中 | 大范围历史文档重复和命名漂移 | P0 不做大清理；P4 指定唯一入口，P8 前强制清理 |

## 最终复核

原 reviewer 对四项返修逐项复核：错误 CMake 结论 resolved；最新评审与命令输出在写入本结论后 resolved；验证脚本覆盖 resolved；AC/QA/证据状态在本次同步后 resolved。documentation maintenance 同时确认过期 warning、AC-00-07、AGENTS 导航、历史 TensorRT 声明边界和状态一致性均已解决，历史目录大清理留到 P4/P8。

P0 已通过独立评审和提交门，可以关闭；Windows、GPU、相机、IO 和算法效果不属于 P0 通过声明。

当前不把实现代理自查计为独立评审。
