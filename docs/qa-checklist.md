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

## P5-01 数据与评估工具

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 116 图审计、重复和尺寸 | 通过（工具切片） | `artifacts/p5-data-20260711-170640`：116/113、3 重复组、60 张 992x300、56 张 1200x600 |
| 预标注不是真值 | 通过 | 113 图/236 框全部 `is_ground_truth=false`；99 个未确认 `wuzi` 框不进入正式真值 |
| 未复核/未授权/预测 provenance/缺 attestation 拒绝 | 通过 | 正式 20/20；reviewer 攻击复算；QA 对抗 12/12 |
| split、area、类别目录与证据哈希 | 通过（工具能力） | manifest/image 一致性；8 项证据 binding；源码运行前后 7/7 稳定 |
| 混淆矩阵、macro、烟支级指标 | 通过（合成测试） | 同类匹配守恒、背景 FP/FN、active-union macro、missed-NG/false-NG |
| 实际准确率 | 仅 pilot CPU 诊断；商业效果未验证 | 30 图 approved/reviewed pilot 中 20 张进入临时 CPU 指标；正式 TensorRT、完整冻结 test split、类别覆盖和代表性不足，不能外推商业准确率 |
| 独立 reviewer | 通过 | `019f505a-151e-79a3-8384-fadca72c4e4d`；首轮 8 项 resolved |
| 独立 QA | 通过 | `artifacts/p5-qa-independent-20260711-170802`；20/20、对抗 12/12、证据门 12/12 |

## P5-02 试标集与复核包

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 确定性选样与非法输入 | 通过（返修自查） | P5 单测 29/29；相同输入一致；非法 size、不完整/真值输入、既有 split、伪造 source_group、重复 canonical hash、悬空 alias、catalog/COCO/P4 布尔 ID 均拒绝 |
| canonical、重复与 split | 通过（实现自查） | 30 canonical；1 个重复别名仅继承 pilot，不复制、不计入 30 |
| 来源/尺寸/判定/类别覆盖 | 通过（实现自查） | `pilot-selection.json` uncovered_features=[]；完整计数见证据 manifest |
| 原图只读与复核包完整性 | 通过（返修自查） | 116 源图与 6 个源码/输入哈希稳定；30 原图+30 预览+COCO+manifest+CSV；30 个 preview 源/副本绑定 |
| P4 预览与 COCO 语义一致 | 通过（返修自查） | 30 份 frame JSON 的尺寸、原预测判定、缺陷数量、类别、bbox、置信度和 detector version 全匹配 |
| 未确认类别和真值安全门 | 通过（实现自查） | 11 图强制 REVIEW；全部 `is_ground_truth=false`；无准确率声明 |
| 独立 reviewer | 通过 | `019f50b8-2d80-7e10-af4a-cdbd9b12cddc`；全部 finding resolved，`192148` 最终 gate PASS |
| 独立 QA | 通过 | `019f50ef-db01-7cb0-b37a-f1af708b8cde`；`artifacts/p5-pilot-20260711-192926`；29/29，fresh manifest passed，gate PASS；额外对抗计数未单独落盘故不声明数量 |
| 人工双人标注 | pilot 已通过；完整数据集未开始 | `artifacts/p5-reviewed-truth-20260719-124537`：30 图由责任人员标注、不同复核人逐页检查并获项目负责人批准；完整数据集仍需授权、业务类别确认和代表性覆盖 |

## P5-02C7 受控输入就绪

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 模型与类别目录存在且哈希可核对 | 通过 | `scripts/p5_input_readiness.py` strict run；当前模型 SHA-256 与目录文件均可读 |
| reviewed-truth/fallback manifest、必需输出、大小和 SHA-256 | 通过（工具能力） | `tests/p5/test_p5_input_readiness.py`；缺失、篡改、attestation catalog 错配和非法输出名均有负路径 |
| 受控证据路径不通过符号链接/路径逃逸或文件系统等价别名 | 通过（工具能力） | readiness 定向测试覆盖共同根、非目录祖先、case/NFC-NFD、symlink loop 和 artifact 根双向 overlap；只读检查，不写入 artifact |
| fresh clone 指定范围 pilot 输入 | 阻断（外部输入缺失） | `python3 scripts/p5_input_readiness.py --require-reviewed --require-fallback`：exit 2，报告 reviewed-truth/fallback 目录缺失 |
| 独立 reviewer | 通过 | `/root/p5_independent_review`：复核 exit 分类、固定类别目录哈希、attestation、路径/共同根/symlink、case/Unicode 等价别名、双向 output overlap 和文档门 wiring；最终 24/24、100/100 及全门禁 PASS |
| 独立 QA | 通过 | `/root/p5_independent_qa`：最终 24/24 targeted、100/100 full、strict fresh exit 2；output 只读契约、actionlint/workflow、artifact 前后快照和无测试副作用均 PASS |

