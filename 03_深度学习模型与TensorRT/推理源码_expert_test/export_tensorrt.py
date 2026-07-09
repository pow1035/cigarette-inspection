"""
YOLO26模型导出TensorRT引擎脚本
支持针对不同显卡优化的模型导出
"""
import os
import sys
import torch
from pathlib import Path

# 添加父目录到路径
sys.path.append(str(Path(__file__).parent.parent))
from ultralytics import YOLO


def check_environment():
    """检查运行环境"""
    print("=" * 60)
    print("环境检查")
    print("=" * 60)
    print(f"PyTorch版本: {torch.__version__}")
    print(f"CUDA可用: {torch.cuda.is_available()}")

    if torch.cuda.is_available():
        print(f"CUDA版本: {torch.version.cuda}")
        print(f"当前GPU: {torch.cuda.get_device_name(0)}")
        print(f"GPU数量: {torch.cuda.device_count()}")

        # 获取GPU计算能力
        capability = torch.cuda.get_device_capability(0)
        print(f"GPU计算能力: {capability[0]}.{capability[1]}")

        # 显存信息
        total_memory = torch.cuda.get_device_properties(0).total_memory / (1024**3)
        print(f"总显存: {total_memory:.2f} GB")

        return True
    else:
        print("错误: 未检测到CUDA，TensorRT导出需要GPU支持")
        return False


def export_onnx(model_path, output_dir, imgsz=640):
    """
    导出ONNX模型作为中间格式

    Args:
        model_path: PyTorch模型路径
        output_dir: 输出目录
        imgsz: 输入图像尺寸
    """
    print("\n" + "=" * 60)
    print("步骤1: 导出ONNX模型")
    print("=" * 60)

    model = YOLO(model_path)

    # 构建输出路径
    model_name = Path(model_path).stem
    onnx_path = os.path.join(output_dir, f"{model_name}.onnx")

    print(f"输入模型: {model_path}")
    print(f"输出路径: {onnx_path}")
    print(f"输入尺寸: {imgsz}x{imgsz}")

    try:
        # 导出ONNX，使用opset 11以获得更好的TensorRT兼容性
        success = model.export(
            format='onnx',
            imgsz=imgsz,
            opset=11,  # TensorRT推荐使用opset 11或更高
            simplify=True,  # 简化ONNX图
            dynamic=False,  # 固定输入尺寸以获得更好的性能
            device=0
        )

        # ultralytics会自动保存到模型同目录，需要移动到输出目录
        default_onnx = model_path.replace('.pt', '.onnx')
        if os.path.exists(default_onnx) and default_onnx != onnx_path:
            import shutil
            shutil.move(default_onnx, onnx_path)

        if os.path.exists(onnx_path):
            file_size = os.path.getsize(onnx_path) / (1024 * 1024)
            print(f"✓ ONNX模型导出成功")
            print(f"  文件大小: {file_size:.2f} MB")
            print(f"  保存位置: {onnx_path}")
            return onnx_path
        else:
            print("✗ ONNX模型导出失败")
            return None

    except Exception as e:
        print(f"✗ 导出失败: {str(e)}")
        return None


def export_tensorrt_engine(model_path, output_dir, gpu_name, imgsz=640, half=True, workspace=4):
    """
    导出TensorRT引擎

    Args:
        model_path: PyTorch模型路径
        output_dir: 输出目录
        gpu_name: GPU名称标识 (3080ti 或 4080s)
        imgsz: 输入图像尺寸
        half: 是否使用FP16精度
        workspace: 工作空间大小(GB)
    """
    print("\n" + "=" * 60)
    print(f"步骤2: 导出TensorRT引擎 ({gpu_name.upper()})")
    print("=" * 60)

    model = YOLO(model_path)

    # 构建输出路径
    model_name = Path(model_path).stem
    precision = "fp16" if half else "fp32"
    engine_path = os.path.join(output_dir, f"{model_name}_{gpu_name}_{precision}.engine")

    print(f"输入模型: {model_path}")
    print(f"目标GPU: {gpu_name.upper()}")
    print(f"输入尺寸: {imgsz}x{imgsz}")
    print(f"精度模式: {precision.upper()}")
    print(f"工作空间: {workspace} GB")
    print(f"输出路径: {engine_path}")

    try:
        # 导出TensorRT引擎
        success = model.export(
            format='engine',
            imgsz=imgsz,
            half=half,
            device=0,
            workspace=workspace,
            verbose=True,
            simplify=True,
        )

        # ultralytics会自动保存，需要重命名
        default_engine = model_path.replace('.pt', '.engine')
        if os.path.exists(default_engine) and default_engine != engine_path:
            import shutil
            shutil.move(default_engine, engine_path)

        if os.path.exists(engine_path):
            file_size = os.path.getsize(engine_path) / (1024 * 1024)
            print(f"✓ TensorRT引擎构建成功")
            print(f"  文件大小: {file_size:.2f} MB")
            print(f"  保存位置: {engine_path}")
            return engine_path
        else:
            print("✗ TensorRT引擎构建失败")
            return None

    except Exception as e:
        print(f"✗ 构建失败: {str(e)}")
        import traceback
        traceback.print_exc()
        return None


