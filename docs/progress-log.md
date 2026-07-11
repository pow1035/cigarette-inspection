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
- 新增 `run_windows_p1_build.ps1`，用于目标机自动执行两配置环境检查、MSBuild、关键输出核对和 SHA-256 证据清单；此条为静态切片历史记录，后续 Windows 取证尝试见下节。
- 原 reviewer 返修复核继续发现 shutdown 裸回调上下文和运行中工位参数竞争；已改为进程期回调守卫 + detach 等待，并在 Run 前校验/冻结 10-30、20-40 工位范围。
- 当前返修复核结果：P1 静态门、项目文档门、三个工程 XML、shell 语法、`git diff --check` 和 `light_gate.py --strict` 均通过；独立 QA 已复核回调屏障、IO 初始化握手、编号快照和停止失败处理。P1-03 静态切片关闭。
- P1 仍未关闭的唯一阶段证据是目标 Windows x64 的 Debug/Release MSBuild、输出文件哈希和程序启动记录；AC-01-02、KI-001 保持未验证，不能用 macOS 检查替代。

## 2026-07-10 - P1 Windows 验证尝试

- 按要求在 VS2022 Developer PowerShell 语境运行 `powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All`。
- 首次取证 `artifacts/p1-windows-20260710-155819` 失败于 Windows PowerShell 5.1 将 `.ps1` 中中文路径字面量误读为 mojibake，导致 `config.ini` 路径不存在；未进入依赖检查或 MSBuild。
- 已做最小 P1 脚本修复：`check_windows_build_env.ps1` 和 `run_windows_p1_build.ps1` 改为动态定位 `CigVision.sln`，避免中文路径字面量；`run_windows_p1_build.ps1` 在失败时也写出 `manifest.json`、环境日志和失败步骤。
- 最终取证目录：`artifacts/p1-windows-20260710-160725`。VS2022 Developer PowerShell v17.11.2 可启动，MSBuild 路径为 `E:\visual studio\\MSBuild\Current\Bin\amd64\MSBuild.exe`；manifest 记录 `OverallResult=failed-before-msbuild`、`OverallCommandExitCode=1`。
- Debug 环境检查 exit 1：`qmake.exe` 不在 PATH；`C:\Program Files\MVTec\HALCON-25.05-Progress` 不存在；MVS header/lib 不存在；`C:\Advantech\DAQNavi\Inc\bdaqctrl.h` 不存在。Release 未执行，因为 Debug 环境检查前置失败。
- 未进入 MSBuild，无 `msbuild-*.log`，无 `CigVision.exe/process.dll` 输出和 SHA-256 构建清单；未启动程序，未收集 UI/Computer Use 证据。
- 本机没有 `bash`/Git Bash，`validate_p1_static.sh` 和 `validate_project_docs.sh` 本轮无法执行，记录为 degraded；`git diff --check` 和 PowerShell 解析检查通过。
- 当前 P1 不能关闭，不能进入 P2；MVS 相机、DAQNavi IO、Halcon 运行时继续记录为未验证/阻断。

## 2026-07-10 - P1 Windows 依赖补齐与第二次取证

