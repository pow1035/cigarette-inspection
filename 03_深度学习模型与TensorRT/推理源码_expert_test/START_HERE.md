# 🚀 立即开始 - 完整执行步骤

你的环境已经检查完成，现在按以下步骤执行：

## 📋 当前状态

✅ Python: 3.10.19 (anaconda3/envs/yolo26)
✅ PyTorch: 2.1.0+cu118
✅ CUDA: 可用
✅ GPU: RTX 4080 SUPER
❌ Ultralytics: **未安装** (需要安装)

---

## 步骤1: 安装ultralytics (1分钟)

在PowerShell中运行：

```powershell
# 激活环境
conda activate yolo26

# 安装ultralytics
pip install ultralytics

# 验证安装
python -c "from ultralytics import YOLO; print('安装成功')"
```

---

## 步骤2: 导出TensorRT模型 (5-10分钟)

安装完成后，运行导出脚本：

```powershell
# 确保在项目根目录或expert_test目录
cd e:\code\yolo26\expert_test

# 运行导出脚本
python export_tensorrt.py
```

**这个脚本会自动：**
1. 检查环境
2. 导出ONNX模型 (992x992分辨率)
3. 为3080Ti导出TensorRT引擎
4. 为4080S导出TensorRT引擎
5. 运行性能测试

**预期输出：**
```
models/yanzhi20260115.onnx
models/yanzhi20260115_3080ti_fp16.engine
models/yanzhi20260115_4080s_fp16.engine
```

---

## 步骤3: 编译C++代码 (2-5分钟)

### 前置要求检查

1. **Visual Studio 2022** - 已安装?
2. **CMake** - 已安装? (运行 `cmake --version` 检查)
3. **TensorRT** - 安装位置? (默认: `C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT`)
4. **OpenCV** - 已安装?

### 编译步骤

打开 **Visual Studio Developer Command Prompt** 或 **x64 Native Tools Command Prompt**:

```cmd
cd e:\code\yolo26\expert_test
mkdir build
cd build

# 配置 (如果TensorRT路径不同，修改-DTENSORRT_ROOT参数)
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DTENSORRT_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT"

# 编译
cmake --build . --config Release
```

**成功后会生成：**
```
build\bin\Release\test_yolo_tensorrt.exe
```

---

## 步骤4: 测试运行

```cmd
# 性能测试
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench

# 图像测试 (准备一张test.jpg)
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine image test.jpg result.jpg
```

---

## 🔧 如果遇到问题

### 问题1: TensorRT未安装

**下载：** https://developer.nvidia.com/tensorrt
- 选择与CUDA 11.8兼容的版本 (TensorRT 8.6.x)
- 解压到系统路径
- 修改CMakeLists.txt中的路径

### 问题2: OpenCV未安装

**Windows快速安装：**

```powershell
# 使用vcpkg
vcpkg install opencv:x64-windows

# 或下载预编译版本
# https://opencv.org/releases/
```

然后在CMake配置时指定OpenCV路径：
```cmd
cmake .. -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR="C:\opencv\build"
```

### 问题3: 编译错误

查看详细文档：
- `README.md` - 完整文档
- `DEPLOYMENT_GUIDE.md` - 部署指南
- `TROUBLESHOOTING.md` - 故障排除

---

## 📊 关于992分辨率

你修改的输入尺寸为992x992 (原来是640x640)。

**影响：**
- ✅ 检测精度可能提升
- ⚠️ 推理速度降低约1.5-2倍
- ⚠️ 显存占用增加

**预期性能 (RTX 4080 SUPER):**
- 640x640: ~2-3ms 推理, 125-200 FPS
- 992x992: ~4-6ms 推理, 80-120 FPS

**C++代码调整：**

test_yolo_tensorrt.cpp中默认使用640，如果要匹配992，需要修改：

```cpp
// 第160行附近
YOLOTensorRT detector(engine_path, 992, 0.25f, 0.45f);  // 改为992
```

---

## ✅ 完整命令总结

```powershell
# 1. 安装依赖
conda activate yolo26
pip install ultralytics

# 2. 导出模型
cd e:\code\yolo26\expert_test
python export_tensorrt.py

# 3. 编译C++ (在VS Developer Command Prompt中)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

# 4. 测试
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench
```

---

## 🎯 快速验证

验证每个步骤：

```powershell
# 验证ultralytics安装
python -c "from ultralytics import YOLO; print('OK')"

# 验证模型文件
ls ..\yanzhi20260115.pt

# 验证导出结果
ls models\

# 验证C++编译
ls build\bin\Release\test_yolo_tensorrt.exe
```

---

现在开始第一步：**安装ultralytics**

```powershell
conda activate yolo26
pip install ultralytics
```

安装完成后告诉我，我们继续下一步！
