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
| KI-012 | 中 | 测试图片存在，但 ground truth、划分、来源和授权边界未形成清单 | P4 固定 116 文件/113 唯一哈希，仅做视觉与运行评估；P5 补齐数据治理 | 开放 | P5 |
| KI-013 | 高 | 生产节拍、最大队列、允许推理时延和剔除延迟公式未确认 | P6 先用一组可配置本地节拍做容量曲线；现场参数继续冻结，不作现场声明 | 开放 | P6/现场冻结 |
| KI-014 | 中 | UI、帧源、GPU、保存和模拟输出的统一运行日志/指标尚未实现 | P3/P4 已有逐帧 JSON、汇总统计和错误计数；P6-P8 补本地流、UI 与资源指标，硬件指标冻结 | 开放 | P6/P7/P8 |
| KI-015 | 阻断 | 新版在线灰度队列仍没有检测消费者 | P3 已完成独立离线图片消费链；P6 只实现录制流/文件流消费者，真实相机回调迁移冻结 | 开放 | P6/现场冻结 |
| KI-016 | 高 | 新版析构曾不停止取流、相机和 IO 线程 | P1 增加回调计数屏障、stop/shutdown、IO 有界等待告警和 RAII Close；待 Windows 运行 QA | 验证中 | P1 |
| KI-017 | 高 | 老版 `2-2` 相机队列未见消费者，图像用异步全局 IO 编号，复检 key 仅为 `uchar` | P2 契约改用 64 位 frameId/32 位烟支编号且不复制老版队列；P6 本地仿真验证关联，现场关联冻结 | 开放 | P6/现场冻结 |
| KI-018 | 高 | 老版队列和统计存在锁不完整/数据竞争，并复现数组释放和回调指针问题 | P3 独立离线链已使用 BoundedQueue；老版保持只读，P6 扩展本地实时仿真 | 开放 | P6 |
| KI-019 | 阻断 | TensorRT 原型不校验 engine shape/binding，CUDA/TensorRT 调用返回值未检查 | 新适配器枚举名称 I/O、验证 FP32 `[1,3,992,992]`/`[1,300,6]` 并检查 CUDA/enqueueV3；旧原型不进入产品构建 | 已修复 | P4 |
| KI-020 | 阻断 | TensorRT 预/后处理依赖未确认的 resize、张量布局、6/7 列和排序契约 | ONNX 本体确认 `images`/`output0`；OpenCV INTER_LINEAR stretch RGB CHW 与 ONNX 对照；按 `[x1,y1,x2,y2,score,class]` 解析 | 已修复 | P4 |
| KI-021 | 高 | TensorRT 测试缺初始化状态检查，自动化会被 `waitKey(0)` 阻塞 | 新批处理入口无 GUI 阻塞；初始化失败 exit 4 并写错误 JSON；旧 demo 不进入产品构建 | 已修复 | P4 |
| KI-022 | 中 | TensorRT 原型目录的多份历史文档含相互冲突的“完成/性能”声明，当前没有配套运行证据 | `运行指南.md:3-20`；`编译完成总结.md:3-23`；`README_USAGE.md:91-94`；`完整部署步骤.md:1-8`；`部署完成总结.md:1-3` | 开放 | P4 |
| KI-023 | 高 | 相机 SDK 回调仍执行 RGB 转灰度和 Halcon 图像构造，不符合最终轻量回调边界 | P3 已验证自持 FramePacket 离线消费链；真实相机回调迁移冻结，本地流不复用该回调 | 开放 | 现场冻结 |
| KI-024 | 高 | `rejectEnabled=false` 只是默认配置，当前没有真实 DO 输出和运行时硬件联锁 | P1 只声明“无输出实现”；P6 仅实现模拟输出，真实安全联锁冻结 | 开放 | P6/现场冻结 |
| KI-025 | 高 | IO 连续读取失败曾只停读取线程，主界面和相机仍显示运行 | P1 已通过 queued signal 进入统一安全停止；P7 可复用该状态模型做本地错误 UI，真实 IO 恢复仍属现场冻结 | 验证中 | P7/现场冻结 |
| KI-026 | 高 | `MV_CC_StopGrabbing` 后是否存在晚到帧、是否会污染下一次 Run 尚无目标 SDK 证据 | P1 增加进程期回调守卫、detach 和双层活动计数；P6 只验证本地流重复启停，MVS 结论保持未验证 | 验证中 | P6/现场冻结 |
| KI-027 | 阻断 | HALCON 22.11.4.0 已加载但本机没有许可证，许可算子和 HDevelop 无法验证 | P3 fixture 不调用许可算子；CigVision 启动只证明 DLL 可加载，不证明许可检测流程 | 开放 | P4 |
| KI-028 | 中 | Release 构建仍有 `IMAGEPROCESS_EXPORTS` 重定义和源文件代码页 C4819 警告 | `artifacts/p1-windows-20260711-115033/msbuild-release.log`；本轮不做编码批量改写，避免扩大 P1 修复范围 | 开放 | P1/P2 |
| KI-029 | 高 | P3 固定样本的 OK/NG 是链路 fixture 期望，不是人工 ground truth，也不能用于准确率评估 | `tests/fixtures/p3-samples.json`；偶数 frame_id 固定 NG、奇数固定 OK；P4 必须建立真实标签/授权/模型评估清单 | 接受限制 | P4 |
| KI-030 | 高 | P4 样本没有人工 ground truth，视觉抽查可见重叠框、超大框、空检候选和标签遮挡，不能判定误检/漏检 | 正式及独立 QA frame 1/3/6/12 抽查；P5 必须建立真值、指标和优化前后对照 | 开放 | P5 |
| KI-031 | 中 | TensorRT engine 与 GPU/TRT 版本绑定，当前候选 engine 仅存 artifacts，不可作为跨机器部署包 | TRT 8.6 旧 engine 在 TRT 10.15 反序列化失败；当前 engine 由 RTX 4060/TRT 10.15 本机构建 | 接受限制 | P4/P7 |

详细审计摘要见 `docs/code-audit.md`。代码检查不能替代 Windows、GPU 或真实硬件运行证据。
