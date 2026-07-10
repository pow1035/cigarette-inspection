# 源码静态审计基线

日期：2026-07-10

三名独立 explorer 分别只读检查新版 Qt、老版闭环和 TensorRT 原型。以下结论是 P0 开始时的历史基线，不代表 P1 返修后的当前状态；修复状态以 `known-issues.md`、`evidence-matrix.md` 和 `review-results.md` 为准。未执行 Windows 编译、GPU 推理、相机采集或真实 IO。

## 新版 CigVision

本节的行号和问题描述用于保留返修前证据。相机映射、帧信息按值保存、数组释放、Run/Stop 生命周期和 IO 线程退出已在 P1 修改，不能继续解读为当前未修复结论。

已有 UI、参数管理、相机枚举/配置/回调、图像入队、PCIE-1730 输入读取和部分 Halcon 定位函数，但没有帧消费、检测结果、保存、统计、剔除和安全停机闭环。

| 严重度 | 结论 | 代码证据 |
| --- | --- | --- |
| 阻断 | 相机映射把 `2-1` 写了两次，`2-2` 无法匹配 | `CigVision.cpp:561-565` |
| 高 | 相机未完整匹配时仍可能返回成功 | `CigVision.cpp:578-703` |
| 高 | 队列保存 SDK 回调的临时 `pFrameInfo` 指针 | `CigVision.cpp:40-42,82-84,124-126`；SDK 说明 `MvCameraControl.h:448-466` |
| 高 | 三个回调对 `new[]` 缓冲使用标量 `delete` | `CigVision.cpp:27-56,69-98,111-130` |
| 阻断 | 三个灰度队列只有入队，未找到实际消费者 | `CigVision.cpp:43-51,86-94,127` |
| 高 | 队列的 `size/dequeue/enqueue` 未统一锁保护 | `CigVision.cpp:15-17,43-51` |
| 高 | Run 按钮只切换布尔量；相机在初始化时已开始取流 | `CigVision.cpp:422-435,688-697` |
| 高 | 析构未停止取流、关闭相机、等待线程或停止 IO | `CigVision.cpp:244-245`；`MyCamera.cpp:14-21` |
| 阻断 | `ProcessImage()` 只复制图像，`Cleanup()` 为空 | `process/ImageProcess.cpp:407-435` |
| 高 | `StickDarkCheck()` 不输出矩形并固定返回失败 | `process/ImageProcess.cpp:364-370` |
| 高 | `testWrite::run()` 的 IO 写入与剔除逻辑全部被注释 | `testWrite.cpp:31-106` |
| 阻断 | Release 缺相机库，process DLL 无项目引用/复制，存在机器绝对路径 | `CigVision.vcxproj:72-90`；`CigVision.sln:6-9`；`CigVisionParams.cpp:6435-6439`；`process/process.vcxproj:73-76` |

## 老版传统算法闭环

可确认的参考链路是：相机回调转灰度并附带异步 IO 编号，`process/process2_1` 消费两个相机队列，生成统计、保存任务和 `uint8` NG 编号，`reject` 写 PCIE-1730，`saveImage` 写固定盘符。

| 可迁移的业务知识 | 不得照搬的问题 | 代码证据 |
| --- | --- | --- |
| 相机身份、触发、像素格式、ROI 和回调入口 | 序列号/分辨率硬编码 | `testQT.cpp:522-606` |
| 每帧携带相机、编号、IO 快照 | 图像读取异步全局 IO，可能错配 | `testQT.cpp:12-52`；`readIOTask.cpp:18-81` |
| 为每个工位定义消费者和丢帧策略 | `2-2` 入队但未见处理线程消费 | `testQT.cpp:96-126`；`process.cpp:17-65`；`process2_1.cpp:18-52` |
| 传统缺陷位和上下半区复检规则 | `uchar` 作为复检 key，回绕/乱序会覆盖 | `process2_1.cpp:214-301,320-449` |
| NG 事件驱动保存和剔除 | NG 队列只有 `uint8`，不可追溯多缺陷来源 | `process.cpp:141-169`；`process2_1.cpp:129-160` |
| DO 握手、编号和脉宽 | 禁用剔除时先出队，NG 事件会静默丢失 | `reject.cpp:20-51` |
| 分类保存原图、单支图和标注图 | 固定 `D:\\test1`、类型拼接风险、无失败回执 | `saveImage.cpp:24-67,92-133` |
| 检测数、NG 数和缺陷类型统计口径 | 多线程无锁修改和读取统计 | `process.cpp:86-118`；`process2_1.cpp:89-106`；`testQT.cpp:1019-1034` |
| 生产者/消费者结构 | 锁不完整、`delete`/`new[]` 不匹配、回调指针未深拷贝 | `testQT.cpp:22-52`；`process.cpp:51-65`；`saveImage.cpp:13-27`；`testQT.h:40-46` |

