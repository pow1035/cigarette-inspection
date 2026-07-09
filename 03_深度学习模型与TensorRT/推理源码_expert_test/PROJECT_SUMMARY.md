# YOLO26 TensorRT C++ 部署项目总结

## 📦 项目概述

本项目为YOLO26模型(`yanzhi20260115.pt`)提供完整的TensorRT优化和C++部署方案,特别针对RTX 3080Ti和RTX 4080S显卡进行了优化。

## 📂 文件清单

### expert_test/ 目录下的所有文件

| 文件名 | 类型 | 说明 |
|--------|------|------|
| **manual_export.py** | Python | 🔥 推荐使用的手动导出脚本 |
| export_tensorrt.py | Python | 完整导出脚本(3080Ti+4080S) |
| quick_export_4080s.py | Python | 快速导出脚本(仅4080S) |
| check_environment.bat | Batch | Windows环境检查工具 |
| **yolo_tensorrt.h** | C++ | TensorRT推理引擎头文件 |
| **yolo_tensorrt.cpp** | C++ | TensorRT推理引擎实现 |
| **test_yolo_tensorrt.cpp** | C++ | 测试程序(图像/视频/benchmark) |
| **CMakeLists.txt** | CMake | 构建配置文件 |
| build_and_test.bat | Batch | Windows一键部署脚本 |
| build_and_test.sh | Shell | Linux一键部署脚本 |
| **README.md** | Markdown | 完整使用文档 |
| **DEPLOYMENT_GUIDE.md** | Markdown | 快速部署指南 |
| PROJECT_SUMMARY.md | Markdown | 本文件(项目总结) |

## 🚀 快速开始(3步部署)

### 步骤1: 导出TensorRT模型

**在Anaconda Prompt或配置好的终端中运行:**

```cmd
cd expert_test
conda activate your_env  # 激活包含PyTorch和ultralytics的环境
python manual_export.py
```

**输出文件:**
- `models/yanzhi20260115.onnx` (~40MB)
- `models/yanzhi20260115_4080s_fp16.engine` (或`_3080ti_fp16.engine`, ~20-30MB)

**预计耗时:** 5-10分钟

### 步骤2: 编译C++代码

**在Visual Studio命令提示符或终端中:**

```cmd
cd expert_test
mkdir build
cd build

# 配置(修改TensorRT路径如果需要)
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build . --config Release
```

**输出文件:**
- `build/bin/Release/test_yolo_tensorrt.exe`

**预计耗时:** 2-5分钟

### 步骤3: 运行测试

```cmd
# 性能基准测试
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench

# 图像测试(需要准备test.jpg)
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine image test.jpg result.jpg
```

## 💡 核心特性

### Python导出脚本
- ✅ 支持ONNX中间格式导出
- ✅ 针对3080Ti和4080S分别优化
- ✅ FP16精度(精度损失<1%,速度提升2-3倍)
- ✅ 自动检测GPU并选择最佳配置
- ✅ 集成性能基准测试

### C++推理引擎
- ✅ 高性能TensorRT推理
- ✅ 异步CUDA流
- ✅ 自动NMS后处理
- ✅ 支持图像和视频推理
- ✅ 详细性能统计
- ✅ 易于集成到其他项目

### 性能优化
- ✅ GPU特定优化(SM86用于3080Ti, SM89用于4080S)
- ✅ 大工作空间(8-12GB)
- ✅ 固定输入尺寸避免动态shape开销
- ✅ 简化的ONNX图
- ✅ 批处理支持(可选)

## 📊 预期性能

| GPU | 推理时间 | 总时间 | 吞吐量 |
|-----|----------|--------|--------|
| **RTX 4080S** | 2-3ms | 5-8ms | 125-200 FPS |
| **RTX 3080Ti** | 3-4ms | 6-10ms | 100-160 FPS |

*基于640x640输入,FP16精度*

## 🔧 环境要求

### 必需软件
- [x] NVIDIA GPU (RTX 3080Ti / 4080S)
- [x] NVIDIA驱动 (最新)
- [x] CUDA Toolkit 11.8+
- [x] TensorRT 8.6+
- [x] Python 3.8+ (PyTorch 2.0+, ultralytics)
- [x] Visual Studio 2022 或 GCC 9+
- [x] CMake 3.18+
- [x] OpenCV 4.5+

### 可选软件
- [ ] OpenCV with CUDA (更快的预处理)
- [ ] cuDNN (TensorRT依赖,通常随CUDA安装)

## 📖 文档说明

1. **README.md** - 最完整的文档
   - 详细安装说明
   - API参考
   - 故障排除
   - 进阶功能

