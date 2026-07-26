# YOLO TensorRT C++ 后处理修复说明

<!-- HISTORICAL_TENSORRT_PROTOTYPE -->
> ⚠️ 历史原型资料：仅保留用于追溯，不是当前构建、性能或交付证据。当前项目状态与验证范围以根目录 [README](../../README.md) 为准。

## 问题描述

C++ 版本的推理程序无法检测到目标，而 Python 版本工作正常。

## 根本原因

**错误的输出格式假设**

原代码假设 YOLO 输出格式为：
```
[1, 300, 6]  ❌ 错误
其中 6 = [x, y, w, h, confidence, class_id]
```

**实际的 YOLO v8/v11 输出格式为：**
```
[1, 84, 8400]  ✓ 正确
其中：
- 84 = 4 (bbox坐标) + 80 (每个类别的分数)
- 8400 = 所有 anchor 点的数量
```

## 关键差异

| 方面 | 原实现（错误） | YOLO v8/v11（正确） |
|------|---------------|-------------------|
| 输出形状 | [1, 300, 6] | [1, 84, 8400] |
| 检测数量 | 硬编码 300 | 8400 个 anchor 点 |
| Confidence | 单个值 det[4] | 需要从 80 个类别分数中取最大值 |
| Class ID | 单个值 det[5] | 通过 argmax 从 80 个分数获得 |
| 数据布局 | 行优先 [N, 6] | 列优先 [84, 8400] |

## 修复内容

### 1. 添加输出维度成员变量 (yolo_trt_v2.h)

```cpp
// 新增成员变量
int num_classes_;      // 类别数量（例如 80）
int num_anchors_;      // anchor 点数量（例如 8400）
int output_features_;  // 输出特征维度（例如 84）
```

### 2. 动态获取输出维度 (yolo_trt_v2.cpp)

在 `buildEngineFromOnnx` 和 `loadEngine` 函数中添加：

```cpp
// 解析 YOLO 输出维度
output_features_ = outputDims.d[1];  // 84
num_anchors_ = outputDims.d[2];      // 8400
num_classes_ = output_features_ - 4; // 80

std::cout << "Parsed YOLO dimensions:" << std::endl;
std::cout << "  Classes: " << num_classes_ << std::endl;
std::cout << "  Anchors: " << num_anchors_ << std::endl;
std::cout << "  Features: " << output_features_ << std::endl;
```

### 3. 重写 postprocess 函数 (yolo_trt_v2.cpp)

**关键改进：**

#### 3.1 正确的数据索引

```cpp
// YOLO v8 输出布局：[features, anchors]
// 对于第 i 个 anchor：
float cx = output[0 * num_anchors_ + i];  // x 坐标
float cy = output[1 * num_anchors_ + i];  // y 坐标
float w  = output[2 * num_anchors_ + i];  // 宽度
float h  = output[3 * num_anchors_ + i];  // 高度
```

#### 3.2 正确的类别分数处理

```cpp
// 找到最大类别分数
float max_score = 0.0f;
int max_class_id = 0;
for (int c = 0; c < num_classes_; ++c) {
    float score = output[(4 + c) * num_anchors_ + i];
    if (score > max_score) {
        max_score = score;
        max_class_id = c;
    }
}
```

#### 3.3 使用 OpenCV NMS

```cpp
// 使用 OpenCV 的 NMS 实现（更高效）
std::vector<int> indices;
cv::dnn::NMSBoxes(boxes, confidences, conf_threshold_, iou_threshold_, indices);
```

## 如何重新编译和测试

### 方式 1: 使用便捷脚本（推荐）

```cmd
cd E:\code\yolo26\expert_test
rebuild_and_test.bat
```

这个脚本会：
1. 自动查找 CMake
2. 重新编译 `test_yanzhi20260120`
3. 自动运行测试

### 方式 2: 手动编译

```cmd
cd E:\code\yolo26\expert_test\build_trt

# 使用 CMake 编译
cmake --build . --config Release --target test_yanzhi20260120

# 运行测试
cd ..\..
expert_test\build_trt\Release\test_yanzhi20260120.exe
```

## 预期结果

修复后，C++ 版本应该：
- ✅ 检测到与 Python 版本相同的目标
- ✅ 输出正确的边界框坐标
- ✅ 显示正确的类别 ID 和置信度
- ✅ 性能更快（C++ 直接调用 TensorRT API）

## 验证步骤

1. **检查输出维度**
   程序启动时会打印：
   ```
   Output dimensions: 1x84x8400
   Parsed YOLO dimensions:
     Classes: 80
     Anchors: 8400
     Features: 84
   ```

2. **对比检测结果**
   - Python 输出：`runs/detect/predict*/`
   - C++ 输出：`inference/test_results/`

   检测框和类别应该基本一致

3. **性能对比**
   C++ 版本通常会快 2-5 倍

## 技术细节

### YOLO v8 输出张量布局

```
Shape: [1, 84, 8400]

内存布局（行优先）：
[x0, x1, x2, ..., x8399,           # 第 0 行：所有 anchor 的 x 坐标
 y0, y1, y2, ..., y8399,           # 第 1 行：所有 anchor 的 y 坐标
 w0, w1, w2, ..., w8399,           # 第 2 行：所有 anchor 的宽度
 h0, h1, h2, ..., h8399,           # 第 3 行：所有 anchor 的高度
 cls0_0, cls0_1, ..., cls0_8399,   # 第 4 行：类别 0 的分数
 cls1_0, cls1_1, ..., cls1_8399,   # 第 5 行：类别 1 的分数
 ...
 cls79_0, cls79_1, ..., cls79_8399] # 第 83 行：类别 79 的分数
```

### 为什么原代码检测不到目标？

1. **硬编码 300 个检测**：只检查了前 300 个位置，实际有 8400 个
2. **错误的数据索引**：假设 `det[4]` 是 confidence，实际上索引完全错误
3. **数据布局理解错误**：YOLO v8 是列优先（转置）布局，不是行优先

## 参考文档

- YOLO v8 官方输出格式：https://github.com/ultralytics/ultralytics
- TensorRT Python API vs C++ API 输出一致性
- OpenCV NMS 实现：cv::dnn::NMSBoxes

## 修复验证

运行以下命令验证修复：

```cmd
cd E:\code\yolo26
compare_python_cpp.bat
```

这会同时运行 Python 和 C++ 版本，可以直接对比结果。