## YOLO/TensorRT 原型

`YOLOTensorRTv2::detect(const cv::Mat&)` 已能返回类别、置信度和框，支持 engine/ONNX 入口，并尝试兼容 raw YOLO 输出与已 NMS 输出。但构建脚本、TensorRT 生命周期、张量契约和后处理假设尚未达到可直接嵌入上位机的程度。

| 严重度 | 结论 | 代码证据 |
| --- | --- | --- |
| 高 | TensorRT 对象使用默认 `unique_ptr` 删除器，生命周期契约不明确 | `yolo_trt_v2.h:71-73` |
| 高 | 输入分配只相信构造参数，不校验 engine shape | `yolo_trt_v2.cpp:172-180,259-267` |
| 高 | 固定两个 binding、固定三维输出，未处理动态 shape/角色/数量 | `yolo_trt_v2.cpp:173-177` |
| 高 | CUDA 拷贝、stream、`enqueueV2`、同步返回值未检查 | `yolo_trt_v2.cpp:20,355-365` |
| 高 | 预处理为直接拉伸；若模型使用 letterbox，坐标会系统性偏移 | `yolo_trt_v2.cpp:321-328,384-413` |
| 高 | raw 输出识别依赖维度经验值，`4+class` 假设不覆盖常见变体 | `yolo_trt_v2.cpp:189-213` |
| 高 | 接受 6/7 列输出却固定按 6 列解析 | `yolo_trt_v2.cpp:199-208,444-456` |
| 中 | 低置信度使用 `break`，依赖未声明的降序输出 | `yolo_trt_v2.cpp:458-460` |
| 高 | 测试程序无初始化状态检查，失败时空结果可能被当作成功 | `test_yanzhi20260120.cpp:145-148`；`yolo_trt_v2.cpp:342-345` |
| 高 | OpenCV DLL 复制路径疑似错误，依赖说明没有精确版本/engine 兼容约束 | `CMakeLists_TRT_v2.txt:14,86`；`07_运行环境与依赖包/README_本地大文件说明.md:3-12` |
| 中 | `waitKey(0)` 会阻断无 GUI 自动化 | `test_yanzhi20260120.cpp:95-99` |

补充核对：`CMakeLists_TRT_v2.txt:48-50` 引用的 `test_yolo_trt_v2.cpp` 在仓库中真实存在，不能把它记录为构建错误。`test_yanzhi20260120.cpp` 是另一个测试入口，是否纳入正式构建由 P4 决定。

### 历史文档边界

该原型目录中的 `运行指南.md`、`编译完成总结.md`、`部署完成总结.md`、`README_USAGE.md` 等文件包含原作者在特定机器上的“已编译/已部署/性能”描述。本轮没有对应构建日志、硬件信息、模型哈希或运行输出，因此这些文件只作为历史线索，不作为 P0、P1 或 P4 的当前通过证据。P4 将指定唯一有效入口，其余文档再做归档或状态标注。

## 对主线的约束

1. P1 先让新版工程可重复构建并修复确定性内存、映射、生命周期和启停缺陷。
2. P2 先建立拥有图像内存的 `FramePacket`、可追踪的结果/剔除契约和线程安全有界队列。
3. P3 用离线回放验证闭环，真实 IO 保持关闭。
4. P4 接入 TensorRT 前必须固定模型输入/输出契约，并为 CUDA/TensorRT 调用增加错误检查。
5. 老版只提供业务规则线索，迁移时逐条写测试，不能复制旧线程和裸队列实现。
