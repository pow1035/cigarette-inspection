# 已知问题与证据缺口

状态：开放、验证中、已修复、接受限制。严重度：阻断、高、中、低。

| ID | 严重度 | 问题 | 位置/证据 | 状态 | 目标阶段 |
| --- | --- | --- | --- | --- | --- |
| KI-001 | 阻断 | Windows/Visual Studio 构建仍未成功，无法确认新版当前可编译 | `artifacts/p1-windows-20260711-115033`：Release MSBuild exit 0，生成并哈希 `CigVision.exe/process.dll`；主界面成功启动 | 已修复 | P1 |
| KI-002 | 高 | 新版运行按钮曾只切换布尔量，相机在初始化时直接取流 | `CigVision::on_btn_run_clicked`、`startCameras/stopCameras` 已重构；待 Windows 重复启停 QA | 已修复 | P1 |
| KI-003 | 阻断 | 新版相机映射曾重复写入 `2-1`，导致 `2-2` 无法匹配 | P1 静态门检查 `2-2` 映射及三相机完整匹配 | 已修复 | P1 |
| KI-004 | 高 | 三个相机回调曾对 `new[]` 缓冲使用标量 `delete` | 回调改用 `std::vector`；P1 静态门禁止 `delete dataGray` | 已修复 | P1 |
| KI-005 | 高 | 帧队列曾保存 SDK 回调临时 `pFrameInfo` 指针 | `picStruct` 现按值保存 `MV_FRAME_OUT_INFO`；待相机运行 QA | 已修复 | P1 |
| KI-006 | 阻断 | 新版曾没有生产检测器；历史 `ProcessImage()`/`StickDarkCheck()` 不能冒充完成算法 | P4 已接入真实 TensorRT `IDetector`，116 图运行及独立门禁通过 | 已修复 | P4 |
| KI-007 | 高 | `testWrite` 引用老版 `testQT` 且无有效 IO 实现 | 历史文件保留，但已从新版 vcxproj/filters 排除 | 已修复 | P1 |
| KI-008 | 阻断 | 新版工程尚未链接 OpenCV/TensorRT 推理实现 | vcxproj 已链接 TensorRT 10、CUDA 13.1、OpenCV 4.9；Release Rebuild 与运行 exit 0 | 已修复 | P4 |
| KI-009 | 高 | Debug/Release Halcon 版本分裂；仅 Release 精确版本已安装 | Release 22.11.4.0 已安装于 `D:\MVTec\HALCON-22.11-Steady` 并通过构建/启动；Debug 目标 `D:\MVTec\HALCON-25.05-Progress` 仍缺失，官方当前目录未提供 25.05 | 开放 | P1/P4 |
| KI-010 | 高 | 老版保存路径和 IO 设备配置硬编码，禁用剔除时还会先丢弃 NG 事件 | 老版仅作参考；P7/P8 重新设计本地保存与模拟输出，不复制旧实现 | 开放 | P7/P8 |
| KI-011 | 阻断 | 模型 ASCII 类别已确认，但正式中文业务映射、逐类阈值和准确率基准未确认 | P4 技术集成关闭时明确不声明准确率；P5 以人工标注和冻结测试集建立产品效果门 | 开放 | P5 |
| KI-012 | 中 | 完整测试数据的 ground truth、互斥划分、来源和授权边界尚未形成清单 | 30 图 reviewed pilot 已授权并冻结，但只覆盖 20 张可比较图片；P4 的 116 文件/113 唯一哈希仍主要用于视觉与运行评估 | 开放 | P5 |
| KI-013 | 高 | 生产节拍、最大队列、允许推理时延和剔除延迟公式未确认 | P6-02 已形成 SDK-free 容量/丢弃/P95 曲线并证明模型内取舍；真实图片解码、TensorRT、Qt 保存和现场参数仍未冻结，不作生产声明 | 开放 | P6/现场冻结 |
| KI-014 | 中 | UI、帧源、GPU、保存和模拟输出的统一运行日志/指标尚未完整实现 | P6-01B 新增 simulation trace 源码，P6-02 新增虚拟 queue/pipeline/P95/逐相机统计；`p6_windows_simulation_evidence.py` 现可保存目标机 batch/trace/负路径日志与哈希，但产品 runtime、资源和 UI 统一指标仍待 P6-P8，硬件指标冻结 | 开放 | P6/P7/P8 |
| KI-015 | 阻断 | 新版在线灰度队列仍没有检测消费者 | P3 已完成独立离线图片消费链；P6 只实现录制流/文件流消费者，真实相机回调迁移冻结 | 开放 | P6/现场冻结 |
| KI-016 | 高 | 新版析构曾不停止取流、相机和 IO 线程 | P1 增加回调计数屏障、stop/shutdown、IO 有界等待告警和 RAII Close；待 Windows 运行 QA | 验证中 | P1 |
| KI-017 | 高 | 老版 `2-2` 相机队列未见消费者，图像用异步全局 IO 编号，复检 key 仅为 `uchar` | P2 契约改用 64 位 frameId/32 位烟支编号且不复制老版队列；P6 本地仿真验证关联，现场关联冻结 | 开放 | P6/现场冻结 |
| KI-018 | 高 | 老版队列和统计存在锁不完整/数据竞争，并复现数组释放和回调指针问题 | P3 独立离线链已使用 BoundedQueue；老版保持只读，P6 扩展本地实时仿真 | 开放 | P6 |
| KI-019 | 阻断 | TensorRT 原型不校验 engine shape/binding，CUDA/TensorRT 调用返回值未检查 | 新适配器枚举名称 I/O、验证 FP32 `[1,3,992,992]`/`[1,300,6]` 并检查 CUDA/enqueueV3；旧原型不进入产品构建 | 已修复 | P4 |
| KI-020 | 阻断 | TensorRT 预/后处理依赖未确认的 resize、张量布局、6/7 列和排序契约 | ONNX 本体确认 `images`/`output0`；OpenCV INTER_LINEAR stretch RGB CHW 与 ONNX 对照；按 `[x1,y1,x2,y2,score,class]` 解析 | 已修复 | P4 |
| KI-021 | 高 | TensorRT 测试缺初始化状态检查，自动化会被 `waitKey(0)` 阻塞 | 新批处理入口无 GUI 阻塞；初始化失败 exit 4 并写错误 JSON；旧 demo 不进入产品构建 | 已修复 | P4 |
| KI-022 | 中 | TensorRT 原型目录的多份历史文档曾含相互冲突的“完成/性能”声明 | 目录内 22 份 Markdown 文档现统一带 `HISTORICAL_TENSORRT_PROTOTYPE` 警告，明确不是当前构建、性能或交付证据；`validate_project_docs.sh` 防回退 | 已修复 | P4/P8 |
| KI-023 | 高 | 相机 SDK 回调仍执行 RGB 转灰度和 Halcon 图像构造，不符合最终轻量回调边界 | P3 已验证自持 FramePacket 离线消费链；真实相机回调迁移冻结，本地流不复用该回调 | 开放 | 现场冻结 |
| KI-024 | 高 | `rejectEnabled=false` 只是默认配置，当前没有真实 DO 输出和运行时硬件联锁 | P1 只声明“无输出实现”；P6 仅实现模拟输出，真实安全联锁冻结 | 开放 | P6/现场冻结 |
| KI-025 | 高 | IO 连续读取失败曾只停读取线程，主界面和相机仍显示运行 | P1 已通过 queued signal 进入统一安全停止；P7 可复用该状态模型做本地错误 UI，真实 IO 恢复仍属现场冻结 | 验证中 | P7/现场冻结 |
| KI-026 | 高 | `MV_CC_StopGrabbing` 后是否存在晚到帧、是否会污染下一次 Run 尚无目标 SDK 证据 | P1 增加进程期回调守卫、detach 和双层活动计数；P6 只验证本地流重复启停，MVS 结论保持未验证 | 验证中 | P6/现场冻结 |
| KI-027 | 阻断 | HALCON 22.11.4.0 已加载但本机没有许可证，许可算子和 HDevelop 无法验证 | P3 fixture 不调用许可算子；CigVision 启动只证明 DLL 可加载，不证明许可检测流程 | 开放 | P4 |
| KI-028 | 中 | Release 构建曾有 `IMAGEPROCESS_EXPORTS` 重定义和源文件代码页 C4819 警告 | process DLL 导出宏已统一由项目定义一次，头文件显式区分 dllexport/dllimport；CigVision/process 全配置增加 `/utf-8`，P1 静态门防回退。仍待目标 Windows Release Rebuild 确认 warning 消失 | 验证中（源配置已修复） | P1/P8 |
| KI-029 | 高 | P3 固定样本的 OK/NG 是链路 fixture 期望，不是人工 ground truth，也不能用于准确率评估 | `tests/fixtures/p3-samples.json`；偶数 frame_id 固定 NG、奇数固定 OK；P4 必须建立真实标签/授权/模型评估清单 | 接受限制 | P4 |
| KI-030 | 高 | P4 样本没有人工 ground truth，视觉抽查可见重叠框、超大框、空检候选和标签遮挡，不能判定误检/漏检 | 正式及独立 QA frame 1/3/6/12 抽查；P5 必须建立真值、指标和优化前后对照 | 开放 | P5 |
| KI-031 | 中 | TensorRT engine 与 GPU/TRT 版本绑定，当前候选 engine 仅存 artifacts，不可作为跨机器部署包 | TRT 8.6 旧 engine 在 TRT 10.15 反序列化失败；当前 engine 由 RTX 4060/TRT 10.15 本机构建 | 接受限制 | P4/P7 |
| KI-032 | 阻断 | 烟支模型缺少完整独立标签集、训练 YAML、训练日志、互斥数据划分和测试独立性证明 | 30 图 reviewed pilot 只用于冻结诊断测试，不得用于训练或阈值调优；完整训练/验证/测试证据仍需补齐 | 开放 | P5 |
| KI-033 | 阻断 | `wuzi`、`jietou` 无源码支持的正式中文业务定义，现有 Qt 也没有对应逐类阈值 | 类别目录标为 `unconfirmed-do-not-label`，等待业务确认，不靠拼音猜测 | 开放 | P5 |
| KI-034 | 阻断 | ONNX 元数据/原型资料包含 AGPL 或研究用途提示，商业许可与第三方代码合规未确认 | 当前只做内部本地评估；商用交付前需形成模型、Ultralytics 和第三方代码许可清单 | 开放 | P5/P8 |
| KI-035 | 高 | P5-02 视觉抽查仍见大范围框、低置信框、重叠框和疑似类别误报，当前模型效果不能按截图判定为商业可用 | 双人 reviewed pilot 与 CPU 诊断基线已量化出明显 FP/FN；仍需恢复正式 TensorRT、补足代表性数据，并在独立验证集上比较阈值/NMS/过滤或训练前后效果 | 开放 | P5 |
| KI-036 | 阻断 | 旧工作台只保存一个名字，曾导致复核身份与授权证据缺失 | 2026-07-19 用户澄清肖朗逐页标注、小狼逐页检查并批准为真实数据；原 pass1 不变，正式证据 `artifacts/p5-reviewed-truth-20260719-124537` 为 30 reviewed、20 comparable、10 REVIEW excluded、35 GT 框；独立 reviewer/QA 最终 PASS | 已修复 | P5-02C3 |
| KI-037 | 高 | P5 首标工作台是无账号体系的 localhost 单机工具，且旧导出只保存单一人员名 | 工作台继续仅绑定 loopback、`/api/export-reviewed` 固定 409；复核晋级改由哈希绑定的 `p5_promote_reviewed_truth.py` 和项目负责人 attestation 完成，不把原 pass1 原地改真值 | 接受限制 | P5-02C3 |
| KI-038 | 低 | 当前 Windows 会话无创建符号链接权限，P5 可视化包的符号链接逃逸子分支未取得运行态证据 | 独立 QA `019f7866-8956-7af3-b466-c578bce34bc6`；`..`、绝对路径、分析输出及构建入口逃逸均已实际拒绝；待开发者模式、相应权限或 CI 补证 | 接受限制（非阻断） | P5/CI |
| KI-039 | 阻断 | 正式 P4 TensorRT 基线在当前主机仍不可用，现有 engine 无法反序列化且 ONNX 重建缺少插件 | 现有 `.engine` 在 TensorRT 8.6.1 报 `Magic tag does not match`；ONNX 重建在 `Mod` 节点失败；需兼容的编译执行文件、engine 或完整 TensorRT/CUDA/plugin 工具链 | 开放 | P5 |
| KI-040 | 阻断 | fresh clone 不包含被 `.gitignore` 排除的 reviewed-truth 与 fallback baseline artifact，指定范围 pilot 无法在本机复算 | readiness strict exit 2；需受控恢复、核对外部 evidence-manifest 摘要，再通过内部 manifest/大小/SHA-256/attestation 门 | 开放 | P5 |
| KI-041 | 高 | P7 产品状态、参数页和复核页尚未在 Qt/Windows 目标机编译运行，当前无法证明布局、信号槽、会话/profile 落盘和交互正确 | `ProductRuntimeState` 8/8；typed profile 和 Qt 源码已接线并固定 local-only；当前 Mac 无 Qt/MSVC，缺 Computer Use/人工 UI 运行证据 | 验证中 | P7 |
| KI-042 | 中 | Qt `QJsonDocument` 默认会折叠重复键，可能让 TensorRT 配置歧义通过 | loader 增加原始 token 扫描并按解码后顶层 key 拒绝重复字段；P8 preflight 再独立门禁；当前无 Qt/MSVC，仅完成源码修复 | 已修复，runtime 待验证 | P7/P8 |
| KI-043 | 阻断 | 尚无真实 Windows 产品发布包、完整 DLL/plugin/prerequisite 清单和 D 盘 A→B→A 转录 | v4 手册已固化现场 challenge 采集、v2 host report、package manifest、collector provenance、外置 32-byte HMAC key、带外 SHA/HMAC 和跨机 verify/import；当前 PowerShell 计数仍为 13 个脚本、CLI 7/7，但 `windowsRuntimeClaimed=false`、`productAcceptanceClaimed=false`。仍需真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV、数据、许可和硬件证据 | 开放 | P8 |
| KI-044 | 高 | 历史 P8 70 项范围曾缺少独立 reviewer/QA 返修结论 | cleanup 4 P1/3 P2、reviewer 追加 2 P1/2 P2、QA 追加 1 P1 均已修；最终 reviewer `019f99cb-7c02-7b23-b498-9b28e7ba761c` 与 QA `019f99d7-8a02-7da1-8641-2d533409d06d` 均 PASS，P0/P1/P2=0/0/0 | 已修复 | P8 |
| KI-045 | 高 | P8 主机报告 v2 / wrapper 与 receipt v4 返修曾缺最终 full gate | HEAD `6886856` 最终 full gate 已覆盖 P8 76/76、collector 锁内 SHA、wrapper collector 全程读锁/前后身份复核、PowerShell 13 脚本零 finding与 CLI 7/7 | 已修复（本地门） | P8 |
| KI-046 | 高 | 旧 `p8_soak_evidence.py` 的短 CI profile 允许几十毫秒成功进程通过，且 RSS 只比较不同重启进程峰值，不能证明同一连续进程的稳定性 | P8 v2 把 project compile 收进 `run` 受控路径，分离 verified duration/真实 process lifetime，增加 Linux `/proc` CPU 与 pure-sleep 拒绝、16/64 MiB RSS、progress/ProductRuntimeState、严格 inventory 和 Windows 双枚举回归；连续 5/5、P8 81/81、公开 wrapper、core/full 及正式 `final-v2` 2×300 evidence 均 PASS。两轮合计 3,793,408 帧，self-verify、独立 verify、reviewer 和正式 evidence QA/observability/cleanup 已收口 | 已修复（P8 v2 本地连续工具证据） | P8 |
| KI-047 | 中 | 本地 P8 v2 `soak-manifest.json` 与离线 verify 没有 Windows v4 的带外 HMAC，不能认证恶意方协调重写整个 evidence 包 | 当前 verify 仍严格复算包内哈希、派生值、snapshot 与当前源码绑定；本地 evidence 应保存在可信目录，跨边界交接时另存带外摘要或使用专门认证链。该限制不判定为工具失败 | 已记录（信任边界） | P8 |
| KI-048 | 高 | 连续工具复用 `p8_soak_evidence.py` helper，且 POSIX 资源采样依赖 `ps`，两者曾未完整进入 provenance | 当前实现已把新/旧 Python tool source 的 path/size/SHA/snapshot 与 POSIX `ps` 的 canonical path/size/SHA/version probe/固定 argv/binary snapshot 写入 toolDependencies；helper drift、PATH shadow、dependency record/helper/ps snapshot/identity 篡改均拒绝。专项 5/5、P8 81/81、core/full、正式 `final-v2` evidence 和多次独立 verify PASS；独立 reviewer 最终 P0/P1/P2/P3=0/0/0/0 | 已修复（独立复核通过） | P8 |
| KI-049 | 低 | 旧失败目录 `artifacts/p8-continuous-local-20260727-invalid-dependency-gap` 仍存在，可能被误认为正式证据 | 该目录无 `soak-manifest.json`、被 Git 忽略，与 `final-v2` 不混用；只隔离保留作失败审计，不得在文档、验收矩阵或交付包中引用。删除仅按用户/留存策略另行执行 | 已记录（非阻断保留策略） | P8 |

