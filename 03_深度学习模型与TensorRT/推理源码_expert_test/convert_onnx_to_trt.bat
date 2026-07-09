@echo off
chcp 65001 >nul 2>&1

echo ========================================
echo Convert ONNX to TensorRT using trtexec
echo ========================================
echo.

REM Set paths
set ONNX_FILE=e:\code\yolo26\yanzhi20260115.onnx
set OUTPUT_DIR=e:\code\yolo26\expert_test\models
set TRTEXEC="C:\Program Files\NVIDIA GPU Computing Toolkit\TensorRT\bin\trtexec.exe"

REM Check if trtexec exists
if not exist %TRTEXEC% (
    echo ERROR: trtexec.exe not found at:
    echo %TRTEXEC%
    echo.
    echo Please install TensorRT or update the path in this script.
    echo Download from: https://developer.nvidia.com/tensorrt
    echo.
    pause
    exit /b 1
)

REM Check if ONNX exists
if not exist "%ONNX_FILE%" (
    echo ERROR: ONNX file not found: %ONNX_FILE%
    echo Please export ONNX first using export_tensorrt.py
    pause
    exit /b 1
)

echo Found:
echo   ONNX: %ONNX_FILE%
echo   trtexec: %TRTEXEC%
echo.

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo ========================================
echo Converting to TensorRT (4080S FP16)
echo ========================================
echo.
echo This will take 5-10 minutes...
echo.

%TRTEXEC% ^
  --onnx="%ONNX_FILE%" ^
  --saveEngine="%OUTPUT_DIR%\yanzhi20260115_4080s_fp16.engine" ^
  --fp16 ^
  --workspace=12288 ^
  --verbose ^
  --dumpLayerInfo ^
  --dumpProfile ^
  --separateProfileRun

if errorlevel 1 (
    echo.
    echo Conversion FAILED
    pause
    exit /b 1
)

echo.
echo ========================================
echo SUCCESS
echo ========================================
echo.
echo Generated:
echo   %OUTPUT_DIR%\yanzhi20260115_4080s_fp16.engine
echo.
dir /b "%OUTPUT_DIR%\*.engine"
echo.
echo Next: Compile C++ code and test
echo.

pause