2. **DEPLOYMENT_GUIDE.md** - 快速部署指南
   - 3步快速部署
   - 常见问题解决
   - 性能指标
   - 集成示例

3. **PROJECT_SUMMARY.md** (本文件) - 项目总结
   - 文件清单
   - 快速开始
   - 工作流程

## 🔄 典型工作流程

```
yanzhi20260115.pt (PyTorch模型)
         ↓
  [manual_export.py]
         ↓
    ├─→ yanzhi20260115.onnx (ONNX中间格式)
    │
    └─→ yanzhi20260115_4080s_fp16.engine (TensorRT引擎)
             ↓
     [CMake + 编译]
             ↓
      test_yolo_tensorrt.exe (C++推理程序)
             ↓
      [运行测试/集成]
```

## 💼 集成到其他项目

### 方式1: 作为子项目

```cmake
# 你的CMakeLists.txt
add_subdirectory(expert_test)
target_link_libraries(your_app yolo_tensorrt)
```

### 方式2: 复制核心文件

最小化集成只需:
1. `yolo_tensorrt.h`
2. `yolo_tensorrt.cpp`
3. `models/*.engine`

### 示例代码

```cpp
#include "yolo_tensorrt.h"

int main() {
    YOLOTensorRT detector("model.engine", 640, 0.25f, 0.45f);
    detector.initialize();

    cv::Mat image = cv::imread("test.jpg");
    auto detections = detector.detect(image);

    for (const auto& det : detections) {
        std::cout << det.class_name << ": " << det.confidence << std::endl;
    }

    detector.printPerformanceStats();
    return 0;
}
```

## ⚠️ 常见问题

### Python环境问题
**症状:** `python`命令失败(exit code 49)

**解决:**
```cmd
# 使用conda
conda activate your_env
python manual_export.py

# 或使用完整路径
C:\Users\Administrator\anaconda3\python.exe manual_export.py
```

### TensorRT路径问题
**症状:** CMake找不到TensorRT

**解决:** 编辑`CMakeLists.txt`第18行,设置正确路径:
```cmake
set(TENSORRT_ROOT "你的TensorRT路径" CACHE PATH "TensorRT安装路径")
```

### 显存不足
**症状:** TensorRT构建失败,OOM错误

**解决:** 减小workspace大小,修改`manual_export.py`:
```python
workspace = 4  # 从8或12改为4
```

## 🎯 优化建议

### 进一步提升性能
1. **INT8量化** - 速度再提升2倍(需校准数据)
2. **多流并行** - 提高吞吐量
3. **批处理** - 处理多张图像
4. **CUDA预处理** - 使用OpenCV CUDA模块

### 调整精度
- **FP32**: 最高精度,速度慢
- **FP16**: 推荐,平衡精度和速度
- **INT8**: 最快,需要校准

### 调整输入尺寸
- **320x320**: 最快
- **640x640**: 推荐
- **1280x1280**: 最高精度

## 📞 获取帮助

1. 查看文档: `README.md`, `DEPLOYMENT_GUIDE.md`
2. 运行环境检查: `check_environment.bat`
3. 检查日志: CMake配置日志,编译日志
4. 验证CUDA: `nvidia-smi`, `nvcc --version`

## ✅ 验证清单

部署完成后,确认以下文件存在:

- [ ] `models/yanzhi20260115.onnx`
- [ ] `models/yanzhi20260115_4080s_fp16.engine` (或`_3080ti_fp16.engine`)
- [ ] `build/bin/Release/test_yolo_tensorrt.exe`

并能成功运行:
```cmd
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench
```

## 🎓 技术细节

### TensorRT优化策略
- 层融合(Layer Fusion)
- 精度校准(FP16/INT8)
- 内核自动调优(Kernel Auto-Tuning)
- 显存优化(Memory Optimization)

### C++实现特点
- RAII资源管理
- 异步CUDA流
- 高效内存布局(CHW格式)
- 优化的NMS算法

### 支持的功能
- [x] 单张图像推理
- [x] 批量推理(可扩展)
- [x] 视频处理
- [x] 性能基准测试
- [x] 自定义类别名称
- [x] 可配置阈值

## 📅 版本历史

**v1.0.0** (2026-01-16)
- 初始发布
- 支持RTX 3080Ti和4080S
- 完整的导出和部署流程
- 详细文档

## 📄 许可

本项目仅供学习和研究使用。

---

**祝你部署成功! 🎉**

如有问题,请仔细阅读README.md和DEPLOYMENT_GUIDE.md,或检查环境配置。