详细审计摘要见 `docs/code-audit.md`。代码检查不能替代 Windows、GPU 或真实硬件运行证据。

## KI-039: formal P4 TensorRT baseline unavailable on current host

- Status: OPEN; fallback baseline available but explicitly non-formal.
- Available `.engine` files fail TensorRT 8.6.1 deserialization with `Magic tag does not match`.
- ONNX-to-engine recovery fails at a `Mod` node because the required plugin is unavailable.
- Required recovery input: compatible compiled inference executable plus DLLs, a host-compatible engine, or the exact original TensorRT/CUDA/plugin toolchain.
- No additional human annotation is required for this blocker.

## KI-040: controlled P5 evaluation artifacts absent from fresh clone

- Status: OPEN; this is an input-availability blocker, separate from the TensorRT compatibility blocker in KI-039.
- Missing paths in the current fresh clone: `artifacts/p5-reviewed-truth-20260719-124537` and `artifacts/p5-fallback-baseline-20260719-161114`.
- The read-only readiness command reports only absent controlled artifact roots with exit 2. Missing base files, incomplete artifact directories, malformed manifests, manifest-bound file mismatches, invalid attestation, parent references and symbolic-link paths return exit 3.
- Readiness proves internal consistency relative to the supplied manifest; it does not authenticate a coordinated replacement of the manifest and all outputs. The fallback evidence-manifest SHA-256 is recorded in `HANDOFF_P5.md`; the reviewed evidence-manifest digest is not present in this clone and must be supplied through the controlled recovery channel.
- Required recovery: copy the two evidence directories through a controlled channel, verify their external evidence-manifest digests, then rerun readiness and the exact pilot command. Do not commit the artifacts or substitute CPU smoke for the formal TensorRT gate.
