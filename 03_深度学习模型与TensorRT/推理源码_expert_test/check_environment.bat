@echo off
REM 环境检查和诊断脚本

echo ========================================
echo YOLO26 TensorRT 环境检查
echo ========================================

echo.
echo [1] 检查Python环境
echo.

REM 尝试多个Python命令
set PYTHON_CMD=python

REM 检查conda环境
where conda >nul 2>&1
if not errorlevel 1 (
    echo 检测到Anaconda环境
    echo.
    echo 可用的conda环境:
    call conda env list
    echo.
    echo 建议使用conda环境运行导出脚本:
    echo   conda activate your_env
    echo   python expert_test\quick_export_4080s.py
)

REM 检查Python
%PYTHON_CMD% --version 2>nul
if errorlevel 1 (
    set PYTHON_CMD=python3
    %PYTHON_CMD% --version 2>nul
    if errorlevel 1 (
        set PYTHON_CMD=C:\Users\Administrator\anaconda3\python.exe
    )
)

echo 使用Python: %PYTHON_CMD%
%PYTHON_CMD% --version

echo.
echo [2] 检查Python包
echo.

%PYTHON_CMD% -c "import torch; print(f'PyTorch: {torch.__version__}')"
%PYTHON_CMD% -c "import torch; print(f'CUDA Available: {torch.cuda.is_available()}')"
%PYTHON_CMD% -c "from ultralytics import YOLO; print('Ultralytics: OK')"
%PYTHON_CMD% -c "import cv2; print(f'OpenCV: {cv2.__version__}')"

echo.
echo [3] 检查GPU
echo.

nvidia-smi --query-gpu=name,driver_version,memory.total --format=csv

echo.
echo [4] 检查CUDA和TensorRT
echo.

where nvcc
if not errorlevel 1 (
    nvcc --version | findstr "release"
)

if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT" (
    echo TensorRT: 已安装 在 C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT
) else (
    echo TensorRT: 未找到标准路径，请手动指定
)

echo.
echo [5] 检查模型文件
echo.

if exist "..\yanzhi20260115.pt" (
    echo ✓ 模型文件存在: yanzhi20260115.pt
    for %%A in ("..\yanzhi20260115.pt") do echo   大小: %%~zA 字节
) else (
    echo ✗ 模型文件不存在: yanzhi20260115.pt
)

echo.
echo ========================================
echo 环境检查完成
echo ========================================
echo.

pause
