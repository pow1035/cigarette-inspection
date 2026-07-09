@echo off
chcp 65001 >nul 2>&1

set SRC=C:\Users\Administrator\anaconda3\envs\yolo26\Lib\site-packages\torch\lib
set DST=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin

echo ========================================
echo 复制所有CUDA库到TensorRT
echo ========================================
echo.

copy "%SRC%\cufft64_10.dll" "%DST%\" /Y
copy "%SRC%\curand64_10.dll" "%DST%\" /Y
copy "%SRC%\cusolver64_11.dll" "%DST%\" /Y
copy "%SRC%\cusparse64_11.dll" "%DST%\" /Y
copy "%SRC%\cusolverMg64_11.dll" "%DST%\" /Y 2>nul
copy "%SRC%\cufftw64_10.dll" "%DST%\" /Y 2>nul
copy "%SRC%\cudnn64_8.dll" "%DST%\" /Y 2>nul

echo.
echo [OK] 复制完成
echo.
pause
