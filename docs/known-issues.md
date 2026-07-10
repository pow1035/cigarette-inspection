# 已知问题与证据缺口

状态：开放、验证中、已修复、接受限制。严重度：阻断、高、中、低。

| ID | 严重度 | 问题 | 位置/证据 | 状态 | 目标阶段 |
| --- | --- | --- | --- | --- | --- |
| KI-001 | 阻断 | 本轮没有 Windows/Visual Studio 构建证据，无法确认新版当前可编译 | `CigVision.sln`、`.vcxproj` 仅代码检查 | 开放 | P1 |
| KI-002 | 高 | 新版运行按钮曾只切换布尔量，相机在初始化时直接取流 | `CigVision::on_btn_run_clicked`、`startCameras/stopCameras` 已重构；待 Windows 重复启停 QA | 已修复 | P1 |
| KI-003 | 阻断 | 新版相机映射曾重复写入 `2-1`，导致 `2-2` 无法匹配 | P1 静态门检查 `2-2` 映射及三相机完整匹配 | 已修复 | P1 |
| KI-004 | 高 | 三个相机回调曾对 `new[]` 缓冲使用标量 `delete` | 回调改用 `std::vector`；P1 静态门禁止 `delete dataGray` | 已修复 | P1 |
| KI-005 | 高 | 帧队列曾保存 SDK 回调临时 `pFrameInfo` 指针 | `picStruct` 现按值保存 `MV_FRAME_OUT_INFO`；待相机运行 QA | 已修复 | P1 |
| KI-006 | 阻断 | 新版 `ProcessImage()` 只复制图像，`StickDarkCheck()` 固定失败 | `process/ImageProcess.cpp:364-370,407-435` | 开放 | P2/P3 |
| KI-007 | 高 | `testWrite` 引用老版 `testQT` 且无有效 IO 实现 | 历史文件保留，但已从新版 vcxproj/filters 排除 | 已修复 | P1 |
| KI-008 | 阻断 | 新版工程尚未链接 OpenCV/TensorRT 推理实现 | `CigVision.vcxproj` 与 `03_深度学习模型与TensorRT` 分离 | 开放 | P4 |
| KI-009 | 阻断 | Debug/Release Halcon 版本分裂，目标机 Qt/MVS/DAQNavi 位置和运行时 DLL 未确认 | `docs/windows-build-baseline.md`；Release MVS、ProjectReference、OutDir 已静态修复 | 开放 | P1/P4 |
| KI-010 | 高 | 老版保存路径和 IO 设备配置硬编码，禁用剔除时还会先丢弃 NG 事件 | `saveImage.cpp:24-67,92-133`；`reject.cpp:20-51` | 开放 | P2/P5 |
| KI-011 | 阻断 | 模型类别名称、业务缺陷映射、逐类阈值和准确率基准未确认 | 模型与样例目录未见正式验收清单 | 开放 | P4 |
| KI-012 | 中 | 测试图片存在，但 ground truth、划分、来源和授权边界未形成清单 | `04_测试数据与样例图片` | 开放 | P3/P4 |
| KI-013 | 高 | 生产节拍、最大队列、允许推理时延和剔除延迟公式未确认 | 待现场参数 | 开放 | P5/P6 |
| KI-014 | 中 | UI、相机、GPU 和 IO 的运行日志/指标尚未实现 | 新版未见统一可观测性模块 | 开放 | P3/P7 |
| KI-015 | 阻断 | 新版灰度队列仍没有检测消费者 | P1 已改为有界且统一加锁；消费链在 P3 实现 | 开放 | P3 |
| KI-016 | 高 | 新版析构曾不停止取流、相机和 IO 线程 | P1 增加回调计数屏障、stop/shutdown、IO 有界等待告警和 RAII Close；待 Windows 运行 QA | 验证中 | P1 |
| KI-017 | 高 | 老版 `2-2` 相机队列未见消费者，图像用异步全局 IO 编号，复检 key 仅为 `uchar` | `testQT.cpp:35-37,96-126`；`process2_1.cpp:261-301` | 开放 | P2/P5 |
| KI-018 | 高 | 老版队列和统计存在锁不完整/数据竞争，并复现数组释放和回调指针问题 | `testQT.cpp:22-52,1019-1034`；`process.cpp:51-65,86-118`；`testQT.h:40-46` | 开放 | P2/P5 |
| KI-019 | 阻断 | TensorRT 原型不校验 engine shape/binding，CUDA/TensorRT 调用返回值未检查 | `yolo_trt_v2.cpp:172-180,259-267,355-365` | 开放 | P4 |
| KI-020 | 阻断 | TensorRT 预/后处理依赖未确认的 resize、张量布局、6/7 列和排序契约 | `yolo_trt_v2.cpp:189-213,321-328,384-413,444-460` | 开放 | P4 |
| KI-021 | 高 | TensorRT 测试缺初始化状态检查，自动化会被 `waitKey(0)` 阻塞 | `test_yanzhi20260120.cpp:95-99,145-148` | 开放 | P4 |
| KI-022 | 中 | TensorRT 原型目录的多份历史文档含相互冲突的“完成/性能”声明，当前没有配套运行证据 | `运行指南.md:3-20`；`编译完成总结.md:3-23`；`README_USAGE.md:91-94`；`完整部署步骤.md:1-8`；`部署完成总结.md:1-3` | 开放 | P4 |
| KI-023 | 高 | 相机 SDK 回调仍执行 RGB 转灰度和 Halcon 图像构造，不符合最终轻量回调边界 | P1 用回调屏障和异常捕获保证退出；原始帧契约与消费线程在 P2/P3 重构 | 开放 | P2/P3 |
| KI-024 | 高 | `rejectEnabled=false` 只是默认配置，当前没有真实 DO 输出和运行时硬件联锁 | P1 只声明“无输出实现”；模拟输出在 P5，真实安全联锁在 P6 | 开放 | P5/P6 |
| KI-025 | 高 | IO 连续读取失败曾只停读取线程，主界面和相机仍显示运行 | P1 已通过 queued signal 进入统一安全停止；持久报警、指标和恢复策略仍待实现 | 验证中 | P1/P5/P7 |
| KI-026 | 高 | `MV_CC_StopGrabbing` 后是否存在晚到帧、是否会污染下一次 Run 尚无目标 SDK 证据 | P1 增加进程期回调守卫、detach 和双层活动计数，避免析构访问已释放窗口；仍需 Windows MVS 重复启停/失败注入 | 验证中 | P1/P5 |

详细审计摘要见 `docs/code-audit.md`。代码检查不能替代 Windows、GPU 或真实硬件运行证据。
