@echo off
chcp 65001 >nul 2>&1

set PYTHON=C:\Users\Administrator\anaconda3\envs\yolo26\python.exe

echo ========================================
echo YOLO26 Export (Using yolo26 environment)
echo ========================================
echo.

echo Python: %PYTHON%
%PYTHON% --version
echo.

echo [1/3] Installing ultralytics in yolo26 env...
%PYTHON% -m pip install ultralytics -q

echo.
echo [2/3] Verifying installation...
%PYTHON% -c "from ultralytics import YOLO; print('  OK - ultralytics installed')"

if errorlevel 1 (
    echo   ERROR - Installation failed
    pause
    exit /b 1
)

echo.
echo [3/3] Exporting models (5-10 minutes)...
echo.

%PYTHON% export_tensorrt.py

if errorlevel 1 (
    echo.
    echo Export FAILED - Check errors above
    pause
    exit /b 1
)

echo.
echo ======== SUCCESS ========
echo Generated files:
dir /b models\*.onnx 2>nul
dir /b models\*.engine 2>nul
echo.

pause
