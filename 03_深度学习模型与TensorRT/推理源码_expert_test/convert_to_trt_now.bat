@echo off
chcp 65001 >nul 2>&1

set TRT_BIN=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin
set ONNX=%~dp0..\yanzhi20260115.onnx
set OUTPUT=%~dp0models\yanzhi20260115_4080s_fp16.engine

cd /d "%TRT_BIN%"

echo ========================================
echo ONNX to TensorRT Conversion
echo ========================================
echo.
echo Working directory: %CD%
echo ONNX: %ONNX%
echo Output: %OUTPUT%
echo.
echo Converting (5-10 minutes)...
echo.

if not exist "%~dp0models" mkdir "%~dp0models"

trtexec.exe ^
  --onnx="%ONNX%" ^
  --saveEngine="%OUTPUT%" ^
  --fp16 ^
  --workspace=12288 ^
  --verbose

echo.
if exist "%OUTPUT%" (
    echo ========================================
    echo SUCCESS
    echo ========================================
    echo.
    echo Generated: %OUTPUT%
    for %%A in ("%OUTPUT%") do echo Size: %%~zA bytes (%%~zAABBB)
    echo.
) else (
    echo ========================================
    echo FAILED
    echo ========================================
    echo.
)

cd /d "%~dp0"
pause