- Qt 5.9.9 msvc2017_64 已核验于 `D:\smokeqt\5.9.9\msvc2017_64`；QtMsBuild 3.2.0.47 位于 `D:\cigarette-inspection-deps\QtMsBuild\3.2.0-rev.47`，独立 Qt smoke exe 构建和启动 exit 0。
- 海康 MVS 5.0.1 / SDK 4.8.0.3 已安装；开发目录为 `D:\Hikrobot\MVS\MVS\Development`，共享 x64 runtime 由厂商安装到 Windows Common Files。未连接相机，不声明采集通过。
- 研华 XNavi 4.0.5.0 / DAQNavi 已安装到用户实际选择的 `D:\advantexh\DAQNavi`；`bdaqctrl.h` SHA-256 为 `37FFF4AEBA3C3777911F66A4CC718B1A83947EBA51AA4255337D041E9EECCA64`，系统 x64 `biodaq.dll` 版本 4.1.22.0。未枚举 PCIE-1730 或执行 IO 读取。
- 最小 P1 路径适配：新增 `CIGVISION_DAQNAVI_ROOT` 解析并将工程 DAQNavi include 改为该变量；同时沿用 `MVCAM_COMMON_RUNENV` 与 Qt 环境变量。未修改 IO、剔除、相机或 TensorRT 业务逻辑。
- `artifacts/p1-windows-20260710-212027` 首次证明 Qt/MVS/DAQNavi 已就绪，但 All 在 Debug 失败后提前终止，Release 仅有手工预检；该目录由后续取证替代为当前证据。
- 独立 reviewer/QA 指出 MVS 路径未显式传给 MSBuild、All 提前终止、HALCON 仅检查空目录、DAQ runtime 未固化进 manifest 和评审文档冲突。已做集中返修。
- `artifacts/p1-windows-20260710-213048` 完成首轮集中返修取证；随后补充 MVS/DAQNavi runtime 环境失败门和混合结果 `partial` 语义。
- 当前正式取证目录为 `artifacts/p1-windows-20260710-213607`。All exit 1，manifest 同时记录 Debug/Release 两个 environment-check failure；两配置分别检查 D 盘目标 HALCON 的 C++ header、x64 lib 和 runtime，均缺失。MVS/DAQNavi x64 runtime 在两份环境日志中均通过。
- manifest 已固化 MVS runtime 4.8.0.3、DAQNavi x64 `biodaq.dll` 4.1.22.0 及 SHA-256；MSBuild 参数显式传入 MVS、DAQNavi 和两套独立 HALCON 根。
- 未生成 msbuild 日志、`CigVision.exe`、`process.dll` 或输出哈希，程序未启动。MVTec 官方下载要求 MVLogin；两套 HALCON 不擅自统一，HALCON 12 不使用。
- P1 继续阻断，不能进入 P2。MVS 相机、DAQNavi 实际设备/IO、HALCON 运行时均保持未验证；真实剔除继续禁止。
- 独立 reviewer `019f4c33-ecab-7591-976f-52c7cc1fd243` 与 QA `019f4c34-157d-77c2-b504-f98ef223e776` 完成返修复核：此前高风险问题均已解决；P1 因 HALCON、MSBuild、输出和启动证据缺失继续阻断。

## 2026-07-10 - P1 依赖获取与正式复验

- MVTec Software Manager 1.7.2.1371 下载包 SHA-512 已与官网校验值一致；程序路径和数据/缓存路径分别设置为 `D:\MVTec`、`D:\MVTecData`，未在 C/E 盘安装 HALCON。
- 登录后的官方目录可识别 HALCON 22.11.4.0 Steady Windows x64 完整包和校验文件；自动下载多次中止且未产生完整文件，未安装、未计为依赖证据。当前官方目录不提供 25.05 Progress，不猜测、不用其他版本替代。
- 在 64 位 Windows 内核 `10.0.22631.0`、Visual Studio Community 2022 17.11.2（安装于 `E:\visual studio`）的 Developer PowerShell 中重新执行 All，正式证据更新为 `artifacts/p1-windows-20260710-235900`。目录含 `full-terminal.log`、`env-debug.log`、`env-release.log` 和 `manifest.json`。
- 顶层 exit 1，`OverallResult=failed-before-msbuild`；Debug/Release 均为 environment-check exit 1，仅各自 HALCON header/x64 lib/runtime 缺失。Qt、MVS、DAQNavi 和 MSBuild 检查通过。
- 未生成 `msbuild-*.log`、`CigVision.exe`、`process.dll` 或输出哈希，程序未启动。相机、DAQNavi 实际 IO、HALCON 运行时继续为未验证；真实剔除仍禁止，P1 不能关闭且不能进入 P2。
- 本轮独立 reviewer `019f4eec-cbd9-7263-bd2c-826506e81f10` 指出环境门主要验证文件存在，不能声明已证明版本组合兼容；AC-01-01 已降为进行中。其同时确认 artifact 含本机识别信息，只可本机留存、禁止提交、外发前需脱敏。
- 独立 QA `019f4eec-dfe0-74f0-8b1f-fcc6e0beae55` 首轮无新增 finding；返修复核要求将 artifact 规则统一为绝对禁止提交，修正后确认 finding 关闭。最终门禁为 FAIL/BLOCKED。Windows 名称已按 CIM/23H2/build 22631 修正为 Windows 11 家庭中文版。

## 2026-07-11 - HALCON Release 构建与安全启动