## P6-01A 核心回放与模拟剔除

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 逐帧节拍与元数据 | 通过（实现自查） | simulation tests：delay、station/camera/cigarette metadata、空序列、非法输入 |
| 停止取消与重复编号 | 通过（实现自查） | 阻塞 pacer 被 stop 唤醒；duplicate frame-id 进入 sourceErrors 且统计守恒 |
| Simulation-only 剔除与回执 | 通过（实现自查） | 4 帧端到端仅 frame 2/4 生成 Simulation 命令；scheduledAt、烟支编号和回执可追踪 |
| 非法/危险输出负路径 | 通过（实现自查） | frame-id mismatch、invalid result、zero number、clock overflow、Executed status、output exception 均失败且无真实 IO |
| Qt/Windows/容量曲线 | 部分通过 | P6-02 SDK-free 容量模型已运行；P6-01B 目标机 runtime 仍未验证，当前不声明产品入口、MSVC、GPU 或现场行为 |

## P6-01B Qt manifest 与 Simulation CLI

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| manifest 可选 station/camera/cigarette/delay 字段与旧默认值 | 已实现，Qt runtime 未验证 | `QtOfflineInspection.cpp`；缺失字段默认 `offline`/文件名/顺序编号/0 delay，显式值类型与范围校验（本地 delay ≤60 s）；`scripts/p6_simulation_preflight.py` 只读复核路径、哈希和元数据 |
| simulation CLI 互斥与非法数值拒绝 | 通过（SDK-free） | `core/BatchCommandLine.h`；`SimulationTests.cpp` CLI 2/2，覆盖冲突、缺值、负/溢出 delay、队列边界、空 target、重复 option |
| SteadyReplayPacer 与 stop 取消 | 通过（核心/源码） | `RealtimeSimulation.h`、`QtOfflineInspection.cpp`；P6-01A source/session stop 测试包含在 simulation 15/15 中；核心回放入口统一拒绝超过 60 秒的单帧延迟 |
| 只构造 SimulationRejectOutput、无真实 IO | 通过（代码审查） | `runSimulationBatchManifest` 运行图；`realIoEnabled=false` trace 字段；未接相机/DAQNavi |
| 原子 simulation-trace.json 字段完整性 | 已实现，runtime 未验证 | `writeSimulationTrace` + `validateSimulationTrace` + `QSaveFile`；配置、frame/cigarette、observed/scheduled/completed clock、command/receipt/error |
| 目标机运行前 manifest/trace 预检 | SDK-free 通过，目标机 runtime 未验证 | `tests/p6/test_p6_simulation_preflight.py` 9/9；覆盖缺失、哈希/路径/符号链接/元数据、Real command、早回执、错误绑定和输出覆盖拒绝 |
| 目标机证据驱动与负路径编排 | 通过（非 Windows 测试替身；不算目标机 QA） | `scripts/p6_windows_simulation_evidence.py`、`tests/p6/test_p6_windows_simulation_evidence.py` 8/8；成功产物绑定、输出篡改、Real IO 标记、错误退出码、rejectEnabled、已有输出根和非 Windows test-only 标签均有检查 |
| Windows/Qt 实际 manifest 与 trace 运行 | 未验证 | `scripts/run_windows_p6_simulation.ps1` 已固化 Release 构建、依赖路径、原始日志和证据目录流程；仍需要目标 Windows/Qt 构建、运行输出和原始日志，不能由本机测试替代 |
| 独立 reviewer / QA | 降级（历史切片未单独完成） | 时间线核对：P6-01B 当时只完成同代理复核；后续 P7/P8 独立门没有明确把该源码切片列入范围，不能反向升级为独立 PASS。该项是历史证据边界，不是“已有结论尚未同步”；目标 Windows/Qt runtime 也仍未验证 |

