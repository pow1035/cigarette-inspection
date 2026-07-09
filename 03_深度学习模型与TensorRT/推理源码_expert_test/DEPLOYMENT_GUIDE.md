# YOLO26 TensorRT 部署完整指南

## 📁 已生成文件清单

### Python导出脚本
1. **export_tensorrt.py** - 完整导出脚本(同时导出3080Ti和4080S模型)
2. **quick_export_4080s.py** - 快速导出脚本(仅导出4080S模型)
3. **check_environment.bat** - Windows环境检查脚本

### C++推理引擎
4. **yolo_tensorrt.h** - TensorRT推理引擎头文件
5. **yolo_tensorrt.cpp** - TensorRT推理引擎实现
6. **test_yolo_tensorrt.cpp** - 测试程序(支持图像/视频/性能测试)

### 构建配置
7. **CMakeLists.txt** - CMake构建配置
8. **build_and_test.bat** - Windows一键部署脚本
9. **build_and_test.sh** - Linux一键部署脚本

### 文档
10. **README.md** - 完整使用文档
11. **DEPLOYMENT_GUIDE.md** - 本文件(快速部署指南)

---

## 🚀 快速部署步骤(4080S)

### 前提条件

确保已安装:
- ✅ NVIDIA驱动 (最新版本)
- ✅ CUDA Toolkit 11.8+
- ✅ TensorRT 8.6+
- ✅ Python 3.8+ (with PyTorch, ultralytics)
- ✅ Visual Studio 2022 (C++开发工具)
- ✅ CMake 3.18+
- ✅ OpenCV 4.5+ (C++版本)

### 步骤1: 导出TensorRT引擎

**使用Anaconda(推荐):**

```cmd
# 激活你的conda环境
conda activate your_env

# 进入expert_test目录
cd expert_test

# 运行快速导出脚本(仅4080S)
python quick_export_4080s.py
```

**或使用完整导出脚本(同时导出3080Ti和4080S):**

```cmd
python export_tensorrt.py
```

**预期输出:**
- `models/yanzhi20260115.onnx` (~40MB)
- `models/yanzhi20260115_4080s_fp16.engine` (~20-30MB)

**注意:** TensorRT引擎构建可能需要5-10分钟

### 步骤2: 配置CMake路径

编辑 `CMakeLists.txt` 第18行,设置正确的TensorRT路径:

```cmake
set(TENSORRT_ROOT "C:/Program Files/NVIDIA GPU Computing Toolkit/TensorRT" CACHE PATH "TensorRT安装路径")
```

如果TensorRT安装在其他位置,修改为实际路径。

### 步骤3: 编译C++代码

```cmd
# 创建构建目录
mkdir build
cd build

# 配置CMake (Visual Studio 2022)
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 编译 (Release模式)
cmake --build . --config Release
```

**预期输出:**
- `build/bin/Release/test_yolo_tensorrt.exe`

### 步骤4: 运行测试

**性能基准测试:**
```cmd
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench
```

**图像推理测试:**
```cmd
# 准备测试图像(替换为你的图像路径)
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine image test.jpg result.jpg
```

**视频推理测试:**
```cmd
# 准备测试视频(替换为你的视频路径)
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine video test.mp4 result.mp4
```

---

## 🔧 常见问题解决

### 问题1: Python命令失败(exit code 49)

**原因:** Windows应用商店的Python占位符导致

**解决方案:**
```cmd
# 方法1: 使用完整路径
C:\Users\Administrator\anaconda3\python.exe quick_export_4080s.py

# 方法2: 使用conda
conda activate base
python quick_export_4080s.py

# 方法3: 禁用应用商店Python
# 设置 -> 应用 -> 应用执行别名 -> 关闭Python应用安装程序
```

### 问题2: 找不到TensorRT

**检查方法:**
```cmd
dir "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT"
```

**如果不存在:**
1. 下载TensorRT: https://developer.nvidia.com/tensorrt
2. 解压到指定位置
3. 修改CMakeLists.txt中的TENSORRT_ROOT路径

### 问题3: 找不到OpenCV

**Windows安装:**
```cmd
# 方法1: vcpkg (推荐)
vcpkg install opencv[cuda]:x64-windows

# 方法2: 下载预编译版本
# 从 https://opencv.org/releases/ 下载
# 设置环境变量: OpenCV_DIR=C:\opencv\build
```

**CMake配置:**
```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR="C:\opencv\build"
```

### 问题4: CUDA内存不足

**解决方案:**
- 减小workspace: 修改`export_tensorrt.py`中的`workspace=8`(从12改为8)
- 关闭其他占用GPU的程序
- 使用FP32而不是FP16(更大内存但可能更稳定)

### 问题5: 编译错误 - C++标准不支持

**解决方案:**
确保使用Visual Studio 2019或更新版本,或者修改CMakeLists.txt:
```cmake
set(CMAKE_CXX_STANDARD 17)  # 或改为 14
```

---

## 📊 预期性能指标

### RTX 4080S (640x640, FP16)

| 指标 | 值 |
|------|-----|
| 推理时间 | 2-3ms |
| 预处理时间 | 1-2ms |
| 后处理时间 | 1-2ms |
| 总时间 | 5-8ms |
| 吞吐量 | 125-200 FPS |

*注: 实际性能依赖于模型复杂度和输入数据*

---

## 🔄 集成到其他项目

### 最小化集成文件

仅需复制:
1. `yolo_tensorrt.h`
2. `yolo_tensorrt.cpp`
3. `models/yanzhi20260115_4080s_fp16.engine`

### 示例代码

```cpp
#include "yolo_tensorrt.h"

int main() {
    // 初始化检测器
    YOLOTensorRT detector("yanzhi20260115_4080s_fp16.engine", 640, 0.25f, 0.45f);

    if (!detector.initialize()) {
        return -1;
    }

    // 加载图像
    cv::Mat image = cv::imread("test.jpg");

    // 推理
    auto detections = detector.detect(image);

    // 处理结果
    for (const auto& det : detections) {
        std::cout << det.class_name << ": " << det.confidence << std::endl;
        cv::rectangle(image, det.box, cv::Scalar(0, 255, 0), 2);
    }

    // 保存结果
    cv::imwrite("result.jpg", image);

    // 打印性能统计
    detector.printPerformanceStats();

    return 0;
}
```

### CMake集成

```cmake
# 添加子目录
add_subdirectory(expert_test)

# 链接库
target_link_libraries(your_app yolo_tensorrt)
```

---

## 📝 类别配置

修改`test_yolo_tensorrt.cpp`中的类别名称列表以匹配你的模型:

```cpp
std::vector<std::string> class_names = {
    // 根据mydata.yaml中的类别填写
    "class1",
    "class2",
    "class3",
    // ...
};
detector.setClassNames(class_names);
```

查看训练配置文件`mydata.yaml`获取正确的类别列表。

---

## 🎯 下一步优化建议

1. **多GPU支持**: 添加GPU设备选择
2. **批处理优化**: 支持动态batch size
3. **INT8量化**: 进一步提升速度(需要校准数据)
4. **异步推理**: 使用多CUDA流并行处理
5. **模型集成**: 集成跟踪、分割等功能

---

## 📞 技术支持

如遇到问题:
1. 检查`README.md`中的详细说明
2. 运行`check_environment.bat`诊断环境
3. 查看CMake和编译日志
4. 检查CUDA和TensorRT版本兼容性

---

## 版本信息

- 文档版本: v1.0
- 创建日期: 2026-01-16
- 支持GPU: RTX 3080Ti, RTX 4080S
- 支持系统: Windows 10/11, Linux (Ubuntu 20.04+)
