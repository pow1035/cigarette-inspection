"""
快速启动脚本 - 仅导出4080S模型并测试
"""
import os
import sys
import torch
from pathlib import Path

sys.path.append(str(Path(__file__).parent.parent))
from ultralytics import YOLO


def main():
    print("=" * 60)
    print("YOLO26 快速导出和测试 (4080S)")
    print("=" * 60)

    # 检查环境
    print(f"\nPyTorch: {torch.__version__}")
    print(f"CUDA: {torch.cuda.is_available()}")

    if not torch.cuda.is_available():
        print("错误: 需要CUDA支持")
        return

    gpu_name = torch.cuda.get_device_name(0)
    print(f"GPU: {gpu_name}")

    # 模型路径
    model_path = "../yanzhi20260115.pt"
    if not os.path.exists(model_path):
        print(f"\n错误: 模型文件不存在: {model_path}")
        return

    print(f"\n模型: {model_path}")
    print(f"大小: {os.path.getsize(model_path) / 1024 / 1024:.2f} MB")

    # 创建输出目录
    os.makedirs("models", exist_ok=True)

    # 加载模型
    print("\n加载模型...")
    model = YOLO(model_path)

    # 导出ONNX
    print("\n[1/2] 导出ONNX...")
    try:
        model.export(
            format='onnx',
            imgsz=640,
            opset=11,
            simplify=True,
            dynamic=False,
            device=0
        )

        # 移动到models目录
        onnx_file = model_path.replace('.pt', '.onnx')
        target_onnx = "models/yanzhi20260115.onnx"

        if os.path.exists(onnx_file):
            import shutil
            if onnx_file != target_onnx:
                shutil.move(onnx_file, target_onnx)
            print(f"✓ ONNX导出成功: {target_onnx}")
        else:
            print("✗ ONNX导出失败")
            return

    except Exception as e:
        print(f"✗ ONNX导出失败: {e}")
        return

    # 导出TensorRT (4080S)
    print("\n[2/2] 导出TensorRT引擎 (4080S优化)...")
    print("注意: 这可能需要5-10分钟...")

    try:
        model.export(
            format='engine',
            imgsz=640,
            half=True,  # FP16
            device=0,
            workspace=12,  # 4080S有16GB显存
            verbose=True,
            simplify=True,
        )

        # 重命名引擎文件
        engine_file = model_path.replace('.pt', '.engine')
        target_engine = "models/yanzhi20260115_4080s_fp16.engine"

        if os.path.exists(engine_file):
            import shutil
            if engine_file != target_engine:
                shutil.move(engine_file, target_engine)
            print(f"✓ TensorRT引擎构建成功: {target_engine}")
            print(f"  大小: {os.path.getsize(target_engine) / 1024 / 1024:.2f} MB")
        else:
            print("✗ TensorRT引擎构建失败")
            return

    except Exception as e:
        print(f"✗ TensorRT引擎构建失败: {e}")
        import traceback
        traceback.print_exc()
        return

    print("\n" + "=" * 60)
    print("导出完成!")
    print("=" * 60)
    print(f"\n生成的文件:")
    print(f"  - models/yanzhi20260115.onnx")
    print(f"  - models/yanzhi20260115_4080s_fp16.engine")
    print(f"\n下一步:")
    print(f"  1. 编译C++代码:")
    print(f"     cd expert_test")
    print(f"     mkdir build && cd build")
    print(f"     cmake .. -G \"Visual Studio 17 2022\" -A x64")
    print(f"     cmake --build . --config Release")
    print(f"  2. 运行测试:")
    print(f"     build\\bin\\Release\\test_yolo_tensorrt.exe models\\yanzhi20260115_4080s_fp16.engine bench")
    print()


if __name__ == "__main__":
    main()
