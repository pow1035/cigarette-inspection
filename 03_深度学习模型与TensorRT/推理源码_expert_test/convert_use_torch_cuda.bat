@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

set TRTEXEC=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin\trtexec.exe
set TRT_ROOT=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6
set ONNX=..\yanzhi20260115.onnx
set OUTPUT=models\yanzhi20260115_4080s_fp16.engine

REM 使用PyTorch自带的CUDA库
set TORCH_CUDA=C:\Users\Administrator\anaconda3\envs\yolo26\Lib\site-packages\torch\lib

REM 设置PATH - 添加所有可能需要的库路径
set PATH=%TRT_ROOT%\lib;%TRT_ROOT%\bin;%TORCH_CUDA%;C:\Users\Administrator\anaconda3\envs\yolo26\Library\bin;%PATH%

echo ========================================
echo ONNX to TensorRT Conversion
echo ========================================
echo.
echo TensorRT: %TRTEXEC%
echo ONNX:      %ONNX%
echo Output:    %OUTPUT%
echo.
echo Using CUDA libraries from PyTorch
echo.
echo Converting (5-10 minutes)...
echo.

if not exist models mkdir models

"%TRTEXEC%" ^
  --onnx=%ONNX% ^
  --saveEngine=%OUTPUT% ^
  --fp16 ^
  --workspace=12288 ^
  --verbose

echo.
echo Checking if engine file was created...
if exist "%OUTPUT%" (
    echo Engine file created successfully!
    dir "%OUTPUT%"
) else (
    echo ERROR: Engine file was NOT created!
    echo Check the trtexec output above for errors
    pause
    exit /b 1
)

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
echo Generated: %OUTPUT%
dir /b models\*.engine
echo.
echo Next: Compile C++ code
echo.

pause
