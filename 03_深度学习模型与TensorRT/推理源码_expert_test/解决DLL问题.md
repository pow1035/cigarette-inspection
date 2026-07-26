# 解决cudart64_110.dll缺失问题

<!-- HISTORICAL_TENSORRT_PROTOTYPE -->
> ⚠️ 历史原型资料：仅保留用于追溯，不是当前构建、性能或交付证据。当前项目状态与验证范围以根目录 [README](../../README.md) 为准。

## 问题
TensorRT需要CUDA 11.0的运行时库`cudart64_110.dll`，但系统中没有安装。

## 解决方案

### 方案1: 使用Python脚本（推荐）

```cmd
cd e:\code\yolo26\expert_test
C:\Users\Administrator\anaconda3\envs\yolo26\python.exe convert_with_python.py
```

### 方案2: 复制CUDA库文件

从conda环境复制CUDA DLL到TensorRT bin目录：

```cmd
copy "C:\Users\Administrator\anaconda3\envs\yolo26\Library\bin\cudart64_*.dll" "D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin\"
```

然后再运行`convert_to_trt.bat`

### 方案3: 安装CUDA Toolkit 11.8

下载并安装完整的CUDA Toolkit:
- https://developer.nvidia.com/cuda-11-8-0-download-archive
- 选择Windows x86_64版本
- 安装后会自动设置环境变量

### 方案4: 手动下载DLL（临时方案）

1. 从这里下载cudart64_110.dll
2. 放到以下任一位置：
   - `D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin\`
   - `C:\Windows\System32\`
   - 当前工作目录

### 方案5: 跳过TensorRT，使用ONNX（快速测试）

ONNX模型已经导出成功，可以直接用于测试：
- 使用ONNX Runtime进行C++推理
- 性能稍慢但功能完整
- 适合快速验证流程

## 推荐执行顺序

1. **先试方案1** (Python脚本)
2. **如果失败，试方案2** (复制DLL)
3. **如果还是不行，考虑方案5** (使用ONNX)

告诉我哪个方案有效，或者遇到什么错误！
