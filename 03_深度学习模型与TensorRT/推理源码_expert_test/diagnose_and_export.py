"""
诊断脚本 - 检查Python环境并尝试导出
"""
import sys
import os

print("=" * 60)
print("环境诊断")
print("=" * 60)

# 1. Python信息
print(f"\nPython路径: {sys.executable}")
print(f"Python版本: {sys.version}")

# 2. 检查PyTorch
try:
    import torch
    print(f"\n✓ PyTorch: {torch.__version__}")
    print(f"  CUDA可用: {torch.cuda.is_available()}")
    if torch.cuda.is_available():
        print(f"  CUDA版本: {torch.version.cuda}")
        print(f"  GPU: {torch.cuda.get_device_name(0)}")
        print(f"  显存: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")
except Exception as e:
    print(f"\n✗ PyTorch错误: {e}")
    sys.exit(1)

# 3. 检查ultralytics
try:
    from ultralytics import YOLO
    print(f"\n✓ Ultralytics导入成功")
except Exception as e:
    print(f"\n✗ Ultralytics错误: {e}")
    print("  请安装: pip install ultralytics")
    sys.exit(1)

# 4. 检查模型文件
from pathlib import Path
script_dir = Path(__file__).parent
model_path = script_dir.parent / "yanzhi20260115.pt"

if not model_path.exists():
    print(f"\n✗ 模型文件不存在: {model_path}")
    sys.exit(1)

print(f"\n✓ 模型文件存在: {model_path}")
print(f"  大小: {model_path.stat().st_size / 1024**2:.2f} MB")

# 转换为字符串用于YOLO
model_path = str(model_path)

# 5. 创建输出目录
os.makedirs("models", exist_ok=True)

# 6. 尝试加载模型
print("\n" + "=" * 60)
print("尝试加载模型")
print("=" * 60)

try:
    model = YOLO(model_path)
    print("✓ 模型加载成功")

    # 获取模型信息
    print(f"\n模型信息:")
    print(f"  任务: {model.task}")
    print(f"  类别数: {len(model.names) if hasattr(model, 'names') else 'Unknown'}")

except Exception as e:
    print(f"✗ 模型加载失败: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# 7. 询问是否继续导出
print("\n" + "=" * 60)
print("环境检查完成 - 一切正常!")
print("=" * 60)

response = input("\n是否开始导出? 这将需要5-10分钟 (y/n): ")
if response.lower() != 'y':
    print("已取消")
    sys.exit(0)

# 8. 开始导出
print("\n" + "=" * 60)
print("步骤1/2: 导出ONNX")
print("=" * 60)

try:
    onnx_path = model.export(
        format='onnx',
        imgsz=992,  # 使用你设置的尺寸
        opset=11,
        simplify=True,
        dynamic=False,
        device=0
    )

    print(f"\n✓ ONNX导出成功: {onnx_path}")

    # 移动到models目录
    import shutil
    target_onnx = "models/yanzhi20260115.onnx"
    if onnx_path != target_onnx:
        if os.path.exists(onnx_path):
            shutil.move(onnx_path, target_onnx)
            print(f"✓ 已移动到: {target_onnx}")

except Exception as e:
    print(f"\n✗ ONNX导出失败: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# 9. 导出TensorRT
print("\n" + "=" * 60)
print("步骤2/2: 导出TensorRT引擎")
print("=" * 60)
print("\n这可能需要5-10分钟,请耐心等待...")

try:
    # 重新加载模型
    model = YOLO(model_path)

    engine_path = model.export(
        format='engine',
        imgsz=992,
        half=True,
        device=0,
        workspace=12,
        verbose=True
    )

    print(f"\n✓ TensorRT引擎导出成功: {engine_path}")

    # 重命名
    import shutil
    gpu_name = torch.cuda.get_device_name(0)
    if "4080" in gpu_name or "4090" in gpu_name:
        gpu_suffix = "4080s"
    elif "3080" in gpu_name:
        gpu_suffix = "3080ti"
    else:
        gpu_suffix = "custom"

    target_engine = f"models/yanzhi20260115_{gpu_suffix}_fp16.engine"
    if engine_path != target_engine:
        if os.path.exists(engine_path):
            shutil.move(engine_path, target_engine)
            print(f"✓ 已移动到: {target_engine}")

    print(f"\n文件大小: {os.path.getsize(target_engine) / 1024**2:.2f} MB")

except Exception as e:
    print(f"\n✗ TensorRT导出失败: {e}")
    import traceback
    traceback.print_exc()

    print("\n可能的原因:")
    print("1. TensorRT未安装或版本不兼容")
    print("2. CUDA版本与TensorRT不匹配")
    print("3. GPU显存不足(尝试减小workspace)")
    print("4. 驱动版本过旧")
    sys.exit(1)

# 10. 完成
print("\n" + "=" * 60)
print("导出完成!")
print("=" * 60)
print(f"\n生成的文件:")
print(f"1. models/yanzhi20260115.onnx")
print(f"2. models/yanzhi20260115_{gpu_suffix}_fp16.engine")
print(f"\n下一步: 编译C++代码")
print("详见 DEPLOYMENT_GUIDE.md")
