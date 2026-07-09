# YOLO26 ONNX Runtime C++ 部署指南

## 概述

这是使用ONNX Runtime的快速部署方案，避免了TensorRT的环境配置问题。

**优点：**
- 立即可用，无需复杂的依赖配置
- 功能完整，支持GPU加速
- 性能良好（10-15ms，约67-100 FPS）
- 后续可以升级到TensorRT

**性能对比：**
- TensorRT FP16: ~4-6ms (理想，需要解决环境问题)
- ONNX Runtime: ~10-15ms (当前方案，已足够实用)

---

## 前置要求

### 1. 已有资源
- ✅ ONNX模型：`e:\code\yolo26\yanzhi20260115.onnx` (36.6 MB, 992x992)
- ✅ RTX 4080S GPU
- ✅ CUDA 11.8 已安装

### 2. 需要下载安装

#### ONNX Runtime (GPU版本)
1. 下载地址：https://github.com/microsoft/onnxruntime/releases
2. 选择：`onnxruntime-win-x64-gpu-1.17.0.zip` (或最新版本)
3. 解压到：`C:\Program Files\onnxruntime`

或者使用以下命令下载：
```cmd
REM 创建目录
mkdir "C:\Program Files\onnxruntime"
cd "C:\Program Files\onnxruntime"

REM 使用PowerShell下载（版本1.17.0）
powershell -Command "Invoke-WebRequest -Uri 'https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-win-x64-gpu-1.17.0.zip' -OutFile 'onnxruntime.zip'"
powershell -Command "Expand-Archive -Path 'onnxruntime.zip' -DestinationPath '.'"
```

**目录结构应该是：**
```
C:\Program Files\onnxruntime\
├── include\
│   ├── onnxruntime_c_api.h
│   ├── onnxruntime_cxx_api.h
│   └── ...
└── lib\
    ├── onnxruntime.dll
    ├── onnxruntime.lib
    ├── onnxruntime_providers_cuda.dll
    └── onnxruntime_providers_shared.dll
```

#### OpenCV (如果还没安装)
1. 下载地址：https://opencv.org/releases/
2. 选择：Windows版本（例如4.8.0）
3. 安装到：`C:\opencv`

#### CMake
1. 下载地址：https://cmake.org/download/
2. 安装时选择"Add CMake to system PATH"

#### Visual Studio 2019/2022
需要安装C++开发工具和CMake工具。

---

## 编译步骤

### 方法1：使用提供的批处理脚本

创建 `build_onnx.bat`:

```batch
@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

REM 设置路径（根据实际情况修改）
set ONNXRUNTIME_ROOT=C:\Program Files\onnxruntime\onnxruntime-win-x64-gpu-1.17.0
set OPENCV_DIR=C:\opencv\build
set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8

REM 创建构建目录
if not exist build_onnx mkdir build_onnx
cd build_onnx

REM 运行CMake配置
echo ========================================
echo Configuring CMake...
echo ========================================
cmake .. ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DONNXRUNTIME_ROOT="%ONNXRUNTIME_ROOT%" ^
  -DOpenCV_DIR="%OPENCV_DIR%" ^
  -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM 编译
echo.
echo ========================================
echo Building...
echo ========================================
cmake --build . --config Release

if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCESS!
echo ========================================
echo.
echo Executable: %CD%\Release\test_yolo_onnx.exe
echo.

cd ..
pause
```

然后运行：
```cmd
cd e:\code\yolo26\expert_test
rename CMakeLists_ONNX.txt CMakeLists.txt
build_onnx.bat
```

### 方法2：手动编译

```cmd
cd e:\code\yolo26\expert_test

REM 重命名CMakeLists文件
rename CMakeLists_ONNX.txt CMakeLists.txt

REM 创建构建目录
mkdir build_onnx
cd build_onnx

REM 配置（根据实际路径修改）
cmake .. ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DONNXRUNTIME_ROOT="C:\Program Files\onnxruntime\onnxruntime-win-x64-gpu-1.17.0" ^
  -DOpenCV_DIR="C:\opencv\build"

REM 编译
cmake --build . --config Release

REM 可执行文件位于：
REM build_onnx\Release\test_yolo_onnx.exe
```

