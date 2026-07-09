@echo off
cd /d "%~dp0"

echo ========================================
echo Rebuild and Test - POST-PROCESSED Format Fix
echo ========================================
echo.

REM Find CMake
set CMAKE_CMD=cmake
where cmake >nul 2>&1
if errorlevel 1 (
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    ) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
        set "CMAKE_CMD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
)

cd build_trt

echo Rebuilding...
"%CMAKE_CMD%" --build . --config Release --target test_yanzhi20260120 --parallel

if errorlevel 1 (
    echo Build FAILED!
    cd ..
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCESS! Running test...
echo ========================================
echo.

cd ..\..

echo Testing with cached engine...
expert_test\build_trt\Release\test_yanzhi20260120.exe yanzhi20260120_cache.engine

echo.
pause
