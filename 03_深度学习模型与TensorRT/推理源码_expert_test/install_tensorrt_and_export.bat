@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

set PYTHON=C:\Users\Administrator\anaconda3\envs\yolo26\python.exe

echo ========================================
echo Install TensorRT and Re-export
echo ========================================
echo.

echo [1/2] Installing TensorRT Python package...
echo.

REM Try nvidia-tensorrt first
echo Trying: nvidia-tensorrt
%PYTHON% -m pip install nvidia-tensorrt

if errorlevel 1 (
    echo.
    echo First attempt failed, trying: tensorrt
    %PYTHON% -m pip install tensorrt

    if errorlevel 1 (
        echo.
        echo ========================================
        echo TensorRT installation failed
        echo ========================================
        echo.
        echo Please install manually:
        echo 1. Download TensorRT from https://developer.nvidia.com/tensorrt
        echo 2. Extract and install Python wheel:
        echo    cd ^<TensorRT^>\python
        echo    %PYTHON% -m pip install tensorrt-*-cp310-*.whl
        echo.
        echo Or use ONNX model for now (already exported successfully^):
        echo    e:\code\yolo26\yanzhi20260115.onnx
        echo.
        pause
        exit /b 1
    )
)

echo.
echo [2/2] Verifying TensorRT installation...
%PYTHON% -c "import tensorrt; print('  TensorRT version:', tensorrt.__version__)"

if errorlevel 1 (
    echo   ERROR - TensorRT import failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo Re-exporting with TensorRT
echo ========================================
echo.

%PYTHON% export_tensorrt.py

if errorlevel 1 (
    echo.
    echo Export failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS
echo ========================================
echo.
echo Generated files:
dir /b models\*.onnx 2>nul
dir /b models\*.engine 2>nul
echo.

pause
