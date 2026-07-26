# TensorRT Python安装指南

<!-- HISTORICAL_TENSORRT_PROTOTYPE -->
> ⚠️ 历史原型资料：仅保留用于追溯，不是当前构建、性能或交付证据。当前项目状态与验证范围以根目录 [README](../../README.md) 为准。

## 问题

ONNX导出成功 ✓
TensorRT导出失败 ✗ - 缺少tensorrt模块

## 解决方案

需要安装TensorRT的Python绑定。

### 方法1: 使用pip安装（推荐）

```powershell
# 在yolo26环境中安装
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe -m pip install tensorrt

# 如果上面的命令失败，使用NVIDIA官方源
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe -m pip install nvidia-tensorrt
```

### 方法2: 从NVIDIA网站下载

如果pip安装失败：

1. 下载TensorRT: https://developer.nvidia.com/tensorrt-download
2. 选择TensorRT 8.6.x for CUDA 11.8
3. 下载后解压
4. 安装Python wheel:

```powershell
cd <TensorRT解压目录>\python

# 安装对应Python 3.10的wheel
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe -m pip install tensorrt-8.6.*-cp310-*.whl
```

### 方法3: 仅使用ONNX（临时方案）

如果TensorRT安装困难，可以先使用已导出的ONNX模型：

**ONNX模型位置:**
```
e:\code\yolo26\yanzhi20260115.onnx (36.6 MB)
```

ONNX可以用于：
- C++部署（使用ONNX Runtime）
- Python推理（速度稍慢但可用）
- 后续转换为TensorRT

---

## 快速执行

```powershell
# 尝试安装TensorRT
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe -m pip install nvidia-tensorrt

# 安装完成后重新运行导出
cd e:\code\yolo26\expert_test
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe export_tensorrt.py
```

---

## 验证安装

```powershell
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe -c "import tensorrt; print('TensorRT版本:', tensorrt.__version__)"
```

---

## 备选方案：使用trtexec转换ONNX

如果Python TensorRT安装困难，可以使用TensorRT自带的命令行工具：

```cmd
# 找到trtexec.exe（通常在TensorRT安装目录的bin文件夹）
cd "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\bin"

# 转换ONNX为TensorRT引擎
trtexec.exe ^
  --onnx=e:\code\yolo26\yanzhi20260115.onnx ^
  --saveEngine=e:\code\yolo26\expert_test\models\yanzhi20260115_4080s_fp16.engine ^
  --fp16 ^
  --workspace=12288 ^
  --verbose
```

这样也能生成TensorRT引擎文件！
