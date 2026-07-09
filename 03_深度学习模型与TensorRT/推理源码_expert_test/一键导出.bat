@echo off
REM 使用UTF-8编码
chcp 65001 >nul

echo ========================================
echo 安装并导出模型
echo ========================================
echo.

echo [1/3] 检查conda...
where conda >nul 2>&1
if errorlevel 1 (
    echo 错误: 未找到conda
    echo 请在Anaconda Prompt中运行
    pause
    exit /b 1
)

echo [2/3] 安装ultralytics...
pip install ultralytics -q

if errorlevel 1 (
    echo 安装失败
    pause
    exit /b 1
)

echo [3/3] 导出模型 (需要5-10分钟)...
python export_tensorrt.py

if errorlevel 1 (
    echo 导出失败
    pause
    exit /b 1
)

echo.
echo ======== 完成 ========
echo 生成的文件:
dir /b models\*.onnx 2>nul
dir /b models\*.engine 2>nul
echo.
pause