## P6-02 多相机、异常与容量模型

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 多相机与编号异常 | 通过（SDK-free） | 两相机交错输入；arrival/frame/cigarette 的乱序、重复和前向跳号均有独立计数与逐帧 disposition |
| 队列容量与丢帧策略 | 通过（SDK-free） | RejectNewest 容量 1/4/32 的 dropped 为 15/12/0；DropOldest 容量 2 为 dropped 14，逐帧可区分 newest/oldest drop |
| 时延与积压 | 通过（虚拟时钟） | 固定 20 帧矩阵记录 max queue、P95 queue wait、P95 end-to-end；严格说明不等于真实 TensorRT/Qt/现场时延 |
| stop、drain、cancel、restart | 通过（SDK-free） | stop boundary 分别验证 drain/cancel；同一 simulator 实例随后重跑 2 帧且状态/计数从零开始 |
| Simulation-only 安全门 | 通过（SDK-free） | NG 只生成 Simulation command；零烟支号、schedule/processing overflow、早于计划时间的回执和命令烟支号篡改均拒绝；validator 重算计数、逐相机守恒、时延和序列账目 |
| 编译与动态检查 | 通过（本机） | C++14/C++17 严格警告；simulation 15/15；连续 20/20；ASan/UBSan 15/15 |
| Qt/Windows 录制流与原始运行产物 | 未验证 | 容量模型尚未接入目标 Windows/Qt 录制流；不关闭 AC-06，不声明产品吞吐或生产容量 |
| 独立 reviewer / QA | 降级、未验证 | 当前会话按上层约束未启动新代理；只完成同代理实现审查和 QA 式运行，后续独立复核仍是提交门缺口 |

## P7-01A 产品状态与最近结果复核

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 运行身份与真实 IO 安全门 | 通过（SDK-free） | 产品状态 8/8；configured/applied typed profile、canonical/golden SHA-256 和帧级 hash 绑定；TensorRT 模式要求模型 SHA-256，`realIoEnabled=true` 固定拒绝 |
| 生命周期与异常输入 | 通过（SDK-free） | start/stop/fault/restart；启动前停止可被下一次 run 消费；非法 NG、参数漂移、窗口内重复 frame 和停止后写入均拒绝且统计不变 |
| 统计与容量边界 | 通过（SDK-free） | OK/NG/error、逐相机、逐类别、elapsed/max queue 守恒；复合 station/camera 身份无分隔符碰撞；最近结果、诊断和 duplicate window 有界 |
| 结果复核 | 通过（SDK-free） | 具名 Confirmed/Corrected/Dismissed、修订计数、缺失复核人和非法 outcome/severity 枚举负路径 |
| 并发 | 通过（SDK-free） | 4 worker、400 帧，统计 400=200 OK+200 NG，最近结果固定 64 |
| Qt worker 与页面接线 | 已实现，runtime 未验证 | worker 发送 frame/station/camera/cigarette/defect/error；运行页刷新快照；`btn_search` 进入深色复核页 |
| 统计/诊断/会话证据 | 已实现，runtime 未验证 | `btn_count`、`btn_log`、原子 `product-session.json`；仅在启动、结束、复核时落盘；统一使用同一快照 |
| 配置冻结与退出 | 源码已实现，runtime 未验证 | 完整 configured/applied profile 与 SHA；legacy UI 阈值可审计但 fixture 明确未应用；TensorRT v2 engine/profile 绑定；品牌冻结和安全退出 |
| 修复后独立门 | 通过（SDK-free/静态） | reviewer 与 QA 均 PASS；C++14/C++17 strict、TSan、ASan/UBSan、重复压力、BarrierSource stop 探针、文档门和 diff 通过；未发现新增 P0-P2 |
| Windows UI/Computer Use | 未验证 | 需目标机验证布局、选中行、三种复核按钮、重复启停和页面切换 |

