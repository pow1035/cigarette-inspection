@echo off
chcp 65001 >nul 2>&1

echo ========================================
echo YOLO26 TensorRT Export Tool
echo ========================================
echo.

echo Checking environment...
echo.

REM Get current conda environment
for /f "tokens=*" %%i in ('conda info --envs ^| findstr /C:"*"') do set CURRENT_ENV=%%i
echo Current conda environment: %CURRENT_ENV%

REM Check if in yolo26 environment
echo %CURRENT_ENV% | findstr /C:"yolo26" >nul
if errorlevel 1 (
    echo.
    echo WARNING: You are not in the 'yolo26' environment
    echo Current environment: %CURRENT_ENV%
    echo.
    echo Please activate yolo26 environment first:
    echo   conda activate yolo26
    echo.
    echo Or continue anyway? (y/n^)
    set /p CONTINUE=
    if /i not "%CONTINUE%"=="y" (
        echo Cancelled.
        pause
        exit /b 1
    )
)

echo.
echo Installing/Upgrading ultralytics in current environment...
pip install ultralytics --upgrade

echo.
echo ========================================
echo Running export script
echo ========================================
echo.
echo This will take 5-10 minutes...
echo.

python export_tensorrt.py

if errorlevel 1 (
    echo.
    echo ========================================
    echo Export FAILED
    echo ========================================
    echo.
    echo Please check the error messages above.
    echo.
    echo Common issues:
    echo 1. Not in correct conda environment - run: conda activate yolo26
    echo 2. Missing dependencies - check error message
    echo 3. TensorRT not installed or not found
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo Export SUCCESSFUL
echo ========================================
echo.
echo Generated files:
dir /b models\*.onnx 2>nul
dir /b models\*.engine 2>nul
echo.
echo Next step: Compile C++ code
echo See START_HERE.md for details
echo.

pause
