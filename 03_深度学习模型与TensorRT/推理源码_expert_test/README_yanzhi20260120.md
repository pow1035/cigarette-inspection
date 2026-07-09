# YOLO TensorRT 推理测试工程

本项目为 `yanzhi20260120.engine` TensorRT 模型的 C++ 推理测试程序。

## 功能特性

- 支持批量处理 `inference/test/` 目录下的所有图片
- 自动检测并可视化目标检测结果
- 显示详细的推理时间和检测信息
- 保存带有检测框的结果图片
- 支持多种图片格式 (JPG, PNG, BMP 等)

## 环境要求

- Windows 10/11
- Visual Studio 2019/2022
- CMake 3.18+
- CUDA 11.8
- TensorRT 8.6.1.6
- OpenCV 4.x

## 构建步骤

### 1. 配置 CMake

在 `expert_test` 目录下创建 build 目录并配置：

```bash
cd expert_test
mkdir build_yanzhi
cd build_yanzhi
cmake .. -G "Visual Studio 16 2019" -A x64
```

或者使用 Visual Studio 2022：

```bash
cmake .. -G "Visual Studio 17 2022" -A x64
```

### 2. 编译项目

使用 CMake 编译：

```bash
cmake --build . --config Release
```

或者用 Visual Studio 打开生成的 `.sln` 文件进行编译。

### 3. 运行测试

编译完成后，可执行文件位于 `build_yanzhi/Release/test_yanzhi20260120.exe`

## 使用方法

### 基本用法（使用默认参数）

```bash
cd e:/code/yolo26
./expert_test/build_yanzhi/Release/test_yanzhi20260120.exe
```

默认参数：
- 模型路径：`yanzhi20260120.engine`（项目根目录下）
- 测试图片目录：`inference/test/`
- 结果输出目录：`inference/test_results/`

### 自定义参数

```bash
./expert_test/build_yanzhi/Release/test_yanzhi20260120.exe [model_path] [test_dir] [output_dir]
```

示例：

```bash
./expert_test/build_yanzhi/Release/test_yanzhi20260120.exe yanzhi20260120.engine inference/test/ results/
```

## 输出说明

### 控制台输出

程序会显示：
1. 模型加载信息
2. 每张图片的处理进度
3. 图片尺寸和推理时间
4. 检测到的目标数量和详细信息（类别、置信度、边界框坐标）
5. 处理总结统计

示例输出：
```
========================================
  YOLO TensorRT Inference Test
  Model: yanzhi20260120.engine
========================================

Model:       yanzhi20260120.engine
Test images: inference/test/
Output dir:  inference/test_results

Loading TensorRT model...
Model loaded successfully!

Found 1 image(s) to process.

------ Processing: inference/test/test1.jpg ------
Image size: 1920x1080
Inference time: 15 ms
Detections found: 5

Detection results:
  No.     Class  Confidence            BBox [x, y, w, h]
-------------------------------------------------------------------
    1         0      95.50%          [120, 240, 180, 320]
    2         1      89.20%          [450, 180, 220, 280]
    ...

Result saved to: inference/test_results/result_test1.jpg

Press any key to continue...

========================================
Processing Summary
========================================
Total images:    1
Total time:      15 ms
Average time:    15 ms/image
Output saved to: inference/test_results
========================================
```

### 可视化结果

- 每张图片处理完成后会弹出显示窗口，显示带有检测框和标签的结果
- 按任意键继续处理下一张图片
- 结果图片自动保存到输出目录，文件名格式：`result_原文件名.jpg`

## 文件结构

```
yolo26/
├── yanzhi20260120.engine          # TensorRT 模型文件
├── inference/
│   ├── test/                      # 测试图片目录
│   │   └── test1.jpg
│   └── test_results/              # 结果输出目录（自动创建）
│       └── result_test1.jpg
└── expert_test/
    ├── yolo_trt_v2.h              # TensorRT 推理类头文件
    ├── yolo_trt_v2.cpp            # TensorRT 推理类实现
    ├── test_yanzhi20260120.cpp    # 测试程序（新增）
    ├── CMakeLists.txt             # CMake 配置
    └── build_yanzhi/              # 编译输出目录
        └── Release/
            └── test_yanzhi20260120.exe
```

## 模型参数

- 输入尺寸：992x992
- 置信度阈值：0.25
- NMS IOU 阈值：0.45

如需修改这些参数，可以在 [test_yanzhi20260120.cpp:141](expert_test/test_yanzhi20260120.cpp#L141) 中修改：

```cpp
YOLOTensorRTv2 detector(model_path, 992, 0.25f, 0.45f);
//                                   ↑    ↑      ↑
//                         input_size  conf   iou_thresh
```

## 常见问题

### 1. 找不到模型文件

确保 `yanzhi20260120.engine` 位于项目根目录（`e:/code/yolo26/`），或者使用完整路径指定模型位置。

### 2. 缺少 DLL 文件

CMake 会自动复制所需的 DLL 到可执行文件目录，如果仍然报错，请检查：
- TensorRT DLL：`nvinfer.dll`, `nvonnxparser.dll` 等
- CUDA DLL：`cudart64_110.dll`, `cublas64_11.dll` 等
- OpenCV DLL：`opencv_world*.dll`

### 3. CUDA 内存不足

如果遇到 CUDA 内存错误，可以尝试：
- 减小输入图片数量
- 关闭其他占用 GPU 的程序

## 性能参考

在典型配置下（RTX 30/40 系列显卡）：
- 单张图片推理时间：5-15 ms
- 吞吐量：60-200 FPS

## 与 Python 版本对比

本 C++ 版本参考了 [ww_test_yolo_to_tensorrt.py](ww_test_yolo_to_tensorrt.py) 的功能：

Python 版本：
```python
results = tensorrt_model.predict(source='./inference/test/',
                                 stream=True, show=True,
                                 imgsz=992, save=True)
```

C++ 版本提供了相似的功能：
- ✅ 支持批量推理 (`inference/test/` 目录)
- ✅ 显示推理结果 (可视化窗口)
- ✅ 保存结果图片 (到 `test_results` 目录)
- ✅ 使用相同的输入尺寸 (992x992)
- ✅ 显示检测框坐标 (xyxy 格式)

## 许可证

参考项目主许可证。