## P8 本地稳定性与部署工具

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| package/主机报告 preflight | 通过（本地工具） | 17/17；HEAD `6886856` 最终 full gate 覆盖，验收声明仍全部为 false |
| package manifest generator | 通过（本地工具） | 7/7；完整文件集、确定性排序、双重稳定性复扫、遍历 fail-closed、祖先 link/reparse、case/NFC 和不可覆盖输出 |
| SDK-free soak | 通过（本地工具） | 9/9；多轮新目录、超时/崩溃、RSS/磁盘/输出、Windows Toolhelp 子进程与 API 失败门 |
| fixture release | 通过（本地工具） | 17/17；source manifest 绑定复制、单快照 manifest、原子 activate/rollback、失败保持 current/shared |
| Windows wrapper/collector | 通过（本地静态契约） | 2/2；collector `Get-LockedFileSnapshot` 检查完整 reparse 祖先链、以 `FileShare.Read` 锁内计算 SHA；wrapper 持 provenance collector 读锁贯穿执行并前后复算 SHA/复查 reparse |
| PowerShell parser/PSScriptAnalyzer/5.1 兼容门 | 通过（本机静态） | Colima/Linux arm64，PowerShell 7.6.3、PSScriptAnalyzer 1.25.0；当前 13 个 `.ps1` parser/analyzer finding 0；`PSUseCompatibleSyntax` target 5.1；7/7 CLI 契约 PASS；`windowsRuntimeClaimed=false`，不并入 P8 的 81 项 Python 清单 |
| 跨机 evidence verify/import | 通过（本地工具） | 24/24；HEAD `6886856` 最终 full gate 覆盖；带外 SHA/HMAC/key 与 `productAcceptanceClaimed=false` 边界不变 |
| P8 v2 Python 防假绿语义 | 通过（当前本地候选） | 连续专项 5/5、P8 精确 81/81；覆盖受控 project build、外部 provenance 仅限 helper、伪造 project provenance 拒绝、verified duration/真实 process lifetime、Linux `/proc` CPU、pure-sleep CPU 负路径、16/64 MiB RSS、严格 inventory 与 Windows 双枚举失败/成功分支 |
| `contract-test-v1` C++ 短语义门 | 本地/独立复核通过；hosted 待复核 | hosted run `30219159920` 在 Linux 将 0.12 秒实际运行的 CPU 量化为 0.00 秒。当前候选让 wrapper/direct project contract 实际运行 1.0 秒，locked minimum 0.12 秒、timeout 2.0 秒和其他阈值不变；Mac/Colima/Linux full 与独立 reviewer PASS |
| `local-sdkfree-v1` 正式连续运行 | 通过（本地连续工具证据） | `artifacts/p8-continuous-local-20260727-final-v2`：两轮 300.046591/300.020852 秒，291/292 samples，progress 3786/3623、gap 0.085584/0.098617 秒，sessions 3786/3623，frames 1,938,432/1,854,976，CPU 30.64/23.51 秒，RSS +16 KiB/0；Offline/ProductRuntimeState 分桶一致、Error 0、无残留。self-verify、主代理独立 verify、reviewer 两次独立 verify PASS |
| 当前树 core/full | 通过（本地） | Mac 与 Colima Docker/Linux arm64、`CLK_TCK=100` 均直接执行原始 `./scripts/run_all_local_gates.sh --full` exit 0：P5 100、P6 17、P8 81、C++17/C++14、repeat20、ASan/UBSan 全 PASS |
| hosted Linux `Local gates` | 返修待复核 | `5f6a06a` → run `30219159920` / job `89838475434` FAIL，两项失败均为有效发现；当前修复 reviewer 与 QA/observability PASS，尚未提交/push，第二次 hosted rerun pending |
| P8 v2 独立 reviewer | 通过 | `/root/p8_v2_reviewer` 对当前 CI 修复和正式 `final-v2` evidence 最终 PASS，P0/P1/P2/P3=0/0/0/0；Linux 原始 full、两次独立 verify、29 个 inventory 文件、8 个源码、compiler/executable、新旧工具与 `/bin/ps` identity 均匹配 |
| P8 v2 独立 QA / observability | 通过 | `/root/p8_gate_docs_audit`：正式 evidence 两轮时长、CPU、RSS、磁盘、progress、业务守恒、无残留和 29-file strict inventory 均 PASS；当前 CI 修复文档复验也 PASS，P0/P1/P2/P3=0/0/0/0 |
| P8 v2 定向 cleanup | 通过（1 个非阻断 P3） | 正式 evidence 根无 tmp、symlink、FIFO、socket、漏列文件或残留进程。旧 `artifacts/p8-continuous-local-20260727-invalid-dependency-gap` 无 manifest、被 Git 忽略，只隔离保留作失败审计，不引用、不混入 `final-v2`、不交付；删除按用户/留存策略另行执行 |
| 早期 39 项独立 reviewer / QA | 通过（历史本地范围） | reviewer `019f99b4-2e4c-7742-9d20-60a0396d7900`、QA `019f99b4-4746-7d72-b5b6-568af6d8dfdd`；最终 P0/P1/P2=0/0/0，不覆盖 evidence v2 |
| 历史 70 项独立 reviewer | 通过（历史本地范围） | `019f99cb-7c02-7b23-b498-9b28e7ba761c`：初审及追加 findings 全部关闭，P0/P1/P2=0/0/0；不覆盖当前 v2/v4/HMAC 返修 |
| 历史 70 项独立 QA | 通过（历史本地范围） | `019f99d7-8a02-7da1-8641-2d533409d06d`：可信仓库 verifier 修复后 PASS，P0/P1/P2=0/0/0；不覆盖当前 v2/v4/HMAC 返修 |
| 历史 P8 v1 测试集 | 通过（历史本地工具） | 76/76：preflight 17、soak 9、release 17、wrapper/collector 2、package manifest 7、evidence verify/import 24；当前清单已扩展为 81 项，最新统一门待重跑 |
| v2/v4/HMAC/文件锁返修 | 通过（最终本地门） | HEAD `6886856` full gate PASS；最终 reviewer `019f9a55-8131-7423-96a7-44d934c28255` 与 QA `019f9a55-99a2-7011-887f-119893e3ec4f` 均 PASS，P0/P1/P2/P3=0/0/0/0；不据此声明 Windows runtime 或产品验收 |
| PowerShell 增量独立 reviewer | 通过（本机静态范围） | `019f9a17-7e58-7350-9ec3-7373f3151f4f` 在两项 P3 和一项文档计数 P2 修复后最终 PASS，P0/P1/P2/P3=0/0/0/0；确认 12 脚本零 finding、7/7 CLI、exit 2/3 JSON、5.1 语法拒绝及文档防回退 |
| PowerShell 增量独立 QA | 通过（本机静态范围） | `019f9a17-98ad-7f23-9c36-6d62a7fa49ec` 最终 PASS，P0/P1/P2=0/0/0；12 脚本零 finding、7/7 CLI、exit 2/3 JSON、5.1 兼容拒绝、文件哈希不变 |
| 历史最终 full local gate | 通过（历史快照） | HEAD `6886856`：P5 100、P6 17、P8 76、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 脚本零 finding、CLI 7/7；不覆盖 P8 v2 |
| Windows 目标机执行与跨机交接 | 手册已返修，runtime 未验证 | `docs/windows-target-execution.md` 固化 package manifest、外置 32-byte key、v4 wrapper 现场采集、带外 SHA/HMAC、拷回、verify/import 和声明边界 |
| Windows/PowerShell/D 盘/GPU 长稳 | 未验证 | 本机无目标环境；不得把 `passed-local-tooling` 外推为 AC-08 产品通过 |

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

