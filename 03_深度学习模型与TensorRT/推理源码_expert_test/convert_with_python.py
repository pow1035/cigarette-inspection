"""
使用Python调用TensorRT转换ONNX
这样可以使用conda环境中的CUDA库
"""
import subprocess
import os
import sys
from pathlib import Path

# 设置路径
trtexec = r"D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin\trtexec.exe"
trt_root = r"D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6"
onnx_file = "../yanzhi20260115.onnx"
output_file = "models/yanzhi20260115_4080s_fp16.engine"

print("=" * 60)
print("ONNX to TensorRT Conversion (Python)")
print("=" * 60)
print()

# 检查文件
if not os.path.exists(trtexec):
    print(f"错误: trtexec不存在: {trtexec}")
    sys.exit(1)

if not os.path.exists(onnx_file):
    print(f"错误: ONNX文件不存在: {onnx_file}")
    sys.exit(1)

# 创建输出目录
os.makedirs("models", exist_ok=True)

# 设置环境变量，添加TensorRT库路径
env = os.environ.copy()
trt_lib = os.path.join(trt_root, "lib")
trt_bin = os.path.join(trt_root, "bin")

# 添加到PATH
if 'PATH' in env:
    env['PATH'] = f"{trt_lib};{trt_bin};{env['PATH']}"
else:
    env['PATH'] = f"{trt_lib};{trt_bin}"

print(f"TensorRT: {trtexec}")
print(f"ONNX:     {onnx_file}")
print(f"Output:   {output_file}")
print()
print("转换中 (5-10分钟)...")
print()

# 执行转换
cmd = [
    trtexec,
    f"--onnx={onnx_file}",
    f"--saveEngine={output_file}",
    "--fp16",
    "--workspace=12288",
    "--verbose"
]

try:
    result = subprocess.run(cmd, env=env, capture_output=False, text=True)

    if result.returncode == 0:
        if os.path.exists(output_file):
            size = os.path.getsize(output_file) / (1024 * 1024)
            print()
            print("=" * 60)
            print("成功!")
            print("=" * 60)
            print(f"生成文件: {output_file}")
            print(f"文件大小: {size:.2f} MB")
        else:
            print()
            print("警告: trtexec返回成功但未找到输出文件")
            print(f"查找位置: {output_file}")
    else:
        print()
        print("转换失败，返回码:", result.returncode)
        sys.exit(1)

except Exception as e:
    print(f"执行错误: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
