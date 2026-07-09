@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

set TRTEXEC=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin\trtexec.exe
set TRT_ROOT=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6
set ONNX=..\yanzhi20260115.onnx
set OUTPUT=models\yanzhi20260115_4080s_fp16.engine

REM 添加TensorRT和conda环境的CUDA库到PATH
REM 确保TensorRT的lib目录在最前面
set PATH=%TRT_ROOT%\lib;%TRT_ROOT%\bin;C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8\bin;C:\Users\Administrator\anaconda3\envs\yolo26\Library\bin;%PATH%

REM 显示关键DLL是否存在
echo Checking TensorRT plugins...
if exist "%TRT_ROOT%\lib\nvinfer_plugin.dll" (
    echo Found: nvinfer_plugin.dll
) else (
    echo WARNING: nvinfer_plugin.dll not found in %TRT_ROOT%\lib
)
echo.

echo ========================================
echo ONNX to TensorRT Conversion
echo ========================================
echo.
echo TensorRT: %TRTEXEC%
echo ONNX:      %ONNX%
echo Output:    %OUTPUT%
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
dir models\*.engine
echo.
echo Next: Compile C++ code
echo   - Update CMakeLists.txt TensorRT path
echo   - mkdir build ^&^& cd build
echo   - cmake .. -G "Visual Studio 17 2022" -A x64
echo   - cmake --build . --config Release
echo.

pause
