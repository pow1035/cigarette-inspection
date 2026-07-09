@echo off
cd /d "%~dp0"

echo ========================================
echo YOLO26 TensorRT - Yanzhi20260120 Test
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

if not exist "%OPENCV_DIR%" (
    echo [WARNING] OpenCV path may be incorrect: %OPENCV_DIR%
) else (
    echo [OK] OpenCV: %OPENCV_DIR%
)

REM Check if model exists
if not exist "..\yanzhi20260120.engine" (
    echo [WARNING] Model file not found: ..\yanzhi20260120.engine
    echo Please make sure the model file is in the correct location.
    echo.
) else (
    echo [OK] Model file found: ..\yanzhi20260120.engine
)

REM Check if test images exist
if not exist "..\inference\test" (
    echo [WARNING] Test images directory not found: ..\inference\test
    echo.
) else (
    echo [OK] Test images directory: ..\inference\test
)

echo.
echo Creating/Using build directory...
if not exist build_trt mkdir build_trt
cd build_trt

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
    echo Trying with VS 2019...

    "%CMAKE_CMD%" .. ^
      -G "Visual Studio 16 2019" ^
      -A x64 ^
      -DTENSORRT_ROOT="%TENSORRT_ROOT%" ^
      -DOpenCV_DIR="%OPENCV_DIR%" ^
      -DCUDA_TOOLKIT_ROOT_DIR="%CUDA_DIR%" ^
      -DCMAKE_BUILD_TYPE=Release

    if errorlevel 1 (
        echo.
        echo CMake Configuration still FAILED!
        echo Please check your Visual Studio installation.
        cd ..
        pause
        exit /b 1
    )
)

echo.
echo ========================================
echo Step 2: Build Project
echo ========================================
echo.

"%CMAKE_CMD%" --build . --config Release --target test_yanzhi20260120 --parallel

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

if exist "Release\test_yanzhi20260120.exe" (
    echo [OK] Executable generated
    echo Location: %CD%\Release\test_yanzhi20260120.exe
    echo.
    for %%A in ("Release\test_yanzhi20260120.exe") do echo Size: %%~zA bytes
) else (
    echo [ERROR] Executable not found!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Running Test
echo ========================================
echo.

cd ..\..

echo Current directory: %CD%
echo.
echo Running inference on test images...
echo.

expert_test\build_trt\Release\test_yanzhi20260120.exe

if errorlevel 1 (
    echo.
    echo ========================================
    echo Test execution FAILED!
    echo ========================================
    echo.
) else (
    echo.
    echo ========================================
    echo Test execution completed!
    echo ========================================
    echo.
    echo Check results in: inference\test_results\
)

echo.
pause
