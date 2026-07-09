"""
最简单的环境测试 - 只检查不导出
"""
import sys

print("Python路径:", sys.executable)
print("Python版本:", sys.version)

try:
    import torch
    print("\n[OK] PyTorch:", torch.__version__)
    print("     CUDA:", torch.cuda.is_available())
    if torch.cuda.is_available():
        print("     GPU:", torch.cuda.get_device_name(0))
except:
    print("\n[ERROR] PyTorch未安装")
    sys.exit(1)

try:
    from ultralytics import YOLO
    print("\n[OK] Ultralytics: 已安装")
except:
    print("\n[ERROR] Ultralytics未安装")
    sys.exit(1)

from pathlib import Path

script_dir = Path(__file__).parent
model_path = script_dir.parent / "yanzhi20260115.pt"

if model_path.exists():
    print("\n[OK] 模型文件存在")
    print("     位置:", model_path)
    print("     大小: {:.2f} MB".format(model_path.stat().st_size / 1024**2))
else:
    print("\n[ERROR] 模型文件不存在")
    print("        查找位置:", model_path)

print("\n" + "="*50)
print("环境检查通过! 可以运行导出脚本")
print("="*50)
print("\n下一步:")
print("  python diagnose_and_export.py")
print("  或者")
print("  python export_tensorrt.py")

