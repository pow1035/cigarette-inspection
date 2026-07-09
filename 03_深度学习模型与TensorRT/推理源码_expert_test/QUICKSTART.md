# 🚀 快速启动指南

欢迎使用YOLO26 TensorRT C++部署项目!

## 📋 你有什么?

✅ 完整的TensorRT导出脚本(Python)
✅ 高性能C++推理引擎
✅ 针对RTX 3080Ti和4080S优化
✅ 完整的测试和部署工具

## 🎯 你需要做什么?

### 第一步: 导出模型 (5-10分钟)

**打开Anaconda Prompt,运行:**

```cmd
cd expert_test
conda activate your_env
python manual_export.py
```

这将生成:
- `models/yanzhi20260115.onnx`
- `models/yanzhi20260115_4080s_fp16.engine` (或`_3080ti_fp16.engine`)

### 第二步: 编译C++代码 (2-5分钟)

**打开Visual Studio命令提示符,运行:**

```cmd
cd expert_test
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

这将生成:
- `build/bin/Release/test_yolo_tensorrt.exe`

### 第三步: 测试运行

```cmd
# 性能测试
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine bench

# 图像测试(准备一张test.jpg)
build\bin\Release\test_yolo_tensorrt.exe models\yanzhi20260115_4080s_fp16.engine image test.jpg result.jpg
```

## 🎉 完成!

现在你可以:
1. ✅ 将引擎集成到其他项目
2. ✅ 使用C++进行高性能推理
3. ✅ 部署到生产环境

## 📚 需要帮助?

| 问题类型 | 查看文档 |
|---------|---------|
| 详细安装步骤 | `README.md` |
| 快速部署指南 | `DEPLOYMENT_GUIDE.md` |
| 项目概览 | `PROJECT_SUMMARY.md` |
| 环境问题 | 运行`check_environment.bat` |

## ⚠️ 常见问题

**Q: Python命令失败?**
A: 使用完整路径或conda activate

**Q: 找不到TensorRT?**
A: 编辑`CMakeLists.txt`,设置正确的TensorRT路径

**Q: 显存不足?**
A: 编辑`manual_export.py`,减小workspace大小

## 💡 提示

- 导出过程需要5-10分钟,是正常的
- 确保GPU驱动是最新的
- 使用conda环境可以避免很多问题

---

**开始吧! Good luck! 🍀**