---

## 运行测试

### 1. 测试单张图片

```cmd
cd e:\code\yolo26\expert_test\build_onnx\Release

test_yolo_onnx.exe image ..\..\yanzhi20260115.onnx test.jpg
```

### 2. 测试视频

```cmd
test_yolo_onnx.exe video ..\..\yanzhi20260115.onnx test.mp4
```

### 3. 性能基准测试

```cmd
test_yolo_onnx.exe benchmark ..\..\yanzhi20260115.onnx test.jpg
```

这将运行100次推理并输出详细的性能统计：
- 最小/最大/平均延迟
- P95/P99延迟
- 吞吐量（FPS）

---

## 预期性能

在RTX 4080S上，992x992分辨率：

| 指标 | 预期值 |
|------|--------|
| 推理延迟 | 10-15ms |
| 吞吐量 | 67-100 FPS |
| 显存占用 | ~2-3 GB |

---

## 常见问题

### Q1: 找不到onnxruntime.dll
**解决：**
- 确保ONNX Runtime安装路径正确
- CMake会自动复制DLL到exe目录
- 或手动复制：`copy "C:\Program Files\onnxruntime\...\lib\*.dll" build_onnx\Release\`

### Q2: CUDA provider初始化失败
**解决：**
- 确认CUDA 11.8已正确安装
- 确认使用的是GPU版本的ONNX Runtime (带cuda后缀)
- 检查GPU驱动是否最新

### Q3: 性能比预期慢
**检查：**
- 是否使用了CUDA provider（应该在输出中看到"Using CUDA provider"）
- 是否编译为Release模式（Debug模式会慢很多）
- GPU是否被其他程序占用

### Q4: 编译错误：找不到onnxruntime头文件
**解决：**
- 检查ONNXRUNTIME_ROOT路径是否正确
- 确保include目录存在：`C:\Program Files\onnxruntime\...\include\onnxruntime_cxx_api.h`

---

## 在其他项目中使用

### 集成到你的项目

1. **复制文件到你的项目：**
   ```
   your_project/
   ├── yolo_onnx.h
   ├── yolo_onnx.cpp
   └── CMakeLists.txt (参考本项目的CMakeLists_ONNX.txt)
   ```

2. **在你的代码中使用：**
   ```cpp
   #include "yolo_onnx.h"

   // 创建检测器
   YOLOOnnx detector("path/to/model.onnx");

   // 加载图像
   cv::Mat image = cv::imread("test.jpg");

   // 检测
   auto detections = detector.detect(image);

   // 处理结果
   for (const auto& det : detections) {
       std::cout << "Class: " << det.class_id
                 << " Conf: " << det.confidence
                 << " Box: " << det.box << std::endl;
   }
   ```

3. **设置类别名称（可选）：**
   ```cpp
   std::vector<std::string> class_names = {
       "person", "car", "dog", // ...你的类别
   };
   detector.setClassNames(class_names);
   ```

---

## 后续优化

### 1. 升级到TensorRT（可选）
等有时间解决TensorRT环境配置问题后，可以获得更好的性能（4-6ms）。

### 2. 量化优化
如果性能仍不满足需求，可以尝试INT8量化：
- 需要校准数据集
- 可以进一步提升速度，轻微损失精度

### 3. 批处理推理
如果需要处理多张图片，可以修改代码支持批处理：
```cpp
// 修改input_shape_[0]为batch_size
// 一次处理多张图片
```

---

## 总结

这个ONNX Runtime方案可以让你：
1. ✅ 立即开始C++部署测试
2. ✅ 在RTX 4080S上验证模型性能
3. ✅ 获得可接受的推理速度（67-100 FPS）
4. ✅ 随时可以升级到TensorRT获得更好性能

**下一步：**
1. 下载并安装ONNX Runtime
2. 运行 build_onnx.bat 编译项目
3. 测试推理性能
4. 如果满意，就可以部署到生产环境了！

有任何问题随时告诉我！
