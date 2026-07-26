# ⚠️ 导出脚本错误解决方案

<!-- HISTORICAL_TENSORRT_PROTOTYPE -->
> ⚠️ 历史原型资料：仅保留用于追溯，不是当前构建、性能或交付证据。当前项目状态与验证范围以根目录 [README](../../README.md) 为准。

## 问题: quick_export_4080s.py 报错

### 原因分析

Windows系统的Python命令可能被应用商店占位符拦截,导致exit code 49错误。

---

## ✅ 解决方案(3种方法)

### 方法1: 使用新的诊断脚本(推荐)

我创建了一个更健壮的脚本,包含完整的错误诊断。

**直接运行批处理文件:**

```cmd
cd expert_test
run_export.bat
```

这个脚本会:
1. 自动检测可用的Python环境
2. 诊断所有依赖项
3. 询问确认后开始导出
4. 提供详细的错误信息

**或者手动运行Python脚本:**

```cmd
# 如果你已经在正确的conda环境中
python diagnose_and_export.py
```

---

### 方法2: 使用Anaconda Prompt

这是最可靠的方法:

1. **打开 Anaconda Prompt** (不是普通的CMD)

2. **激活环境:**
   ```cmd
   conda activate your_env
   ```

3. **进入目录:**
   ```cmd
   cd e:\code\yolo26\expert_test
   ```

4. **运行脚本:**
   ```cmd
   python quick_export_4080s.py
   ```

---

### 方法3: 使用完整的Python路径

找到你的Python安装路径,直接使用:

```cmd
# 示例(替换为你的实际路径)
C:\Users\Administrator\anaconda3\python.exe quick_export_4080s.py

# 或者,如果在特定环境中
C:\Users\Administrator\anaconda3\envs\your_env\python.exe quick_export_4080s.py
```

---

## 🔍 诊断步骤

如果上述方法都不行,按以下步骤诊断:

### 1. 检查Python是否可用

```cmd
python --version
```

如果报错或显示Microsoft Store,说明需要:
- 方法A: 使用Anaconda Prompt
- 方法B: 禁用应用商店Python别名
  - 设置 → 应用 → 应用执行别名
  - 关闭 "应用安装程序 python.exe"

### 2. 检查conda环境

```cmd
conda env list
```

找到包含PyTorch的环境,然后激活它。

### 3. 验证依赖

在正确的环境中运行:

```cmd
python -c "import torch; print(torch.__version__)"
python -c "from ultralytics import YOLO; print('OK')"
```

如果任何一个失败,安装相应的包:

```cmd
pip install torch torchvision ultralytics opencv-python
```

---

## 📋 推荐的完整流程

**最简单可靠的方法:**

1. **打开 Anaconda Prompt**

2. **运行以下命令:**
   ```cmd
   cd e:\code\yolo26\expert_test
   conda activate base
   python diagnose_and_export.py
   ```

3. **按提示操作:**
   - 检查环境信息
   - 输入 'y' 确认开始导出
   - 等待5-10分钟

---

## 📊 导出参数说明

脚本已根据你的修改使用 `imgsz=992`:

```python
# ONNX导出
model.export(
    format='onnx',
    imgsz=992,      # 你设置的输入尺寸
    opset=11,
    simplify=True,
    dynamic=False,
    device=0
)

# TensorRT导出
model.export(
    format='engine',
    imgsz=992,      # 与ONNX保持一致
    half=True,      # FP16精度
    device=0,
    workspace=12,   # 4080S有16GB显存
    verbose=True
)
```

### 关于 imgsz=992

- ✅ 更高的分辨率,可能提升检测精度
- ⚠️ 推理时间会增加(约1.5-2倍)
- ⚠️ 需要更多显存

**预期性能(992x992 vs 640x640):**
- 640x640: ~2-3ms, 125-200 FPS
- 992x992: ~4-6ms, 80-120 FPS

如果性能不满足需求,可以改回640。

---

## 🛠️ 如果还是失败

### 检查TensorRT

TensorRT导出可能失败的原因:

1. **TensorRT未安装**
   - 下载: https://developer.nvidia.com/tensorrt
   - 解压到系统路径

2. **版本不兼容**
   - CUDA 11.8 → TensorRT 8.6.x
   - CUDA 12.x → TensorRT 9.x

3. **显存不足**
   - 减小workspace: `workspace=8` 或 `workspace=4`
   - 关闭其他GPU程序

### 仅导出ONNX

如果TensorRT问题无法解决,可以先导出ONNX:

```python
from ultralytics import YOLO

model = YOLO("../yanzhi20260115.pt")
model.export(format='onnx', imgsz=992, opset=11, simplify=True)
```

ONNX也可以在C++中使用(通过ONNX Runtime),虽然速度稍慢。

---

## 📞 需要帮助?

1. 运行 `diagnose_and_export.py` 查看完整的错误信息
2. 检查CUDA和GPU驱动: `nvidia-smi`
3. 检查Python环境: `conda list | grep torch`

---

## ✅ 成功标志

导出成功后会看到:

```
✓ ONNX导出成功: models/yanzhi20260115.onnx
  大小: ~60-80 MB (992x992比640x640更大)

✓ TensorRT引擎导出成功: models/yanzhi20260115_4080s_fp16.engine
  大小: ~30-50 MB
```

然后就可以继续编译C++代码了!
