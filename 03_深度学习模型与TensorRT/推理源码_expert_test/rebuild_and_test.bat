@echo off
cd /d "%~dp0"

echo ========================================
echo Rebuilding test_yanzhi20260120
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

    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        echo Found CMake in VS 2022 Community
        goto cmake_found
    )

    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        echo Found CMake in VS 2019 Community
        goto cmake_found
    )

    echo [ERROR] CMake not found!
    pause
    exit /b 1
) else (
    echo Found CMake in PATH
)

:cmake_found

cd build_trt

echo.
echo ========================================
echo Building test_yanzhi20260120
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
    echo [OK] Executable: %CD%\Release\test_yanzhi20260120.exe
    for %%A in ("Release\test_yanzhi20260120.exe") do echo Size: %%~zA bytes
) else (
    echo [ERROR] Executable not found!
    cd ..
    pause
    exit /b 1
)

cd ..\..

echo.
echo ========================================
echo Running Test
echo ========================================
echo.

expert_test\build_trt\Release\test_yanzhi20260120.exe

echo.
pause
