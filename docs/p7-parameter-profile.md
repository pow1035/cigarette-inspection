# P7 参数快照与身份契约

## 目的

P7 不再用一个可自由填写的 `parameterVersion` 代表“实际检测参数”。每次运行同时保存已配置参数和检测器实际应用参数，两者各自生成稳定 SHA-256；逐帧结果必须携带已应用参数 SHA-256，身份漂移会在写入统计前被拒绝。

核心实现位于：

- `core/Sha256.h`
- `core/ProductParameterProfile.h`
- `core/ProductRuntimeState.h`
- `adapters/tensorrt/TensorRtDetector.h`

这些头文件只依赖 C++14 标准库。

## 三类 profile

| kind | 用途 | 约束 |
| --- | --- | --- |
| `LEGACY_DEEP_LEARNING_PAGE` | 品牌参数页的迁移/审计快照 | 恰好 9 个有序类别；ID 0-6 使用七个品牌阈值；ID 7-8 固定禁用、阈值 1.0；不得包含 detector/model/tensor 字段 |
| `DETERMINISTIC_FIXTURE` | 当前 local-only 产品 UI 的实际离线检测器 | 只记录 fixture profile、parameter 和 detector version；不得伪造 model、tensor 或类别参数 |
| `TENSORRT_OFFLINE` | P4/P7 TensorRT 离线批处理 | 完整 model SHA-256、输入/输出 tensor、尺寸、预处理、detector/parameter version 和 9 个逐类阈值/启用状态 |

当前产品 UI 使用 deterministic fixture。因此品牌页七个阈值会进入 `configuredParameters`，但 `configuredParametersApplied=false`；`appliedParameters` 明确记录 fixture。只有 TensorRT 路径通过严格配置加载后，configured 和 applied profile 才相同且标记为已应用。

## Canonical SHA-256

`ProductParameterProfile::canonicalPayload()` 使用固定字段顺序、键和值的长度前缀以及 IEEE-754 binary32 十六进制位模式。engine 文件路径、JSON 空白和 JSON 键顺序不进入身份；模型字节 SHA-256、tensor、尺寸、预处理、类别名称、阈值、启用状态和版本任一变化都会改变身份。

legacy 默认 profile 的冻结 golden SHA-256 为：

```text
a973097f62c125cab024aa3db40ce304bb0e7ce57b22e1565daf5cf5032605a6
```

`ProductStateTests` 同时校验 SHA-256 标准空串/`abc` 向量、golden profile、字段敏感性、engine 路径不敏感性、配置/帧级漂移拒绝和无部分统计写入。

## TensorRT JSON v2

`loadTensorRtConfig` 只接受 `cigvision-tensorrt-detector-v2`，要求完整 model SHA-256、9 个唯一类名和 9 个有限 `[0,1]` 阈值。加载过程先构造临时配置，验证 engine 实际 SHA-256 与声明一致且 profile 合法后才提交。批处理输出原子写入 `parameter-profile.json`；检测器的 `DetectionBatch` 独立回传实际 `parameterVersion` 与 `parameterSha256`，worker 原样传到帧结果，再与运行快照比较，避免从 UI 当前期望配置反填 SHA 形成同源比较。

loader 在 Qt 解析后仍扫描原始 JSON token，并按解码后的顶层 key 拒绝重复字段；未知、缺失、类型错误和重复禁用类同样拒绝。P8 严格预检还会在启动前独立执行同类门禁。目标 Windows/Qt runtime 仍需单独取证。

## 参数页与品牌持久化

深度学习参数页使用独立 `deepNGSet_widget`，七个控件限制为有限 `[0,1]`，只在显式点击保存时写入当前品牌 `para.ini` 的 `DeepLearningParams`。品牌切换会重新加载并刷新控件；缺失文件时保留构造阶段建立的 0.5 安全默认值。

页面提示这些阈值是品牌配置快照，不能在 `configuredParametersApplied=false` 时宣称检测器实际使用。

## 会话证据

`product-session.json` 保存：

- `configuredParametersApplied`
- configured/applied 两份完整 profile 及各自 SHA-256
- detector、parameter、model identity
- 最近结果的 `parameterSha256`

该接线当前只有源码与 SDK-free 契约证据；Mac 环境没有 Qt/MSVC，不能据此声称 Windows 页面、JSON 实际落盘或 TensorRT/GPU runtime 通过。
