"""
手动运行此脚本进行模型导出
请在正确配置的Python环境中运行
"""

print("""
=======================================================
YOLO26 TensorRT 模型导出脚本
=======================================================

使用说明:

1. 激活正确的Python环境:
   conda activate your_env
   (或确保已安装: torch, ultralytics, opencv-python)

2. 运行此脚本:
   python manual_export.py

3. 等待导出完成(可能需要5-10分钟)

=======================================================
""")

import os
import sys

# 检查依赖
try:
    import torch
    print(f"✓ PyTorch {torch.__version__}")
except ImportError:
    print("✗ 错误: 未安装PyTorch")
    print("  安装: pip install torch torchvision")
    sys.exit(1)

try:
    from ultralytics import YOLO
    print("✓ Ultralytics YOLO")
except ImportError:
    print("✗ 错误: 未安装ultralytics")
    print("  安装: pip install ultralytics")
    sys.exit(1)

# 检查CUDA
if not torch.cuda.is_available():
    print("✗ 错误: CUDA不可用")
    print("  请检查:")
    print("  1. NVIDIA驱动已安装")
    print("  2. CUDA Toolkit已安装")
    print("  3. PyTorch CUDA版本正确")
    sys.exit(1)

print(f"✓ CUDA {torch.version.cuda}")
print(f"✓ GPU: {torch.cuda.get_device_name(0)}")

# 配置
MODEL_PATH = "../yanzhi20260115.pt"
OUTPUT_DIR = "./models"
IMG_SIZE = 640

# 检查模型文件
if not os.path.exists(MODEL_PATH):
    print(f"\n✗ 错误: 模型文件不存在: {MODEL_PATH}")
    sys.exit(1)

print(f"\n模型文件: {MODEL_PATH}")
print(f"大小: {os.path.getsize(MODEL_PATH) / 1024 / 1024:.2f} MB")

# 创建输出目录
os.makedirs(OUTPUT_DIR, exist_ok=True)

# 加载模型
print("\n" + "=" * 50)
print("加载模型...")
print("=" * 50)

try:
    model = YOLO(MODEL_PATH)
    print("✓ 模型加载成功")
except Exception as e:
    print(f"✗ 模型加载失败: {e}")
    sys.exit(1)

# ========== 导出ONNX ==========
print("\n" + "=" * 50)
print("步骤 1/2: 导出ONNX")
print("=" * 50)

try:
    print("开始导出ONNX...")
    print(f"  输入尺寸: {IMG_SIZE}x{IMG_SIZE}")
    print(f"  OpSet: 11")
    print(f"  简化: 是")

    model.export(
        format='onnx',
        imgsz=IMG_SIZE,
        opset=11,
        simplify=True,
        dynamic=False,
        device=0
    )

    # 移动文件
    onnx_src = MODEL_PATH.replace('.pt', '.onnx')
    onnx_dst = os.path.join(OUTPUT_DIR, "yanzhi20260115.onnx")

    if os.path.exists(onnx_src):
        import shutil
        shutil.move(onnx_src, onnx_dst)
        print(f"\n✓ ONNX导出成功!")
        print(f"  位置: {onnx_dst}")
        print(f"  大小: {os.path.getsize(onnx_dst) / 1024 / 1024:.2f} MB")
    else:
        print("\n✗ ONNX文件未生成")
        sys.exit(1)

except Exception as e:
    print(f"\n✗ ONNX导出失败: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# ========== 导出TensorRT ==========
print("\n" + "=" * 50)
print("步骤 2/2: 导出TensorRT引擎(4080S)")
print("=" * 50)
print("\n⚠️  注意: 这可能需要5-10分钟,请耐心等待...\n")

try:
    # 检测GPU
    gpu_name = torch.cuda.get_device_name(0)
    print(f"目标GPU: {gpu_name}")

    # 根据GPU选择配置
    if "3080" in gpu_name:
        gpu_suffix = "3080ti"
        workspace = 8  # 3080Ti有12GB显存
    elif "4080" in gpu_name or "4090" in gpu_name:
        gpu_suffix = "4080s"
        workspace = 12  # 4080S有16GB显存
    else:
        gpu_suffix = "custom"
        workspace = 8

    print(f"  配置: {gpu_suffix}")
    print(f"  精度: FP16")
    print(f"  工作空间: {workspace} GB")
    print(f"  输入尺寸: {IMG_SIZE}x{IMG_SIZE}")

    # 重新加载模型(避免缓存问题)
    model = YOLO(MODEL_PATH)

    # 导出TensorRT
    model.export(
        format='engine',
        imgsz=IMG_SIZE,
        half=True,  # FP16
        device=0,
        workspace=workspace,
        verbose=True,
        simplify=True,
    )

    # 移动并重命名文件
    engine_src = MODEL_PATH.replace('.pt', '.engine')
    engine_dst = os.path.join(OUTPUT_DIR, f"yanzhi20260115_{gpu_suffix}_fp16.engine")

    if os.path.exists(engine_src):
        import shutil
        shutil.move(engine_src, engine_dst)
        print(f"\n✓ TensorRT引擎构建成功!")
        print(f"  位置: {engine_dst}")
        print(f"  大小: {os.path.getsize(engine_dst) / 1024 / 1024:.2f} MB")
    else:
        print("\n✗ TensorRT引擎文件未生成")
        sys.exit(1)

except Exception as e:
    print(f"\n✗ TensorRT导出失败: {e}")
    print("\n可能的原因:")
    print("  1. TensorRT未正确安装")
    print("  2. CUDA版本与TensorRT不兼容")
    print("  3. GPU显存不足")
    print("  4. 驱动版本过旧")
    import traceback
    traceback.print_exc()
    sys.exit(1)

# ========== 完成 ==========
print("\n" + "=" * 50)
print("导出完成!")
print("=" * 50)

print("\n生成的文件:")
print(f"  1. ONNX: {os.path.join(OUTPUT_DIR, 'yanzhi20260115.onnx')}")
print(f"  2. TensorRT: {os.path.join(OUTPUT_DIR, f'yanzhi20260115_{gpu_suffix}_fp16.engine')}")

print("\n下一步:")
print("  1. 编译C++代码:")
print("     cd expert_test")
print("     mkdir build && cd build")
print("     cmake .. -G \"Visual Studio 17 2022\" -A x64 -DCMAKE_BUILD_TYPE=Release")
print("     cmake --build . --config Release")
print("")
print("  2. 运行测试:")
print(f"     build\\bin\\Release\\test_yolo_tensorrt.exe models\\yanzhi20260115_{gpu_suffix}_fp16.engine bench")
print("")

print("详细文档请查看:")
print("  - README.md (完整文档)")
print("  - DEPLOYMENT_GUIDE.md (部署指南)")
print("")
