"""
使用Python + TensorRT直接转换ONNX到Engine
绕过trtexec.exe的DLL加载问题
"""
import tensorrt as trt
import os

def build_engine(onnx_file_path, engine_file_path, fp16_mode=True, max_workspace_size=12):
    """
    使用TensorRT Python API构建引擎

    Args:
        onnx_file_path: ONNX模型路径
        engine_file_path: 输出引擎文件路径
        fp16_mode: 是否使用FP16
        max_workspace_size: 最大workspace大小(GB)
    """
    TRT_LOGGER = trt.Logger(trt.Logger.VERBOSE)

    # 创建builder和network
    builder = trt.Builder(TRT_LOGGER)
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    parser = trt.OnnxParser(network, TRT_LOGGER)

    print(f"Loading ONNX file: {onnx_file_path}")
    with open(onnx_file_path, 'rb') as model:
        if not parser.parse(model.read()):
            print('ERROR: Failed to parse the ONNX file.')
            for error in range(parser.num_errors):
                print(parser.get_error(error))
            return None

    print(f"ONNX file loaded successfully")
    print(f"Network inputs: {[network.get_input(i).name for i in range(network.num_inputs)]}")
    print(f"Network outputs: {[network.get_output(i).name for i in range(network.num_outputs)]}")

    # 配置builder
    config = builder.create_builder_config()
    config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, max_workspace_size * (1 << 30))  # GB to bytes

    if fp16_mode and builder.platform_has_fast_fp16:
        print("Enabling FP16 mode")
        config.set_flag(trt.BuilderFlag.FP16)
    else:
        print("FP16 not available or not enabled")

    # 构建引擎
    print(f"Building TensorRT engine... This may take several minutes.")
    print(f"Workspace size: {max_workspace_size} GB")

    serialized_engine = builder.build_serialized_network(network, config)

    if serialized_engine is None:
        print("ERROR: Failed to build engine")
        return None

    # 保存引擎
    print(f"Saving engine to: {engine_file_path}")
    os.makedirs(os.path.dirname(engine_file_path), exist_ok=True)
    with open(engine_file_path, 'wb') as f:
        f.write(serialized_engine)

    file_size_mb = os.path.getsize(engine_file_path) / (1024 * 1024)
    print(f"Engine saved successfully! Size: {file_size_mb:.2f} MB")

    return True

if __name__ == "__main__":
    import sys

    # 路径配置
    script_dir = os.path.dirname(os.path.abspath(__file__))
    onnx_path = os.path.join(os.path.dirname(script_dir), "yanzhi20260115.onnx")
    engine_path = os.path.join(script_dir, "models", "yanzhi20260115_4080s_fp16.engine")

    print("=" * 60)
    print("TensorRT Engine Builder (Python API)")
    print("=" * 60)
    print()

    # 检查ONNX文件
    if not os.path.exists(onnx_path):
        print(f"ERROR: ONNX file not found: {onnx_path}")
        sys.exit(1)

    print(f"ONNX model: {onnx_path}")
    print(f"Output engine: {engine_path}")
    print()

    # 构建引擎
    try:
        success = build_engine(
            onnx_file_path=onnx_path,
            engine_file_path=engine_path,
            fp16_mode=True,
            max_workspace_size=12
        )

        if success:
            print()
            print("=" * 60)
            print("SUCCESS!")
            print("=" * 60)
            print()
            print(f"Engine file: {engine_path}")
            print()
            print("Next step: Compile and test C++ code")
        else:
            print()
            print("=" * 60)
            print("FAILED")
            print("=" * 60)
            sys.exit(1)

    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
