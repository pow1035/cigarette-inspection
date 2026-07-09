#!/bin/bash
# YOLO26 TensorRT模型导出和C++部署脚本 (Linux)

set -e  # 遇到错误立即退出

echo "========================================"
echo "YOLO26 TensorRT 完整部署流程"
echo "========================================"

# 检查Python环境
if ! command -v python3 &> /dev/null; then
    echo "错误: 未找到Python3"
    exit 1
fi

# 步骤1: 导出TensorRT引擎
echo ""
echo "[步骤1/3] 导出TensorRT引擎..."
echo ""

python3 export_tensorrt.py

echo ""
echo "✓ 模型导出完成"
echo ""

# 步骤2: 编译C++代码
echo "[步骤2/3] 编译C++推理引擎..."
echo ""

# 创建构建目录
mkdir -p build
cd build

# 配置CMake (根据你的环境修改路径)
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DTENSORRT_ROOT="/usr/local/TensorRT"

# 编译 (使用所有可用核心)
cmake --build . --config Release -j$(nproc)

cd ..

echo ""
echo "✓ C++代码编译完成"
echo ""

# 步骤3: 运行测试
echo "[步骤3/3] 运行性能测试..."
echo ""

# 检测当前GPU并选择对应的模型
GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader | head -n 1)
echo "检测到GPU: $GPU_NAME"

# 根据GPU选择引擎文件
if [[ $GPU_NAME == *"3080"* ]]; then
    ENGINE_FILE="models/yanzhi20260115_3080ti_fp16.engine"
    echo "使用3080Ti优化引擎"
else
    ENGINE_FILE="models/yanzhi20260115_4080s_fp16.engine"
    echo "使用4080S优化引擎"
fi

# 运行性能测试
./build/bin/test_yolo_tensorrt "$ENGINE_FILE" bench

echo ""
echo "========================================"
echo "部署完成!"
echo "========================================"
echo ""
echo "生成的文件:"
echo "  - models/yanzhi20260115.onnx"
echo "  - models/yanzhi20260115_3080ti_fp16.engine"
echo "  - models/yanzhi20260115_4080s_fp16.engine"
echo "  - build/bin/test_yolo_tensorrt"
echo ""
echo "使用方法:"
echo "  测试图像: ./build/bin/test_yolo_tensorrt $ENGINE_FILE image input.jpg output.jpg"
echo "  测试视频: ./build/bin/test_yolo_tensorrt $ENGINE_FILE video input.mp4 output.mp4"
echo "  性能测试: ./build/bin/test_yolo_tensorrt $ENGINE_FILE bench"
echo ""
