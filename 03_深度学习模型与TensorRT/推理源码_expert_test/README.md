# YOLO26 TensorRT C++ 部署指南

本项目提供YOLO26模型的TensorRT优化和C++部署方案，针对RTX 3080Ti和RTX 4080S显卡进行了优化。

## 目录结构

```
expert_test/
├── export_tensorrt.py          # Python导出脚本
├── yolo_tensorrt.h             # C++推理引擎头文件
├── yolo_tensorrt.cpp           # C++推理引擎实现
├── test_yolo_tensorrt.cpp      # 测试程序
├── CMakeLists.txt              # CMake构建配置
├── build_and_test.bat          # Windows一键部署脚本
├── build_and_test.sh           # Linux一键部署脚本
├── README.md                   # 本文件
└── models/                     # 导出的模型文件(自动创建)
    ├── yanzhi20260115.onnx
    ├── yanzhi20260115_3080ti_fp16.engine
    └── yanzhi20260115_4080s_fp16.engine
```

## 环境要求

### 必需组件

1. **NVIDIA GPU**
   - RTX 3080Ti (12GB VRAM) 或
   - RTX 4080S (16GB VRAM)

2. **CUDA Toolkit** (>= 11.8)
   - 下载: https://developer.nvidia.com/cuda-toolkit

3. **TensorRT** (>= 8.6)
   - 下载: https://developer.nvidia.com/tensorrt

4. **Python** (>= 3.8)
   - PyTorch >= 2.0
   - ultralytics (YOLO)

5. **OpenCV** (>= 4.5)
   - C++库，需要带CUDA支持（可选，但推荐）

6. **CMake** (>= 3.18)

### Windows安装示例

```powershell
# 1. 安装CUDA Toolkit
# 从NVIDIA官网下载安装程序

# 2. 安装TensorRT
# 解压到: C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT

# 3. 安装Python依赖
pip install torch torchvision ultralytics opencv-python numpy

# 4. 安装OpenCV C++库
# 从 https://opencv.org/releases/ 下载预编译版本
# 或使用vcpkg: vcpkg install opencv[cuda]:x64-windows

# 5. 安装CMake
# 从 https://cmake.org/download/ 下载
```

### Linux安装示例

```bash
# 1. 安装CUDA Toolkit
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.0-1_all.deb
sudo dpkg -i cuda-keyring_1.0-1_all.deb
sudo apt-get update
sudo apt-get -y install cuda

# 2. 安装TensorRT
# 下载并解压到 /usr/local/TensorRT

# 3. 安装Python依赖
pip install torch torchvision ultralytics opencv-python numpy

# 4. 安装OpenCV和CMake
sudo apt-get install -y cmake libopencv-dev

# 5. 设置环境变量
export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:/usr/local/TensorRT/lib:$LD_LIBRARY_PATH
```

## 快速开始

### 方式1: 一键部署（推荐）

**Windows:**
```cmd
cd expert_test
build_and_test.bat
```

**Linux:**
```bash
cd expert_test
chmod +x build_and_test.sh
./build_and_test.sh
```

脚本将自动完成:
1. 导出ONNX和TensorRT引擎
2. 编译C++代码
3. 运行性能测试

### 方式2: 手动步骤

#### 步骤1: 导出TensorRT引擎

```bash
cd expert_test
python export_tensorrt.py
```

这将生成:
- `models/yanzhi20260115.onnx` - ONNX中间格式
- `models/yanzhi20260115_3080ti_fp16.engine` - 3080Ti优化引擎
- `models/yanzhi20260115_4080s_fp16.engine` - 4080S优化引擎

#### 步骤2: 编译C++代码

**Windows (Visual Studio):**
```cmd
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

**Linux:**
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

#### 步骤3: 运行测试

```bash
# 性能基准测试
./build/bin/test_yolo_tensorrt models/yanzhi20260115_4080s_fp16.engine bench

# 图像推理测试
./build/bin/test_yolo_tensorrt models/yanzhi20260115_4080s_fp16.engine image input.jpg output.jpg

# 视频推理测试
./build/bin/test_yolo_tensorrt models/yanzhi20260115_4080s_fp16.engine video input.mp4 output.mp4
```

## 使用说明

### Python导出脚本

`export_tensorrt.py` 提供以下功能:

- **ONNX导出**: 作为中间格式，兼容性更好
- **GPU特定优化**: 为3080Ti和4080S分别优化
- **FP16精度**: 在精度损失<1%的情况下提升2-3倍速度
- **性能基准**: 自动测试导出模型的推理速度

### C++推理引擎

`YOLOTensorRT` 类提供:

```cpp
// 初始化
YOLOTensorRT detector("model.engine", 640, 0.25f, 0.45f);
detector.initialize();