- HALCON 22.11.4.0 Windows x64 完整包下载到 `D:\download01`，精确大小 `6516267818` 字节，SHA-512 与官方值一致；离线仓库解压到 `D:\MVTecData\offline`。
- 通过包内 Software Manager 1.6.6 只安装 EULA、维护、Runtime、Development、HDevelop 和 VC++ 运行库，共 1.86 GB，目标 `D:\MVTec\HALCON-22.11-Steady`；未安装示例、HDevelop XL、Deep Learning、AI Accelerator 或接口包。安装器显示无 HALCON license。
- 首次 Release MSBuild 发现父目录和 process 本地的同内容 `paramStructs.h` 被同一编译单元引入，造成结构体重定义。两文件 SHA-256 相同；最小修复仅删除 `process/dllmain.cpp` 的冗余 include。
- 修复后 Release 定向构建 exit 0；正式 All 证据为 `artifacts/p1-windows-20260711-115033`，顶层 `partial`/exit 1，Debug 25.05 环境阻断，Release environment/MSBuild exit 0。
- manifest 记录 `CigVision.exe`、`process.dll` 及 SHA-256。CigVision 在 `rejectEnabled=false` 下成功显示主界面；`startup-window.png` 通过无交互窗口抓取生成，未点击任何应用控件。启动模块记录 Qt 5.9.9、HALCON 22.11.4.0、MVS 4.8.0.3 和 DAQNavi 4.1.22.0 已加载。
- 本轮 reviewer 发现证据未绑定 dirty 工作树及未捕获异常可能生成假 `passed`；脚本已记录 Git 状态和 `source-snapshot.diff` 哈希、增加 `unhandled-exception` failure，并以 `115033` 重新 Rebuild。QA 发现的旧 artifact 结论与 `process.dll` 加载措辞也已修正，等待同一 reviewer/QA 返修复核。
- HALCON 许可证、MVS 相机采集、DAQNavi 实际 IO、重复启停和剔除继续未验证，不随 P1 构建通过而升级声明。
- 同一 reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` 返修复核确认三项 finding resolved、reviewer gate PASS；同一 QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` 确认两项 finding 关闭、QA gate PASS。P1 提交门关闭，阶段指针切换到 P2 待开始；本轮未执行 P2 实现。

## 2026-07-11 - P2 数据契约与接口验收候选

- 新增 C++14 标准库 `core` 层：FramePacket 自持像素字节，Detection/DetectionBatch、InspectionResult、RejectCommand/RejectExecutionResult 带明确追踪、版本、错误和安全模式字段。
- 新增 IFrameSource、IDetector、IInspectionResultSink、IRejectOutput、IClock 可替换接口，以及有容量、溢出策略、丢弃计数、关闭和等待语义的线程安全 BoundedQueue。
- 当时的 P2 计划中，现有 picStruct、Halcon 图像队列和相机回调未替换，避免引入双路入队或越过后续阶段；未接 TensorRT、未增加检测消费者、未调用真实 IO。当前 P5-P8 路线以后文 2026-07-11 调整记录为准。
- 首次契约测试的 C++ Debug/Release 均 4/4 通过，但取证脚本在写 manifest 时因 PowerShell 5.1 中文路径字面量失败；改为动态定位 InspectionContracts.h 后重跑成功。
- 首轮正式契约证据 `artifacts/p2-contracts-20260711-120857`：Debug/Release MSBuild 和 test exit 0，Level4/WX，无 Qt/Halcon/MVS/DAQ/TensorRT 依赖。主程序回归 `artifacts/p2-main-regression-20260711-120926`：Release 环境检查和 Rebuild exit 0；原 P1 代码页/宏警告仍由 KI-028 跟踪。
- 独立 reviewer 首轮发现非法安全枚举可通过、队列拒绝提前消费 move 所有权、缺少真实并发/关闭测试及 artifact 未绑定脚本，gate 为 BLOCKED；独立 QA 技术 gate PASS，同时发现命令为空和脚本未哈希。
- 返修后正式证据更新为 `artifacts/p2-contracts-20260711-122211`：显式封闭 InspectionDecision/RejectMode，队列在接受后才复制/移动，并新增 move-only 拒绝、多等待者唤醒、关闭后排空和并发生产消费，Debug/Release 7/7 通过。manifest 记录确定命令和取证脚本自身 SHA-256，等待同一 reviewer/QA 复核。
- 同一 reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c` 确认四项 finding 全部 resolved、无新增 finding；同一 QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b` 复算 40/40 artifact 文件并独立连续运行两配置各 20 次，全部 7/7。两门均 PASS，AC-02-01/02 通过，P2 关闭。
- 阶段指针切换到 P3 待开始，本轮未实现离线回放、检测消费者、结果保存、UI 结果、TensorRT、相机适配或真实剔除。

## 2026-07-11 - P3 离线检测闭环验收候选