def benchmark_model(model_path, imgsz=640, warmup=50, iterations=200):
    """
    测试模型性能

    Args:
        model_path: 模型路径
        imgsz: 输入尺寸
        warmup: 预热次数
        iterations: 测试迭代次数
    """
    print("\n" + "=" * 60)
    print("性能基准测试")
    print("=" * 60)

    try:
        import time
        import numpy as np

        model = YOLO(model_path)

        # 创建随机输入
        dummy_img = np.random.randint(0, 255, (imgsz, imgsz, 3), dtype=np.uint8)

        print(f"预热中 ({warmup} 次迭代)...")
        for _ in range(warmup):
            _ = model.predict(dummy_img, imgsz=imgsz, verbose=False)

        print(f"测试中 ({iterations} 次迭代)...")
        times = []
        for _ in range(iterations):
            start = time.perf_counter()
            _ = model.predict(dummy_img, imgsz=imgsz, verbose=False)
            torch.cuda.synchronize()  # 等待GPU完成
            end = time.perf_counter()
            times.append((end - start) * 1000)  # 转换为毫秒

        times = np.array(times)
        print(f"\n性能统计:")
        print(f"  平均延迟: {times.mean():.2f} ms")
        print(f"  中位延迟: {np.median(times):.2f} ms")
        print(f"  最小延迟: {times.min():.2f} ms")
        print(f"  最大延迟: {times.max():.2f} ms")
        print(f"  标准差: {times.std():.2f} ms")
        print(f"  吞吐量: {1000/times.mean():.2f} FPS")

    except Exception as e:
        print(f"性能测试失败: {str(e)}")


def main():
    """主函数"""
    print("\n" + "=" * 60)
    print("YOLO26 TensorRT模型导出工具")
    print("=" * 60)

    # 检查环境
    if not check_environment():
        return

    # 配置参数 - 自动检测模型路径
    script_dir = Path(__file__).parent
    model_path = script_dir.parent / "yanzhi20260115.pt"
    output_dir = script_dir / "models"
    imgsz = 992  # 使用992分辨率

    # 创建输出目录
    os.makedirs(output_dir, exist_ok=True)

    # 检查模型文件
    if not model_path.exists():
        print(f"\n错误: 模型文件不存在: {model_path}")
        return

    model_size = model_path.stat().st_size / (1024 * 1024)
    print(f"\n模型文件: {model_path}")
    print(f"模型大小: {model_size:.2f} MB")

    # 步骤1: 导出ONNX
    onnx_path = export_onnx(str(model_path), str(output_dir), imgsz)

    if not onnx_path:
        print("\nONNX导出失败，终止流程")
        return

    # 步骤2: 为不同GPU导出TensorRT引擎
    # 3080Ti优化 (Ampere架构, SM86)
    print("\n正在为RTX 3080Ti优化模型...")
    engine_3080ti = export_tensorrt_engine(
        str(model_path),
        str(output_dir),
        "3080ti",
        imgsz=imgsz,
        half=True,  # FP16精度
        workspace=8  # 3080Ti有12GB显存，可以使用更大workspace
    )

    # 4080S优化 (Ada Lovelace架构, SM89)
    print("\n正在为RTX 4080S优化模型...")
    engine_4080s = export_tensorrt_engine(
        str(model_path),
        str(output_dir),
        "4080s",
        imgsz=imgsz,
        half=True,  # FP16精度
        workspace=12  # 4080S有16GB显存，可以使用更大workspace
    )

    # 总结
    print("\n" + "=" * 60)
    print("导出完成总结")
    print("=" * 60)
    print(f"ONNX模型: {'✓' if onnx_path else '✗'}")
    print(f"3080Ti引擎: {'✓' if engine_3080ti else '✗'}")
    print(f"4080S引擎: {'✓' if engine_4080s else '✗'}")

    # 性能测试当前GPU的模型
    current_gpu = torch.cuda.get_device_name(0).lower()
    if engine_4080s and "4080" in current_gpu:
        print("\n检测到RTX 4080系列GPU，测试4080S引擎性能...")
        benchmark_model(engine_4080s, imgsz)
    elif engine_3080ti and "3080" in current_gpu:
        print("\n检测到RTX 3080系列GPU，测试3080Ti引擎性能...")
        benchmark_model(engine_3080ti, imgsz)

    print("\n所有文件保存在: " + os.path.abspath(output_dir))
    print("\n下一步: 使用C++代码进行部署测试")


if __name__ == "__main__":
    main()
