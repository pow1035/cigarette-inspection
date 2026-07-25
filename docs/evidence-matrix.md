# 证据矩阵

证据状态：待收集、进行中、通过、失败、未验证、未请求/不声明。

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-00-01 | 项目入口可导航到记录系统 | `AGENTS.md`；`scripts/validate_project_docs.sh` 输出；独立 reviewer 结论 | 通过 | 仅证明入口与文件存在 |
| AC-00-02 | 需求被拆分并记录 | `docs/requirements.md`；独立 reviewer 结论 | 通过 | P5-P8 本地门槛仍需逐阶段冻结；现场参数已移出当前排期 |
| AC-00-03 | 架构边界和安全门被记录 | `docs/architecture.md`；独立 reviewer 结论 | 通过 | 尚未通过代码或运行验证 |
| AC-00-04 | 当前阶段唯一 | `docs/task-plan.md` 唯一 CURRENT_PHASE 标记（现为 P8 进行中）；验证脚本输出 | 通过 | 只覆盖阶段指针；P5/P6/P7 保持外部阻断记录 |
| AC-00-05 | 已知问题和验证边界可见且事实经复核 | `docs/known-issues.md`；`docs/code-audit.md`；三名 explorer 审计；独立 reviewer 首轮发现与最终复核 | 通过 | 只证明静态代码现状，不证明运行行为 |
| AC-00-06 | P0 有验证和独立评审 | `docs/review-packet.md`、`docs/review-results.md`；reviewer `019f49b6-62f7-7320-8f43-228c21bfca93` 最终复核；documentation maintenance `019f49b6-62b5-7511-9b1c-93f055de83bc` 复核 | 通过 | 不包含 Windows/硬件 QA |
| AC-00-07 | P0 未修改业务源码 | 增强后的 `scripts/validate_project_docs.sh` exit 0；限定业务目录的 `git status` 输出为空 | 通过 | P0 新增项仅为 `AGENTS.md`、`docs/`、`scripts/` |

| AC-01-01 | Windows 工具链清单可执行 | docs/windows-build-baseline.md；scripts/check_windows_build_env.ps1；`artifacts/p1-windows-20260711-115033/manifest.json`、`env-debug.log`、`env-release.log` | 通过 | Release 实际组合已由 MSBuild 和启动证明：VS2022 17.11.2、Qt 5.9.9、HALCON 22.11.4.0、MVS 4.8.0.3、DAQNavi 4.1.22.0；不等于硬件功能通过 |
| AC-01-02 | 目标 Windows 构建成功 | `artifacts/p1-windows-20260711-115033/full-terminal.log`、`msbuild-release.log`、`manifest.json`、`source-snapshot.diff`、`startup-metadata.json`、`startup-observation.json`、`startup-window-capture.json`、`startup-window.png`、`evidence-file-hashes.json`；reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0`；QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` | 通过 | Release environment/MSBuild exit 0，`CigVision.exe` 成功启动；`CigVision.exe`、`process.dll` 均已生成并记录 SHA-256，但启动模块列表未显示 `process.dll` 已加载；Debug 25.05 缺失，All 顶层为 `partial`/exit 1 |
| AC-01-03 | 映射、内存、帧元数据和编号快照问题已返修 | scripts/validate_p1_static.sh exit 0；源码 diff；独立 QA 返修复核 | 通过（静态） | 不证明相机运行；轻量回调留在 P2/P3 |
| AC-01-04 | P1 无剔除输出实现且默认关闭 | config.ini 的 rejectEnabled=false；新版工程无 DO 输出实现；P1 静态门 | 通过（静态） | 不是运行时硬件安全门，KI-024 继续追踪 |
| AC-01-05 | 采集生命周期有进程期回调守卫、双层屏障、故障锁定、IO 故障安全停止和清理路径 | CigVision/MyCamera/readIOTask diff；P1 静态门；独立 QA 返修复核 | 通过（静态） | 晚到帧污染、重复启停、Stop 失败和断连仍需 Windows/MVS QA |
| AC-01-06 | 工程依赖关系和父目录 include 已声明 | 三个 vcxproj 通过 XML 解析；process 的 `$(ProjectDir)..`；P1 静态门；Release Rebuild exit 0；独立 QA 返修复核 | 通过 | Release 已证明当前路径和构建依赖可用；Debug 仍因 25.05 缺失未进入 MSBuild |
| AC-01-07 | testWrite 不参与新版构建 | vcxproj/filters 无 testWrite；P1 静态门 | 通过（静态） | 历史文件仍保留在目录 |

