@echo off
REM YOLO26 TensorRT模型导出和C++部署脚本 (Windows)

echo ========================================
echo YOLO26 TensorRT 完整部署流程
echo ========================================

REM 检查Python环境
python --version >nul 2>&1
if errorlevel 1 (
    echo 错误: 未找到Python
    pause
    exit /b 1
)

REM 步骤1: 导出TensorRT引擎
echo.
echo [步骤1/3] 导出TensorRT引擎...
echo.

python export_tensorrt.py
if errorlevel 1 (
    echo 导出失败
    pause
    exit /b 1
)

echo.
echo ✓ 模型导出完成
echo.

REM 步骤2: 编译C++代码
echo [步骤2/3] 编译C++推理引擎...
echo.

REM 创建构建目录
if not exist "build" mkdir build
cd build

REM 配置CMake (根据你的环境修改路径)
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DTENSORRT_ROOT="C:/Program Files/NVIDIA GPU Computing Toolkit/TensorRT"

if errorlevel 1 (
    echo CMake配置失败
    cd ..
    pause
    exit /b 1
)

REM 编译
cmake --build . --config Release

if errorlevel 1 (
    echo 编译失败
    cd ..
    pause
    exit /b 1
)

cd ..

echo.
echo ✓ C++代码编译完成
echo.

REM 步骤3: 运行测试
echo [步骤3/3] 运行性能测试...
echo.

REM 检测当前GPU并选择对应的模型
nvidia-smi --query-gpu=name --format=csv,noheader > gpu_name.txt
set /p GPU_NAME=<gpu_name.txt
del gpu_name.txt

echo 检测到GPU: %GPU_NAME%

REM 根据GPU选择引擎文件
set ENGINE_FILE=models\yanzhi20260115_4080s_fp16.engine
echo %GPU_NAME% | findstr /i "3080" >nul
if not errorlevel 1 (
    set ENGINE_FILE=models\yanzhi20260115_3080ti_fp16.engine
    echo 使用3080Ti优化引擎
) else (
    echo 使用4080S优化引擎
)

REM 运行性能测试
build\bin\Release\test_yolo_tensorrt.exe %ENGINE_FILE% bench

echo.
echo ========================================
echo 部署完成!
echo ========================================
echo.
echo 生成的文件:
echo   - models\yanzhi20260115.onnx
echo   - models\yanzhi20260115_3080ti_fp16.engine
echo   - models\yanzhi20260115_4080s_fp16.engine
echo   - build\bin\Release\test_yolo_tensorrt.exe
echo.
echo 使用方法:
echo   测试图像: build\bin\Release\test_yolo_tensorrt.exe %ENGINE_FILE% image input.jpg output.jpg
echo   测试视频: build\bin\Release\test_yolo_tensorrt.exe %ENGINE_FILE% video input.mp4 output.mp4
echo   性能测试: build\bin\Release\test_yolo_tensorrt.exe %ENGINE_FILE% bench
echo.

pause
