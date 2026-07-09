# TensorRT转换 - 最终方案

## 当前情况

✅ **ONNX已导出成功**: `e:\code\yolo26\yanzhi20260115.onnx` (36.6 MB)
❌ **Python TensorRT安装失败**: pip安装有问题

## 🎯 推荐方案：使用trtexec工具

TensorRT自带命令行工具`trtexec`，可以直接转换ONNX为TensorRT引擎，无需Python绑定。

### 方法1: 自动转换（最简单）

运行批处理文件：

```cmd
cd e:\code\yolo26\expert_test
convert_onnx_to_trt.bat
```

这个脚本会：
1. 检查trtexec是否存在
2. 检查ONNX文件
3. 转换为TensorRT引擎（FP16, 992x992）
4. 保存到`models/yanzhi20260115_4080s_fp16.engine`

### 方法2: 手动转换

如果批处理文件中的TensorRT路径不对，手动运行：

```cmd
# 找到trtexec.exe（通常在这些位置之一）
# C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\bin\trtexec.exe
# 或你安装TensorRT的位置

cd "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\bin"

# 转换ONNX为TensorRT（4080S, FP16）
trtexec.exe ^
  --onnx=e:\code\yolo26\yanzhi20260115.onnx ^
  --saveEngine=e:\code\yolo26\expert_test\models\yanzhi20260115_4080s_fp16.engine ^
  --fp16 ^
  --workspace=12288 ^
  --verbose
```

### 参数说明

- `--onnx`: 输入的ONNX文件
- `--saveEngine`: 输出的TensorRT引擎文件
- `--fp16`: 使用FP16精度
- `--workspace=12288`: 工作空间12GB（12288MB）
- `--verbose`: 显示详细信息

### 预期输出

转换成功后会生成：
```
e:\code\yolo26\expert_test\models\yanzhi20260115_4080s_fp16.engine
```

文件大小约30-50MB

---

## 备选方案：跳过TensorRT，使用ONNX

如果TensorRT安装困难，可以直接使用ONNX模型：

### C++部署选项

1. **ONNX Runtime** - 使用ONNX模型（速度稍慢但简单）
2. **TensorRT手动加载** - 稍后有TensorRT再转换

ONNX性能对比：
- TensorRT FP16: ~4-6ms (推荐)
- ONNX Runtime: ~10-15ms (可接受)

---

## 验证TensorRT安装位置

检查这些常见位置：

```cmd
dir "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\bin\trtexec.exe"
dir "C:\TensorRT\bin\trtexec.exe"
dir "C:\Program Files\TensorRT\bin\trtexec.exe"
```

或搜索：
```cmd
where /r "C:\Program Files" trtexec.exe
```

---

## 下一步

1. **找到trtexec.exe位置**
2. **运行转换脚本**或手动转换
3. **验证生成的.engine文件**
4. **编译C++代码**
5. **测试推理**

如果trtexec也找不到，说明需要下载安装TensorRT：
- 下载地址: https://developer.nvidia.com/tensorrt
- 选择TensorRT 8.6.x for Windows CUDA 11.8
- 解压到系统路径