| AC-02-01 | 四类核心数据契约可脱离 SDK 构造并测试 | `core/InspectionContracts.h`；`tests/CigVision.Contracts/ContractTests.cpp`；`artifacts/p2-contracts-20260711-122211/manifest.json`、两配置 test log、evidence-file-hashes；reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c`；QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b` | 通过 | Debug/Release 7/7，独立各连续 20 次通过；不证明相机、算法或离线闭环 |
| AC-02-02 | 采集、检测、结果存储和剔除输出具有可替换接口 | `core/InspectionInterfaces.h`；FakeFrameSource/FakeDetector/FakeResultSink/FakeRejectOutput；主程序回归 `artifacts/p2-main-regression-20260711-120926`；同一 reviewer/QA 最终 PASS | 通过 | 只证明接口可替换和主工程可构建；没有生产适配器、真实输出或 TensorRT |

| AC-03-01 | 固定离线图片完成读取、判定、结果/图像保存和统计 | `artifacts/p3-offline-20260711-132348/manifest.json`、`batch-output/summary.json`、`input-manifest.json`、8 个完整追踪 `frame-*.json`、8 个 `frame-*.png`；reviewer/QA 最终 PASS | 通过 | 检测器为非生产 fixture，不支持准确率声明 |
| AC-03-02 | 空输入、源/检测/协作者异常、保存失败、队列满、启动即停止和重复运行有确定行为 | 同目录 Debug/Release `test-offline-*.log`，各 7/7；非法 manifest exit 2；reviewer/QA 最终 PASS | 通过 | 真实磁盘满、长时压力和现场文件系统未验证 |
| AC-03-UI | 无硬件模式可选择图片、保存并刷新预览/统计 | `startup/startup-observation.json`、`startup/offline-window.png`；Computer Use；`ui-qa-observation.json` 与 `ui-qa-output`；最终 QA PASS | 通过 | 单图 UI 流程；未测试长批次 UI 手工停止 |

| AC-04-01 | TensorRT 10.x 检测器通过 JSON 配置 engine、张量名、992 输入、阈值、9 类映射和禁用类别 | `adapters/tensorrt/TensorRtDetector.*`；`artifacts/p4-tensorrt-20260711-150510/detector-config.json`、`manifest.json`；engine/尺寸 exit 4，残缺/冲突 CLI exit 2；reviewer 最终复核 | 通过 | 中文业务名、逐类阈值仍未确认，不影响适配器可配置性证明 |
| AC-04-02 | 116 图固定清单生成逐图结果、带框图、汇总分布和 detector latency | 同目录 `fixed-input-manifest.json`、`batch-output`、`effect-summary.json`；`artifacts/p4-audit-20260711-141545`；独立 QA `artifacts/p4-qa-independent-postfix-20260711` | 通过 | 116 文件含 113 个唯一哈希；无人工 ground truth，不声明准确率 |

| AC-05-01 | P5 数据清单、重复关系、来源组、类别目录和真值规则可审计 | 既有数据审计；原 pass1 SHA-256 `E097...E43`；晋级证据 `artifacts/p5-reviewed-truth-20260719-124537`：30 reviewed、20 comparable、10 REVIEW excluded、35 GT 框；reviewer/QA 最终 PASS | 进行中 | 30 图 pilot 已授权并冻结；完整 train/validation/test 划分、类别业务批准和代表性覆盖仍未完成 |
| AC-05-02 | 评估工具拒绝未复核、未授权、带预测 provenance 或缺哈希声明的数据，并可计算框级/烟支级指标 | `scripts/p5_dataset_tools.py`；`artifacts/p5-reviewed-truth-20260719-124537`；定向 8/8、P5 全量 75/75，正式 GT 与 predictions 校验 error 0 | 进行中 | pilot 真值已具备并已运行 CPU 诊断基线；完整测试集、类别覆盖和正式 TensorRT 绑定仍未完成，多类零支持 |
| AC-05-04 | 30 图试标集可确定性复现且不修改源图 | 正式 `artifacts/p5-pilot-20260711-192148`；reviewer `019f50b8-2d80-7e10-af4a-cdbd9b12cddc` PASS；独立 QA `019f50ef-db01-7cb0-b37a-f1af708b8cde`、`artifacts/p5-pilot-20260711-192926`：29/29、30 原图/预览/绑定，gate PASS | 通过 | QA 额外对抗检查未单独持久化，故不作数量声明；pilot 的授权与双人真值已由 P5-02C3 完成，完整数据集仍待覆盖 |
| AC-05-05 | localhost 首标工作台的身份、状态、保存和导出门可重复验证 | 既有工作台证据；原 pass1 保持 `is_ground_truth=false`；独立晋级脚本、attestation 与输出 manifest 位于 `artifacts/p5-reviewed-truth-20260719-124537`；reviewer/QA 最终 PASS | 通过（技术切片） | 工作台只保存单名；用户补充复核事实后通过独立哈希晋级，不修改旧 pass1 |
| AC-05-06 | 受控 P5 评估输入在运行前被存在性、manifest 内部 schema/大小/SHA-256、路径和 attestation 绑定检查，并要求外部 evidence-manifest 摘要核对 | `scripts/p5_input_readiness.py`；readiness 24/24、P5 100/100；strict exit 2；非目录祖先、case/NFC-NFD 等价别名、symlink loop 和双向 output-overlap 只读门；独立 reviewer/QA 最终 PASS | 进行中 | readiness 不是 manifest 签名认证；受控 reviewed-truth 与 fallback artifact 不在 Git clone，且 reviewed evidence-manifest 摘要尚未提供，恢复前不得开始指定范围 pilot |

