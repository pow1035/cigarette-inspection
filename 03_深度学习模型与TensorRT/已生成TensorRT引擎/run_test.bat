@echo off
cd /d "%~dp0"
test_yolo_trt_v2.exe image ..\..\yanzhi20260115.onnx E:\code\yolo26\datasets\coco128\images\train2017\000000000009.jpg
pause