## P5-02C1 localhost 首标工作台

| 检查 | 状态 | 证据 |
| --- | --- | --- |
| 30 图 package 加载和身份绑定 | 通过（实现/Browser 自查） | `/api/state`、Browser DOM、`state-after-browser-qa.json` |
| 保存、revision 和重载恢复 | 通过（实现/Browser 自查） | QA workspace revision=1；1 张 OK 重载保持 |
| 未完成、未确认类别和非法状态拒绝 | 通过（三轮返修自查） | 48/48；Browser 剩余 29 张导出拒绝；GT/类别/package/preview/运行期替换/署名/provenance 均有对抗测试 |
| 模型预览只读和三栏布局 | 通过（二轮返修 Browser 自查） | select/draw/delete/category 均禁用；预览键盘 Delete 框数 4→4；console 0 warning/error |
| 首轮/真值 provenance 门 | 通过（实现自查） | pass1 为 `annotated`/false；reviewed endpoint 固定 409 |
| 独立 reviewer | 通过 | `019f4fd0-7aa5-7af3-b82b-7b7ebb14ed55` 最终复现 48/48、65/65、20/20，无新 blocker；gate PASS |
| 独立 QA | 通过 | `019f4fd0-8ec7-7801-a493-e2d6797aea23` 最终独立复跑运行期替换/Host/证据；gate PASS |
| 责任人员人工首标/复核 | pilot 已完成；不外推到完整数据集 | P5-02C2/C3 正式证据与独立门禁；Codex QA 草稿不计人工结果，完整数据集仍待补齐 |