P5-P8 已按用户决定重排为全本地阶段。P5 当前为外部阻断、未关闭：P5-02C3/C4/C5/C7 等局部切片已通过，但完整数据、正式 TensorRT、业务确认和受控 artifact 恢复尚未满足退出条件。P6/P7 的本地核心和源码已实现，Windows/Qt runtime 作为外部目标机阻断保留；当前阶段已推进到 P8。真实相机、DAQNavi 和真实剔除冻结且不在这些阶段声明内。

## P6 核心切片证据

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-06-01 | 文件/录制流按可配置节拍模拟多相机、编号、队列和异常 | `core/RealtimeSimulation.h`、`core/RealtimeLoadSimulation.h`、`QtOfflineInspection.*`、`scripts/p6_simulation_preflight.py`、`scripts/p6_windows_simulation_evidence.py`；simulation 15/15、P6 preflight 9/9、目标机驱动测试 8/8；逐帧 delay、camera/cigarette metadata、路径/哈希/元数据、乱序/重复/跳号、两种溢出策略、source/session/虚拟 stop 与 restart | 进行中 | SDK-free 模型、预检和可测试的证据编排已验证；实际 Qt manifest 解码/运行、Windows/MSVC 产物仍待目标机执行 |
| AC-06-02 | 模拟剔除由 frame_id 追踪到烟支编号、判定、时钟和配置且无真实 IO | `SimulationRejectObserver`、`SimulationRejectOutput`、`validateSimulationTrace`、`validateRealtimeSimulationSummary`、`scripts/p6_simulation_preflight.py`、`scripts/p6_windows_simulation_evidence.py`；负路径拒绝 Real command、零编号、clock overflow、早于计划时间的回执、命令烟支号/错误绑定篡改和未完成 trace；目标驱动还核对逐帧 JSON/PNG、summary、trace 和六类 CLI rejection；`simulation-trace.json` 原子写入源码 | 进行中 | 目标驱动的非 Windows 结果明确标为 test-only；产品入口 trace、Windows/MSVC 运行日志和现场参数仍未声明 |
| AC-06-03 | 固定负载矩阵可复现容量、丢弃和 P95 时延取舍 | 20 帧虚拟矩阵：10 µs 到达间隔、50 µs 处理；RejectNewest 容量 1/4/32 分别为 processed 5/8/20、dropped 15/12/0、max queue 1/4/16、P95 wait 50/200/720 µs、P95 end-to-end 101/251/771 µs；DropOldest 容量 2 为 processed 6、dropped 14、P95 60/111 µs | 进行中 | 数值只证明确定性模型与指标守恒，不代表真实图像解码、TensorRT、Qt 保存、生产吞吐或现场节拍 |

P6-02 综合验证（2026-07-25）：文档门（受控 P5 输入按预期 exit 2）、P1 静态门、P5 100/100、P6 preflight 9/9、C++17 contracts/offline/simulation 7/7 + 7/7 + 15/15、C++14/Clang simulation、ASan/UBSan、20 次重复、头文件自包含编译、Python 编译、simulation vcxproj XML、workflow YAML、`light_gate.py` 和 `git diff --check` 全部通过。simulation validator 与 preflight 还覆盖回执计划时间、命令烟支号、错误绑定、逐相机统计和 P95 时延统计篡改。该证据仍不包含 Qt/Windows runtime、GPU、真实相机或真实 IO。

