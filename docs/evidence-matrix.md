# 证据矩阵

证据状态：待收集、进行中、通过、失败、未验证、未请求/不声明。

| 验收 ID | 声明 | 证据 | 状态 | 限制 |
| --- | --- | --- | --- | --- |
| AC-00-01 | 项目入口可导航到记录系统 | `AGENTS.md`；`scripts/validate_project_docs.sh` 输出；独立 reviewer 结论 | 通过 | 仅证明入口与文件存在 |
| AC-00-02 | 需求被拆分并记录 | `docs/requirements.md`；独立 reviewer 结论 | 通过 | P5-P8 本地门槛仍需逐阶段冻结；现场参数已移出当前排期 |
| AC-00-03 | 架构边界和安全门被记录 | `docs/architecture.md`；独立 reviewer 结论 | 通过 | 尚未通过代码或运行验证 |
| AC-00-04 | 当前阶段唯一 | `docs/task-plan.md` 唯一 CURRENT_PHASE 标记（现为 P5 进行中）；验证脚本输出；独立 reviewer 结论 | 通过 | 只覆盖阶段指针，不代表 P5 已完成 |
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
| AC-05-02 | 评估工具拒绝未复核、未授权、带预测 provenance 或缺哈希声明的数据，并可计算框级/烟支级指标 | `scripts/p5_dataset_tools.py`；`artifacts/p5-reviewed-truth-20260719-124537`；定向 8/8、P5 全量 75/75，正式 GT 与 predictions 校验 error 0 | 进行中 | 真值已具备；下一步须精确绑定 P4 模型、engine、detector config 后运行 pilot 基线，10 REVIEW 排除且多类零支持 |
| AC-05-04 | 30 图试标集可确定性复现且不修改源图 | 正式 `artifacts/p5-pilot-20260711-192148`；reviewer `019f50b8-2d80-7e10-af4a-cdbd9b12cddc` PASS；独立 QA `019f50ef-db01-7cb0-b37a-f1af708b8cde`、`artifacts/p5-pilot-20260711-192926`：29/29、30 原图/预览/绑定，gate PASS | 通过 | QA 额外对抗检查未单独持久化，故不作数量声明；11 图 REVIEW，授权/双人真值/准确率仍未验证 |
| AC-05-05 | localhost 首标工作台的身份、状态、保存和导出门可重复验证 | 既有工作台证据；原 pass1 保持 `is_ground_truth=false`；独立晋级脚本、attestation 与输出 manifest 位于 `artifacts/p5-reviewed-truth-20260719-124537`；reviewer/QA 最终 PASS | 通过（技术切片） | 工作台只保存单名；用户补充复核事实后通过独立哈希晋级，不修改旧 pass1 |

P5-P8 已按用户决定重排为全本地阶段，当前均保持未验证：P5 需要标注/指标/优化对照，P6 需要本地实时流与模拟剔除证据，P7 需要沿用现有风格的 Qt UI/功能运行证据，P8 需要稳定性、性能和部署预验收证据。真实相机、DAQNavi 和真实剔除冻结且不在这些阶段声明内。

路线文档一致性证据（2026-07-11）：`README.md`、`AGENTS.md`、`docs/task-plan.md` 及相关记录系统；`light_gate.py` 无 warning；`git diff --check` exit 0；唯一阶段标记为 P5；旧 P5/P6 现场路线扫描无命中；PowerShell 等价文档结构检查通过。Git Bash 可用，但原脚本的未跟踪文件检查与当前 Windows CRLF 工作树不兼容，记录为 degraded validator compatibility。

## P5-02C4 provisional fallback pilot baseline (2026-07-19)

- Status: implementation and local verification PASS; independent review/QA pending.
- Artifact: `artifacts/p5-fallback-baseline-20260719-161114/` (local evidence only; never commit).
- Classification: ONNX Runtime 1.20.1 CPU fallback, **not** formal P4 TensorRT evidence.
- Frozen inputs: approved 30-image pilot, reviewed GT, model SHA-256 `956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4`, confidence threshold 0.25, IoU 0.50. No training or threshold/NMS tuning occurred.
- Evaluation population: 20 comparable images; 10 REVIEW images excluded.
- Box-level micro: TP=13, FP=24, FN=22, precision=0.351351, recall=0.371429, F1=0.361111.
- Cigarette-level: 10 GT NG / 10 GT OK; missed-NG=3 (0.30), false-NG=3 (0.30).
- Regression: `python -m unittest discover -s tests/p5 -p "test_*.py"` -> 76/76 PASS.
- TensorRT remains blocked: all available engines fail TensorRT 8.6.1 deserialization; rebuilding from ONNX fails because the `Mod` node plugin is unavailable.
- This pilot result is diagnostic only and is not a product-acceptance or commercial-performance claim.

## P5-02C4 final independent review (2026-07-19)

- Initial reviewer: FAIL because `runtime_contract` was hash-bound but not semantically validated.
- Repair: strict schema/backend/provider/version/scope validation; model and predictions cross-hash checks; source runtime manifest hash check; detector-config identity check; explicit report classification; negative tests.
- Independent reviewer after repair: PASS.
- Independent QA: PASS, limited to provisional ONNX Runtime CPU fallback baseline.
- Regression after repair: 76/76 PASS; `git diff --check` PASS (line-ending warnings only).
- Evidence report SHA-256: `2bc9b7115ba67563308fcb70fe3c4435c8849b89346962b8148d6487917b24d3`.
- Evidence manifest SHA-256: `9a9fd67471413eebcd8cd57179a8048bf7e868f313035921459046b6735968d6`.
- Formal TensorRT baseline remains BLOCKED / NOT VERIFIED under KI-037.
