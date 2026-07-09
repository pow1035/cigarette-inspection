@echo off
chcp 65001 >nul 2>&1
cd /d "%~dp0"

echo ========================================
echo TensorRT 依赖检查和修复工具
echo ========================================
echo.

set TRT_ROOT=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6

echo 检查TensorRT安装...
if not exist "%TRT_ROOT%\bin\trtexec.exe" (
    echo [错误] 找不到trtexec.exe
    echo 路径: %TRT_ROOT%\bin\trtexec.exe
    pause
    exit /b 1
)
echo [OK] TensorRT已安装

echo.
echo 检查CUDA Toolkit安装...
set CUDA_FOUND=0

REM 检查常见的CUDA安装路径
for %%v in (11.8 11.7 11.6 11.5 11.4 11.3 11.2 11.1 11.0) do (
    if exist "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v%%v\bin\cudart64_*.dll" (
        set CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v%%v
        set CUDA_VERSION=%%v
        set CUDA_FOUND=1
        echo [OK] 找到CUDA %%v: !CUDA_PATH!
        goto :cuda_found
    )
)

:cuda_found
if %CUDA_FOUND%==0 (
    echo [警告] 未找到CUDA Toolkit安装
    echo.
    echo TensorRT需要CUDA 11.x运行时库
    echo 请从以下地址下载安装CUDA Toolkit 11.8:
    echo https://developer.nvidia.com/cuda-11-8-0-download-archive
    echo.
    echo 或者使用方案2：从PyTorch复制CUDA库
    goto :use_pytorch_cuda
)

echo.
echo 检查cuDNN安装...
set CUDNN_FOUND=0
if exist "%CUDA_PATH%\bin\cudnn*.dll" (
    echo [OK] cuDNN已安装
    set CUDNN_FOUND=1
) else (
    echo [警告] 未找到cuDNN
)

echo.
echo ========================================
echo 解决方案选择
echo ========================================
echo.
echo 方案1: 使用已安装的CUDA Toolkit [需要CUDA已正确安装]
echo 方案2: 从PyTorch复制CUDA库到TensorRT [快速方案]
echo 方案3: 手动下载CUDA库文件 [临时方案]
echo.
set /p choice="请选择方案 (1/2/3): "

if "%choice%"=="1" goto :use_cuda_toolkit
if "%choice%"=="2" goto :use_pytorch_cuda
if "%choice%"=="3" goto :manual_download
echo 无效选择
pause
exit /b 1

:use_cuda_toolkit
echo.
echo ========================================
echo 方案1: 配置系统PATH使用CUDA Toolkit
echo ========================================
echo.
if %CUDA_FOUND%==0 (
    echo [错误] 未找到CUDA Toolkit安装
    echo 请先安装CUDA Toolkit或选择方案2
    pause
    exit /b 1
)

echo 将以下路径添加到系统PATH:
echo   %CUDA_PATH%\bin
if %CUDNN_FOUND%==1 (
    echo   cuDNN已包含在CUDA中
)
echo.
echo 是否现在自动设置PATH? (当前会话有效)
set /p set_path="(y/n): "
if /i "%set_path%"=="y" (
    set "PATH=%CUDA_PATH%\bin;%PATH%"
    echo [OK] PATH已设置
    goto :test_conversion
) else (
    echo.
    echo 请手动将以下路径添加到系统环境变量PATH:
    echo   %CUDA_PATH%\bin
    echo.
    echo 然后重新运行此脚本
    pause
    exit /b 0
)

:use_pytorch_cuda
echo.
echo ========================================
echo 方案2: 从PyTorch复制CUDA库
echo ========================================
echo.

set TORCH_CUDA=C:\Users\Administrator\anaconda3\envs\yolo26\Lib\site-packages\torch\lib

if not exist "%TORCH_CUDA%" (
    echo [错误] 找不到PyTorch CUDA库
    echo 路径: %TORCH_CUDA%
    echo.
    echo 请确认yolo26环境中已安装PyTorch
    pause
    exit /b 1
)