P6 目标机证据编排（2026-07-25）：新增 `scripts/p6_windows_simulation_evidence.py` 与 `scripts/run_windows_p6_simulation.ps1`。Python 驱动先运行 manifest preflight，再执行 Simulation batch，核对 `input-manifest.json`、逐帧 JSON/PNG、`summary.json`、`simulation-trace.json` 和 trace preflight，随后运行缺失输出、互斥输出、队列/延迟/target 边界及残缺 manifest 六类 CLI 负路径；所有日志、命令、源码/输入/输出 SHA-256 写入新证据目录。伪目标进程测试 8/8（含输出篡改、`realIoEnabled` 篡改、错误退出码、`rejectEnabled=true`、已有输出根和非 Windows 标记）通过。当时 Mac 宿主机没有原生 PowerShell/Qt/MSVC，故没有生成目标机证据；2026-07-26 的 Colima/Linux PowerShell 静态门仍不生成 Windows runtime 证据。`--allow-non-windows-test` 结果只能标为 `passed-test-only`，不能关闭 AC-06 或替代 Windows runtime。

路线文档一致性证据（2026-07-25）：`README.md`、`AGENTS.md`、`docs/task-plan.md` 及相关记录系统；`light_gate.py` 无 warning；`git diff --check` exit 0；唯一阶段标记已推进为 P8；P5/P6/P7 外部阻断与本地边界均有记录；旧现场路线扫描无命中；PowerShell 等价文档结构检查通过。Git Bash 可用，但原脚本的未跟踪文件检查与当前 Windows CRLF 工作树不兼容，记录为 degraded validator compatibility。

## P7 产品状态与复核切片证据

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-07-01 | 本地产品运行状态可统一承载生命周期、参数身份、逐帧统计、复核、诊断和会话证据 | `core/Sha256.h`、`core/ProductParameterProfile.h`、`core/ProductRuntimeState.h`；`tests/CigVision.ProductState/ProductStateTests.cpp` 8/8；`docs/p7-parameter-profile.md`；Qt worker/UI/session 接线 | 进行中 | SDK-free 核心已运行；当前 UI 不初始化相机/DAQNavi；Qt/Windows 页面、JSON runtime 和 TensorRT/GPU 尚未运行验证 |
| AC-07-02 | 现有控件在变更前有消费者与保留决策，新增页面沿用当前视觉语言 | `docs/p7-ui-inventory.md`；`CigVision::initReviewView` 深色表格/按钮；本切片没有删除或隐藏旧控件 | 进行中 | 代码检查不证明实际布局、缩放、键鼠操作或风格一致性；仍需 Computer Use/人工 QA |

P7 参数身份增量（2026-07-25）：ProductState 已扩展到 8/8，并在 GCC C++14/C++17 strict 下通过；SHA-256 标准向量和 legacy profile golden `a973097f...2605a6` 固定。测试覆盖 configured/applied profile 区分、TensorRT adapter 投影、engine 路径不进入身份、阈值/类别/模型变化改变身份、run/frame 参数哈希漂移拒绝且无部分统计写入。detector 现在通过 `DetectionBatch` 回传实际 version/SHA，worker 原样传入状态层；独立 reviewer 已确认同源比较问题 resolved。当前 Mac 无 Qt/MSVC，因此不声明页面、JSON 或 TensorRT runtime PASS。