- 新增 C++14 `OfflineInspectionSession`：每次运行独立有界队列、图片源生产线程/检测消费线程、检测结果判定、统计不变量、显式停止和重复运行。
- 新增 Qt 图片列表 source、SHA-256 校验、深拷贝 FramePacket、`QSaveFile` 原子逐帧 JSON/PNG、汇总 JSON，以及独立 QThread worker。fixture detector 明确标注为非生产，只验证链路。
- 主程序新增 `--offline` 无硬件模式和 `--offline-batch-manifest/--offline-output` 批处理入口；前者跳过相机/IO 初始化，后者在构造主窗口前完成，不生成 RejectCommand，不触达真实 IO。
- 首次主工程编译发现 `QVariantMap` typedef 被错误前置声明，最小改为包含 Qt QVariant；Release Rebuild 随后通过。新适配器的非 ASCII 日志已改为英文，没有新增 C4819；旧 MyCamera/readIOTask 与 process 宏警告继续由 KI-028 跟踪。
- P3 取证脚本前两次分别发现 PowerShell 5 中文路径字面量和 stderr warning 误判问题；已动态定位 solution/config，并按子进程退出码判断。失败目录不作为正式证据。
- 首轮证据后 reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 判定 BLOCKED，指出停止竞态、worker 裸指针、协作者异常终止、不受控容量/停止排空和追踪字段不足；QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 限定 PASS 并要求补实际 UI 流程。
- 集中返修：state mutex 保证停止不丢失；QPointer 管理 worker；source/sink/archive/observer 异常转为统计/issue；固定容量 4 + 背压，UI 停止不排空；严格 manifest；输入清单与完整追踪 JSON。
- 返修正式证据 `artifacts/p3-offline-20260711-132348` 顶层 exit 0：Debug/Release 各 7/7；Release Rebuild exit 0；非法 manifest exit 2；8 图生成 input manifest、8 JSON、8 PNG、summary，processed=8、OK=4、NG=4、所有错误计数为 0。
- Computer Use 在 `--offline` 下实际选择一张固定原图和本地输出目录，预览显示，UI 为合格 1/缺陷 0/错误 0；`ui-qa-observation.json` 和三份输出文件哈希保存在返修证据目录。未初始化相机/DAQ，未调用剔除。
- `CigVision.exe --offline` 成功显示并响应，`startup/offline-window.png` 非空，未点击任何应用控件。该证据不声明 Halcon 许可算法、MVS 相机、DAQNavi IO、TensorRT、准确率或剔除通过。
- 当前为验收候选，等待独立 reviewer/QA；P4 尚未开始，未提交、未 push。
- 最终 reviewer 复核要求补 `source.start()` 异常边界和两处评审包陈旧文字；已新增 `ThrowingStartSource` 覆盖并以 `artifacts/p3-offline-20260711-132348` 最终重跑。
- reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 最终确认所有 findings resolved、gate PASS；QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 复算 37 项记录全部一致、gate PASS。
- P3 提交门关闭，阶段指针切换到 P4 待开始。本轮未接 TensorRT、未触碰真实相机/DAQ/剔除，未 commit、未 push。

## 已完成的历史工作

- 原始杂乱资料已整理为 `00` 至 `07` 和 `99` 目录。
- 缓存、重复内容和无必要构建产物已清理；安装包/运行依赖单独保留在本机目录。
- 本地 Git 初始化完成并推送到 GitHub 私有仓库。
- 大依赖包、超大图片压缩包和超大 PPT 已通过 `.gitignore` 排除普通 Git 历史。

历史条目只说明操作已经发生，不等于当前代码已通过构建或现场验收。

## 2026-07-11 - P4 TensorRT 正式集成验收候选

