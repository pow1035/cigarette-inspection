@echo off
cd /d "%~dp0"

echo ========================================
echo YOLO26 TensorRT v2 Build Script
echo ========================================
echo.

REM Path Configuration
set TENSORRT_ROOT=D:/深度学习软件/TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8/TensorRT-8.6.1.6
set OPENCV_DIR=E:/code/opencv/build/x64/vc16/lib
set CUDA_DIR=C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8

REM Try to find CMake
set CMAKE_CMD=cmake
where cmake >nul 2>&1
if errorlevel 1 (
    echo CMake not found in PATH, trying Visual Studio locations...

    REM Try VS 2022 Community
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        echo Found CMake in VS 2022 Community
        goto cmake_found
    )

    REM Try VS 2022 Professional
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        echo Found CMake in VS 2022 Professional
        goto cmake_found
    )

    REM Try VS 2019 Community
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        echo Found CMake in VS 2019 Community
        goto cmake_found
    )

    echo [ERROR] CMake not found!
    echo.
    echo Please install CMake from: https://cmake.org/download/
    echo Or install Visual Studio with C++ CMake tools
    echo.
    echo Or install via winget:
    echo   winget install -e --id Kitware.CMake
    pause
    exit /b 1
) else (
    echo Found CMake in PATH
)

:cmake_found

echo.
echo Checking paths...
if not exist "%TENSORRT_ROOT%\include\NvInfer.h" (
    echo [ERROR] TensorRT not found: %TENSORRT_ROOT%
    pause
    exit /b 1
)
echo [OK] TensorRT: %TENSORRT_ROOT%

if not exist "%OPENCV_DIR%\include\opencv2\opencv.hpp" (
    echo [WARNING] OpenCV path may be incorrect: %OPENCV_DIR%
) else (
    echo [OK] OpenCV: %OPENCV_DIR%
)

echo.
echo Preparing CMakeLists.txt...
copy /Y CMakeLists_TRT_v2.txt CMakeLists.txt >nul
if errorlevel 1 (
    echo [ERROR] Cannot copy CMakeLists_TRT_v2.txt
    pause
    exit /b 1
)
echo [OK] CMakeLists.txt ready

echo.
echo Creating build directory...
if not exist build_trt mkdir build_trt
cd build_trt

if exist CMakeCache.txt (
    echo Cleaning old CMake cache...
    del CMakeCache.txt
)

echo.
echo ========================================
echo Step 1: CMake Configuration
echo ========================================
echo.

REM Run CMake configuration
"%CMAKE_CMD%" .. ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DTENSORRT_ROOT="%TENSORRT_ROOT%" ^
  -DOpenCV_DIR="%OPENCV_DIR%" ^
  -DCUDA_TOOLKIT_ROOT_DIR="%CUDA_DIR%" ^
  -DCMAKE_BUILD_TYPE=Release

if errorlevel 1 (
    echo.
    echo ========================================
    echo CMake Configuration FAILED!
    echo ========================================
    echo.
    echo Possible reasons:
    echo 1. Visual Studio 2022 not installed
    echo 2. OpenCV path incorrect
    echo 3. TensorRT path incorrect
    echo.
    echo If VS version issue, try editing script to use:
    echo   -G "Visual Studio 16 2019"
    echo.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Step 2: Build Project
echo ========================================
echo.

"%CMAKE_CMD%" --build . --config Release --parallel

if errorlevel 1 (
    echo.
    echo ========================================
    echo Build FAILED!
    echo ========================================
    echo.
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCESS!
echo ========================================
echo.

if exist "Release\test_yolo_trt_v2.exe" (
    echo [OK] Executable generated
    echo Location: %CD%\Release\test_yolo_trt_v2.exe
    echo.
    for %%A in ("Release\test_yolo_trt_v2.exe") do echo Size: %%~zA bytes
) else (
    echo [WARNING] Executable not found
)

echo.
echo ========================================
echo Copying DLLs
echo ========================================
echo.

echo Copying TensorRT DLLs...
copy "%TENSORRT_ROOT%\lib\nvinfer.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\lib\nvinfer_plugin.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\lib\nvonnxparser.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\lib\nvinfer_dispatch.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\lib\nvinfer_lean.dll" "Release\" /Y >nul 2>&1

echo Copying CUDA DLLs...
copy "%TENSORRT_ROOT%\bin\cudart64_110.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\bin\cublas64_11.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\bin\cublasLt64_11.dll" "Release\" /Y >nul 2>&1
copy "%TENSORRT_ROOT%\bin\cudnn*.dll" "Release\" /Y >nul 2>&1

echo [OK] DLLs copied to Release directory

echo.
echo ========================================
echo Next Steps
echo ========================================
echo.
echo 1. Prepare test image (e.g., test.jpg)
echo.
echo 2. Run benchmark (first time builds engine, 5-10 min):
echo    cd Release
echo    test_yolo_trt_v2.exe benchmark ..\\..\\yanzhi20260115.onnx test.jpg
echo.
echo 3. Test image:
echo    test_yolo_trt_v2.exe image ..\\..\\yanzhi20260115.onnx test.jpg
echo.
echo 4. Test video:
echo    test_yolo_trt_v2.exe video ..\\..\\yanzhi20260115.onnx test.mp4
echo.
echo 5. Use cached engine (after first run):
echo    test_yolo_trt_v2.exe benchmark ..\\..\\yanzhi20260115_cache.engine test.jpg
echo.

cd ..
pause
