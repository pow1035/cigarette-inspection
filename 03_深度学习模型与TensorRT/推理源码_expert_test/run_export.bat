@echo off
REM 自动检测并使用正确的Python环境运行导出脚本

echo ========================================
echo YOLO26 TensorRT 导出工具
echo ========================================
echo.

REM 尝试检测Python
set PYTHON_CMD=

REM 方法1: 检查conda
where conda >nul 2>&1
if not errorlevel 1 (
    echo 检测到Conda环境
    echo.
    echo 可用的环境:
    call conda env list
    echo.
    echo 请选择一个环境名称 ^(例如: base^):
    set /p ENV_NAME="环境名称: "

    echo.
    echo 激活环境: !ENV_NAME!
    call conda activate !ENV_NAME!
    set PYTHON_CMD=python
    goto :run_script
)

REM 方法2: 使用默认Python
where python >nul 2>&1
if not errorlevel 1 (
    python --version >nul 2>&1
    if not errorlevel 1 (
        set PYTHON_CMD=python
        goto :run_script
    )
)

REM 方法3: 尝试anaconda路径
if exist "C:\Users\Administrator\anaconda3\python.exe" (
    set PYTHON_CMD=C:\Users\Administrator\anaconda3\python.exe
    goto :run_script
)

REM 方法4: 尝试ProgramData anaconda
if exist "C:\ProgramData\anaconda3\python.exe" (
    set PYTHON_CMD=C:\ProgramData\anaconda3\python.exe
    goto :run_script
)

REM 如果都没找到
echo.
echo 错误: 未找到可用的Python
echo.
echo 请手动运行:
echo   1. 打开Anaconda Prompt
echo   2. 激活环境: conda activate your_env
echo   3. 运行: python diagnose_and_export.py
echo.
pause
exit /b 1

:run_script
echo.
echo 使用Python: %PYTHON_CMD%
%PYTHON_CMD% --version
echo.

REM 运行诊断和导出脚本
%PYTHON_CMD% diagnose_and_export.py

if errorlevel 1 (
    echo.
    echo 导出失败! 请检查错误信息
    echo.
    pause
    exit /b 1
)

echo.
echo ========================================
echo 导出成功!
echo ========================================
echo.
echo 生成的文件在 models\ 目录下
echo.
pause