## P8 本地工具证据

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-08-01 | SDK-free soak 编排可记录多轮重启、超时/崩溃、RSS、磁盘和输出量 | `scripts/p8_soak_evidence.py`、`tests/p8/test_p8_soak.py` 9/9、`docs/p8-local-closure.md` | 进行中（本地子门通过） | Windows Toolhelp 首项/后续项错误与正常枚举结束已区分并用 mock 覆盖，目标机仍未实跑；GPU 必须是 not-collected |
| AC-08-02 | package 与现场主机报告可由外部 manifest SHA、challenge、逐文件哈希及严格 schema/同源身份绑定预检 | `scripts/p8_preflight.py`、`config/p8-local-gates-v1.json`、`tests/p8/test_p8_preflight.py` 17/17；HEAD `6886856` full gate | 进行中（本地子门通过） | v2 Windows/GPU 报告必须绑定本次 challenge、package manifest、同 capture/host/time/collector SHA、精确检查集全部 passed；ready 不等于产品语义验收 |
| AC-08-02 | package manifest 可由 package 完整文件集确定性生成且生成期间保持稳定 | `scripts/p8_package_manifest.py`、`tests/p8/test_p8_package_manifest.py` 7/7、`docs/windows-target-execution.md` | 进行中（本地子门通过） | 输出必须位于 package 外且不可覆盖；遍历错误、祖先 link/reparse、case/NFC 冲突及生成中变化 fail-closed；不证明 package 是产品级交付物 |
| AC-08-02 | fixture release 可验证并事务式激活/回滚 | `scripts/p8_release.py`、`tests/p8/test_p8_release.py` 17/17 | 进行中（本地子门通过） | 外部 package manifest 约束 staging 复制字节且 release 使用精确 schema；artifact kind 仍是 fixture，D 盘/Windows 产品包未验证 |
| AC-08-02 | Windows collector/wrapper 固定 D 盘并串联现场主机采集、预检、soak、fixture 发布和可选回滚 | `scripts/collect_windows_p8_host_reports.ps1`、`scripts/run_windows_p8_preacceptance.ps1`、`tests/p8/test_p8_windows_wrapper.py` 2/2；HEAD `6886856` full gate | 进行中（本地子门通过） | `Get-LockedFileSnapshot` 检查完整 reparse 祖先链并以 `FileShare.Read` 锁内计算 SHA；wrapper 对 provenance collector 持读锁贯穿执行、前后复算 SHA/复查 reparse。Windows/D 盘实跑仍缺失 |
| AC-08-02 | 仓库 PowerShell 脚本具有可重复的 parser/PSScriptAnalyzer/Windows PowerShell 5.1 兼容门和 CLI 成败契约 | `scripts/validate_powershell_scripts.ps1`、`tests/powershell/test_validate_powershell_scripts.ps1`；PowerShell 7.6.3 + PSScriptAnalyzer 1.25.0；`PSUseCompatibleSyntax` target 5.1；13 个 `.ps1` parser/analyzer 0 finding；成功、损坏语法、PowerShell 7 三元语法拒绝、缺根、缺/空 `scripts/` 目录、缺 analyzer 共 7/7 CLI 契约 PASS；exit 2/3 为 JSON | 进行中（本地静态子门通过） | Colima/Linux arm64；`windowsHost=false`、`windowsRuntimeClaimed=false`；独立 reviewer/QA 只覆盖该静态门；不证明 Windows/Qt/GPU/D 盘/硬件，且不并入 P8 76/76 |
| AC-08-03 | Windows evidence v4 可移植验真并原子导入 | `scripts/p8_windows_evidence_verify.py`、`tests/p8/test_p8_windows_evidence_verify.py` 24/24、`docs/windows-target-execution.md`、HEAD `6886856` full gate | 进行中（本地子门通过） | 外部 manifest SHA + HMAC + exactly 32-byte key 共同认证 manifest；key 隔离、固定 bytes、精确 preflight/host/provenance 复验保持不变；receipt v4 保持 `productAcceptanceClaimed=false` |

主代理定向复核为 32/32。

HEAD `6886856` 最终 full gate（2026-07-26）PASS：P5 100/100、P6 17/17、P8 76/76（preflight 17、soak 9、release 17、wrapper/collector 2、package manifest 7、evidence verify/import 24）、C++14/C++17、20 次重复、ASan/UBSan；PowerShell 13 个脚本 parser/analyzer 0 finding、CLI 7/7。该结论不覆盖目标 Windows/Qt/GPU/SDK/D 盘、真实数据、许可或硬件；P5 受控 artifact 仍缺失。

