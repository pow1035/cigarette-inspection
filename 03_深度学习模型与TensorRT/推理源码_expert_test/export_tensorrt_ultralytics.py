"""
使用Ultralytics直接导出TensorRT引擎
这是最可靠的方法,因为Ultralytics会处理所有依赖
"""
from ultralytics import YOLO
from pathlib import Path

# 配置
script_dir = Path(__file__).parent
model_path = script_dir.parent / "yanzhi20260115.pt"
output_dir = script_dir / "models"
output_dir.mkdir(exist_ok=True)

print("=" * 60)
print("YOLO26 TensorRT Export (Ultralytics)")
print("=" * 60)
print()
print(f"Model: {model_path}")
print(f"Output dir: {output_dir}")
print()

# 加载模型
print("Loading model...")
model = YOLO(str(model_path))

# 导出为TensorRT
print("Exporting to TensorRT...")
print("This will take 5-10 minutes...")
print()

try:
    # Ultralytics的export会自动:
    # 1. 导出ONNX
    # 2. 使用trtexec或Python API转换为engine
    # 3. 处理所有依赖关系
    exported_model = model.export(
        format='engine',  # TensorRT engine format
        half=True,        # FP16
        workspace=12,     # 12 GB workspace
        verbose=True,
        device=0          # GPU 0
    )

    print()
    print("=" * 60)
    print("SUCCESS!")
    print("=" * 60)
    print()
    print(f"Engine file: {exported_model}")
    print()

    # 复制到我们的models目录
    import shutil
    engine_name = "yanzhi20260115_4080s_fp16.engine"
    target = output_dir / engine_name

    if Path(exported_model).exists():
        shutil.copy2(exported_model, target)
        print(f"Copied to: {target}")

        # 显示文件大小
        size_mb = target.stat().st_size / (1024 * 1024)
        print(f"Size: {size_mb:.2f} MB")
    else:
        print(f"Warning: Engine file not found at {exported_model}")

    print()
    print("Next: Build C++ code and test inference")

except Exception as e:
    print()
    print("=" * 60)
    print("FAILED")
    print("=" * 60)
    print()
    print(f"Error: {e}")
    import traceback
    traceback.print_exc()

    print()
    print("If this fails due to TensorRT issues, we can:")
    print("1. Use the ONNX model with ONNX Runtime instead")
    print("2. Try installing TensorRT Python package")
    print("3. Manual fix environment dependencies")
