@echo off
chcp 65001 >nul 2>&1

set TRT_BIN=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin
set TRT_LIB=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\lib
set ONNX=%~dp0..\yanzhi20260115.onnx
set OUTPUT=%~dp0models\yanzhi20260115_4080s_fp16.engine

REM 切换到TensorRT的bin目录运行
cd /d "%TRT_BIN%"

REM 设置PATH包含lib目录
set PATH=%TRT_LIB%;%PATH%

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

REM 创建输出目录
if not exist "%~dp0models" mkdir "%~dp0models"

REM 运行trtexec
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
    for %%A in ("%OUTPUT%") do echo Size: %%~zA bytes
    echo.
) else (
    echo ========================================
    echo FAILED
    echo ========================================
    echo.
    echo Engine file was not created
    echo.
)

cd /d "%~dp0"
pause