// 单张图像推理
cv::Mat image = cv::imread("test.jpg");
auto detections = detector.detect(image);

// 处理结果
for (const auto& det : detections) {
    std::cout << det.class_name << ": " << det.confidence << std::endl;
    cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);
}
```

### 配置参数

可以在代码中调整以下参数:

1. **输入尺寸** (`input_size`): 默认640，可选320/416/640/1280
2. **置信度阈值** (`conf_thresh`): 默认0.25
3. **NMS IoU阈值** (`iou_thresh`): 默认0.45
4. **工作空间** (`workspace`):
   - 3080Ti: 8GB (总显存12GB)
   - 4080S: 12GB (总显存16GB)

## 性能优化建议

### 1. 选择合适的输入尺寸

- **640x640**: 平衡精度和速度（推荐）
- **320x320**: 最快速度，精度下降
- **1280x1280**: 最高精度，速度较慢

### 2. 精度模式

- **FP16**: 推荐，速度快，精度损失<1%
- **FP32**: 最高精度，但速度慢2-3倍
- **INT8**: 需要校准数据，速度最快但精度可能下降

### 3. 批处理

修改输入batch size可以提高吞吐量:

```python
# 在export_tensorrt.py中
model.export(
    format='engine',
    batch=4,  # 批处理大小
    # ...
)
```

### 4. CUDA流优化

C++代码已使用异步CUDA流，可以进一步优化:

```cpp
// 多流并行
cudaStream_t streams[4];
for (int i = 0; i < 4; i++) {
    cudaStreamCreate(&streams[i]);
}
```

## 预期性能

### RTX 4080S (FP16, 640x640)

- **推理时间**: 2-3ms
- **总时间** (含预/后处理): 5-8ms
- **吞吐量**: ~125-200 FPS

### RTX 3080Ti (FP16, 640x640)

- **推理时间**: 3-4ms
- **总时间** (含预/后处理): 6-10ms
- **吞吐量**: ~100-160 FPS

*注: 实际性能取决于模型复杂度、输入分辨率和系统配置*

## 集成到其他项目

### 作为静态库使用

```cmake
# 在你的CMakeLists.txt中
add_subdirectory(expert_test)
target_link_libraries(your_app yolo_tensorrt)
```

### 头文件包含

```cpp
#include "yolo_tensorrt.h"

// 在你的代码中使用
YOLOTensorRT detector("model.engine");
detector.initialize();
auto results = detector.detect(image);
```

### 复制核心文件

最小化集成只需要:
- `yolo_tensorrt.h`
- `yolo_tensorrt.cpp`
- TensorRT引擎文件

## 故障排除

### 问题1: TensorRT引擎构建失败

**解决方案:**
- 确保CUDA和TensorRT版本兼容
- 检查GPU驱动是否最新
- 增加工作空间大小: `workspace=16`

### 问题2: 推理结果不准确

**解决方案:**
- 检查置信度阈值设置
- 验证类别名称是否正确
- 尝试使用FP32精度进行对比

### 问题3: 编译错误

**解决方案:**
- 检查CMake是否找到所有依赖
- 确认OpenCV版本>=4.5
- 在CMakeLists.txt中设置正确的TensorRT路径

### 问题4: 运行时CUDA错误

**解决方案:**
- 使用`nvidia-smi`检查GPU状态
- 确保没有其他程序占用GPU
- 检查显存是否足够

## 进阶功能

### 1. 多GPU支持

```cpp
// 指定GPU设备
cudaSetDevice(1);  // 使用第二个GPU
YOLOTensorRT detector("model.engine");
```

### 2. 动态batch处理

```cpp
std::vector<cv::Mat> images = {...};
auto results = detector.detectBatch(images);
```

### 3. 自定义NMS

修改`yolo_tensorrt.cpp`中的`nms()`函数实现自定义NMS策略。

## 类别配置

修改`test_yolo_tensorrt.cpp`中的类别名称以匹配你的数据集:

```cpp
std::vector<std::string> class_names = {
    "class1", "class2", "class3", // 你的类别
};
detector.setClassNames(class_names);
```

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题，请提交Issue或联系开发团队。

## 更新日志

### v1.0.0 (2026-01-16)
- 初始版本
- 支持RTX 3080Ti和4080S优化
- FP16精度支持
- 完整的C++推理引擎
- 图像、视频、基准测试功能