echo PyTorch CUDA库位置: %TORCH_CUDA%
echo 目标位置: %TRT_ROOT%\bin
echo.
echo 将复制以下类型的文件:
echo   - cudart*.dll   (CUDA Runtime)
echo   - cublas*.dll   (CUDA BLAS)
echo   - cublasLt*.dll (CUDA BLAS Lt)
echo   - cudnn*.dll    (cuDNN)
echo.

set /p confirm="确认复制? (y/n): "
if /i not "%confirm%"=="y" (
    echo 操作已取消
    pause
    exit /b 0
)

echo.
echo 复制文件中...

REM 复制CUDA运行时库
for %%f in ("%TORCH_CUDA%\cudart*.dll") do (
    echo 复制: %%~nxf
    copy /y "%%f" "%TRT_ROOT%\bin\" >nul
)

REM 复制cuBLAS库
for %%f in ("%TORCH_CUDA%\cublas*.dll") do (
    echo 复制: %%~nxf
    copy /y "%%f" "%TRT_ROOT%\bin\" >nul
)

REM 复制cuDNN库
for %%f in ("%TORCH_CUDA%\cudnn*.dll") do (
    echo 复制: %%~nxf
    copy /y "%%f" "%TRT_ROOT%\bin\" >nul
)

REM 复制其他可能需要的库
for %%f in ("%TORCH_CUDA%\nvrtc*.dll") do (
    echo 复制: %%~nxf
    copy /y "%%f" "%TRT_ROOT%\bin\" >nul
)

echo.
echo [OK] 文件复制完成
goto :test_conversion

:manual_download
echo.
echo ========================================
echo 方案3: 手动下载指南
echo ========================================
echo.
echo 请手动下载以下文件并放到:
echo   %TRT_ROOT%\bin\
echo.
echo 需要的DLL文件:
echo   1. cudart64_110.dll  - CUDA Runtime 11.x
echo   2. cublas64_11.dll   - CUDA BLAS
echo   3. cublasLt64_11.dll - CUDA BLAS Lt
echo   4. cudnn64_8.dll     - cuDNN 8.x
echo.
echo 下载方式:
echo   方式1: 从NVIDIA官网下载完整的CUDA Toolkit 11.8
echo   方式2: 从已安装CUDA的其他机器复制
echo   方式3: 从DLL下载网站获取 (不推荐，安全风险)
echo.
pause
exit /b 0

:test_conversion
echo.
echo ========================================
echo 测试TensorRT转换
echo ========================================
echo.

set ONNX=%~dp0..\yanzhi20260115.onnx
set OUTPUT=%~dp0models\yanzhi20260115_4080s_fp16.engine

if not exist "%ONNX%" (
    echo [错误] 找不到ONNX模型: %ONNX%
    pause
    exit /b 1
)

if not exist "%~dp0models" mkdir "%~dp0models"

echo ONNX: %ONNX%
echo Output: %OUTPUT%
echo.
echo 开始转换 (5-10分钟)...
echo.

cd /d "%TRT_ROOT%\bin"

trtexec.exe ^
  --onnx="%ONNX%" ^
  --saveEngine="%OUTPUT%" ^
  --fp16 ^
  --workspace=12288 ^
  --verbose ^
  --device=0

echo.
if exist "%OUTPUT%" (
    echo ========================================
    echo 成功!
    echo ========================================
    echo.
    echo 生成的引擎文件:
    dir "%OUTPUT%"
    echo.
    echo 下一步: 编译C++代码并测试
) else (
    echo ========================================
    echo 转换失败
    echo ========================================
    echo.
    echo 请检查上面的错误信息
    echo.
    echo 如果仍然是DLL加载错误，可能需要:
    echo 1. 安装完整的CUDA Toolkit 11.8
    echo 2. 安装对应版本的cuDNN
    echo 3. 检查GPU驱动是否最新
)

echo.
cd /d "%~dp0"
pause