- 审计本机 RTX 4060 Laptop GPU、驱动 592.27、CUDA 13.1、TensorRT 10.15.1.29 和 OpenCV 4.9；模型本体确认输入 `images [1,3,992,992]`、输出 `output0 [1,300,6]`、9 个类别。
- 旧 `*_cache.engine` 由 TensorRT 8.6.1.6 生成，在 10.15 反序列化失败；非 cache `.engine` 无有效 plan tag。精确日志保存在 `artifacts/p4-audit-20260711-141545`。
- 由 `yanzhi20260120.onnx` 在本机生成 TRT 10.15 FP16/FP32 候选 engine；FP16 trtexec 随机输入基准 1391 查询、约 138.9 qps、host latency median 6.98 ms/p95 7.81 ms。该数字不是上位机端到端时延。
- 新增 TensorRT 10 `IDetector` 适配器、显式 detector JSON 配置和 `--tensorrt-batch-manifest` 离线入口；P3 fixture 入口保持不变。适配器校验 tensor 名称、模式、dtype、shape，检查 CUDA/TRT 返回，支持 Mono8/RGB8/BGR8 与 stride，串行保护共享 context。
- 首版自写双线性在模型 NMS 临界区域与 ONNX 参考差异偏大；改为 OpenCV 4.9 `INTER_LINEAR` 后，TensorRT FP32 与 ONNX 参考高度一致。12 图 FP16/FP32 检测数与类别顺序一致，最大置信度差约 0.02，因此选择 FP16 候选。
- 返修后正式命令 `scripts/run_windows_p4_tensorrt.ps1` exit 0，证据 `artifacts/p4-tensorrt-20260711-150510`：Release Rebuild 0、batch 0、缺失 engine/非法尺寸 exit 4、残缺/冲突 CLI exit 2；116 图全部处理，OK=27、NG=89、246 框、error/drop/save failure=0；116 JSON、116 原图、116 带框图，3 组重复输入结果一致；10 个 P4 源码/工程/脚本哈希已绑定且复算一致。
- 返修后 detector latency（mutex 等待、预处理、推理和后处理）：min 8.111 ms、median 33.593 ms、p95 95.783 ms、p99 112.084 ms、max 114.694 ms；不包含图片解码和保存。样本无人工 ground truth，以上 OK/NG 和类别分布不代表正确率。
- 视觉抽查 frame 1/3/6/12：框能落在明显破损/污点区域，但存在同一区域重叠框、超大框和文字遮挡；无标签条件下只记录为人工复核风险。
- P2 回归 `artifacts/p2-contracts-20260711-145147` Debug/Release 各 7/7；P3 回归 `artifacts/p3-offline-20260711-145147` 顶层 PASS。
- P4 实现阶段当时等待独立 reviewer 和 QA；其后续返修及最终结论见下两条。相机、DAQNavi、HALCON 许可、在线节拍、真实剔除均未验证或未触达。
- 独立 reviewer 首轮 FAIL 的 6 项已集中返修并逐项 resolved，技术 gate PASS；返修后独立 QA 在 `artifacts/p4-qa-independent-postfix-20260711` 最终 gate PASS，364 个证据文件和 10 个源码哈希全匹配。
- 一次 P3 回归 `artifacts/p3-offline-20260711-150846` 因与 QA 并发 Rebuild 默认输出目录发生 `process.dll` 竞争而失败；串行重跑 `artifacts/p3-offline-20260711-151220` 顶层 PASS。失败证据保留，不用于通过声明。
- P4 技术集成提交门关闭，阶段指针切换到 P5 待开始。本轮未开始后续实现；真实剔除继续禁止。准确率、中文业务映射和模型质量风险随后已按用户决定提前到 P5 本地标注与效果闭环处理。

## 2026-07-11 - 后续阶段改为全本地产品化路线

- 用户确认当前无法到现场，要求后续规划先在本地把商业项目做好：深度学习识别要有准确性证据、推理时间要短、效果要好，并允许优化 Qt 功能和 UI、清理无用功能、增加产品化能力。
- P5-P8 已重排为本地数据与算法效果闭环、本地实时流与模拟剔除、Qt 产品功能与 UI 重构、本地稳定性/部署/交付预验收。真实相机、DAQNavi、卷烟机同步和真实剔除移出当前排期并保持冻结。
- 老版 Qt/Halcon 传统算法只作业务规则和可选兜底参考；新版 Qt + TensorRT 深度学习继续作为唯一产品主线。功能移除必须先做清单、依赖分析和回归，不直接删除可能仍有业务价值的能力。
- documentation maintenance agent 随后完整检查并维护 README、AGENTS 与 `docs/*.md`：清除 README 停在 P1、P5/P6 现场路线、P7/P8 归属漂移等当前计划误导；P0-P4 历史证据保持不变。
- 验证：长期任务 `light_gate.py` 无 warning；全工作树 `git diff --check` exit 0；唯一阶段标记为 P5；旧路线扫描无命中；PowerShell 等价执行文档结构、关键 ID、未跟踪文件空白和 P5 受限目录检查通过。
- 已通过 `D:\git\Git\bin\bash.exe` 启动 `scripts/validate_project_docs.sh`，但脚本的未跟踪文件 `git diff --no-index --check` 在当前 Windows CRLF 工作树中会把换行转换提示或 CR 字节判为 whitespace，不能形成可信的原命令通过证据；已用 PowerShell 等价复核并记录 degraded validator compatibility。本轮文档维护未修改源码/脚本，P5 仍为待开始。
