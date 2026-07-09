@echo off
chcp 65001 >nul 2>&1

set SRC=C:\Users\Administrator\anaconda3\envs\yolo26\Lib\site-packages\torch\lib
set DST=D:\深度学习软件\TensorRT-8.6.1.6.Windows10.x86_64.cuda-11.8\TensorRT-8.6.1.6\bin

echo ========================================
echo 从PyTorch复制CUDA库到TensorRT
echo ========================================
echo.
echo Source: %SRC%
echo Target: %DST%
echo.

copy "%SRC%\cudart64_110.dll" "%DST%\" /Y
copy "%SRC%\cublas64_11.dll" "%DST%\" /Y
copy "%SRC%\cublasLt64_11.dll" "%DST%\" /Y
copy "%SRC%\cudnn_adv_infer64_8.dll" "%DST%\" /Y
copy "%SRC%\cudnn_adv_train64_8.dll" "%DST%\" /Y
copy "%SRC%\cudnn_cnn_infer64_8.dll" "%DST%\" /Y
copy "%SRC%\cudnn_cnn_train64_8.dll" "%DST%\" /Y
copy "%SRC%\cudnn_ops_infer64_8.dll" "%DST%\" /Y
copy "%SRC%\cudnn_ops_train64_8.dll" "%DST%\" /Y
copy "%SRC%\nvrtc64_112_0.dll" "%DST%\" /Y 2>nul

echo.
echo [OK] 文件复制完成
echo.

dir "%DST%\cudart*.dll"
dir "%DST%\cublas*.dll"
dir "%DST%\cudnn*.dll"

echo.
pause