最终独立复核（2026-07-26）：reviewer `019f9a55-8131-7423-96a7-44d934c28255` 与 QA `019f9a55-99a2-7011-887f-119893e3ec4f` 对当前 v2/v4/HMAC/文件锁增量均给出 PASS，P0/P1/P2/P3=0/0/0/0。PowerShell 静态门在 Colima/Linux arm64 使用 PowerShell 7.6.3 与 PSScriptAnalyzer 1.25.0 扫描 13 个 `.ps1`，parser/analyzer finding 0，CLI 7/7。提交 `7e7c2de2b157cf5c2ace8b5263e8db5f57c89902` 已 push；2026-07-26（Asia/Shanghai）的在线 Actions `Local gates` 运行 `30169095184`（job `89706968923`）SUCCESS，全部步骤通过、check-run annotations 为空，原 Node 20 warning 已消失，checkout/setup-python v6 均固定完整提交 SHA。该 CI 硬化切片的最终 reviewer `019f9a6b-f76e-7620-a9e4-1e681e7f8d0b` PASS（P0/P1/P2/P3=0/0/0/0，对抗 18/18），QA `019f9a6c-0733-7da1-986f-6176824be92d` PASS（P0/P1/P2/P3=0/0/0/0，对抗 17/17）。首次运行发现的全局 analyzer 隔离差异已改用显式 `ScriptAnalyzerModulePath` 并本地复验；真实 Windows/Qt/GPU/SDK/D 盘、数据、许可和硬件仍不在该在线证据范围内。

## P5-02C5 provisional fallback pilot baseline (2026-07-19)

- Status: implementation, local verification, independent review, and QA PASS; formal TensorRT remains blocked.
- Artifact: `artifacts/p5-fallback-baseline-20260719-161114/` (local evidence only; never commit).
- Classification: ONNX Runtime 1.20.1 CPU fallback, **not** formal P4 TensorRT evidence.
- Frozen inputs: approved 30-image pilot, reviewed GT, model SHA-256 `956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4`, confidence threshold 0.25, IoU 0.50. No training or threshold/NMS tuning occurred.
- Evaluation population: 20 comparable images; 10 REVIEW images excluded.
- Box-level micro: TP=13, FP=24, FN=22, precision=0.351351, recall=0.371429, F1=0.361111.
- Cigarette-level: 10 GT NG / 10 GT OK; missed-NG=3 (0.30), false-NG=3 (0.30).
- Regression: `python -m unittest discover -s tests/p5 -p "test_*.py"` -> 76/76 PASS.
- TensorRT remains blocked: all available engines fail TensorRT 8.6.1 deserialization; rebuilding from ONNX fails because the `Mod` node plugin is unavailable.
- This pilot result is diagnostic only and is not a product-acceptance or commercial-performance claim.

## P5-02C5 final independent review (2026-07-19)

- Initial reviewer: FAIL because `runtime_contract` was hash-bound but not semantically validated.
- Repair: strict schema/backend/provider/version/scope validation; model and predictions cross-hash checks; source runtime manifest hash check; detector-config identity check; explicit report classification; negative tests.
- Independent reviewer after repair: PASS.
- Independent QA: PASS, limited to provisional ONNX Runtime CPU fallback baseline.
- Regression after repair: 76/76 PASS; `git diff --check` PASS (line-ending warnings only).
- Evidence report SHA-256: `2bc9b7115ba67563308fcb70fe3c4435c8849b89346962b8148d6487917b24d3`.
- Evidence manifest SHA-256: `9a9fd67471413eebcd8cd57179a8048bf7e868f313035921459046b6735968d6`.
- Formal TensorRT baseline remains BLOCKED / NOT VERIFIED under KI-039.

## P5-02C6 tooling portability and gate hardening (2026-07-25)

- Status: local implementation, independent reviewer, and independent QA PASS.
- Scope: FreeType-independent Pillow bitmap fallback; exact `Pillow==11.3.0` declaration; duplicate known-issue/current-status/document-drift checks; Linux `P5 local gates` workflow; unique P5-02C4/P5-02C5 identifiers.
- Local gates: `validate_project_docs.sh` PASS; `validate_p1_static.sh` PASS; P5 `76/76`; `py_compile` PASS; SDK-free C++ contracts/offline `7/7 + 7/7`; `git diff --check` PASS; `light_gate.py` no warning; YAML and equivalent workflow-structure checks PASS.
- Independent reviewer rebuilt a clean snapshot from `git archive HEAD` plus the final diff and reproduced the same gates with no P0/P1/P2 finding. Independent QA reproduced the full visual-pack failure path with both TrueType factories blocked and confirmed all outputs were generated; a fresh Python 3.11 environment installed from `requirements-p5.txt` and passed `76/76` with `pip check` clean.
- Non-blocking limits: actionlint was not independently available in this snapshot; the hosted Ubuntu runner was not executed. Pillow versions older than the pinned 11.3.0 may not expose the legacy bitmap loader; Windows/MSVC, TensorRT/GPU, and hardware remain separate gates.
