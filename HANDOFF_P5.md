# P5 项目交接说明

更新时间：2026-07-19  
当前阶段：P5（本地数据效果与冻结 pilot 评估）  
当前分支：`main`

## 1. 本轮已经完成的工作

### 1.1 中文人工标注工作台

已完善 `tools/p5_review_workbench/`：

- 中文界面；
- 支持“正常 / 缺陷 / 待确认”烟支级状态；
- 支持普通缺陷框与待确认框同时存在；
- 支持完成本张、保存进度、导出 pass1；
- 增加服务端完整性校验、非法请求和路径安全测试。

### 1.2 冻结 pilot 真值

已完成 30 张 pilot 的人工标注、双人复核和项目负责人批准：

- 标注人：肖朗；
- 复核人：小狼；
- OK / NG / REVIEW 各 10 张；
- 20 张 OK/NG 可进入指标；
- 10 张 REVIEW 必须排除；
- 正式可比较真值框 35 个。

本地正式证据目录为：

`artifacts/p5-reviewed-truth-20260719-124537/`

该目录受 `.gitignore` 保护，**不会上传 GitHub**。交接机器需要通过受控渠道单独复制，并核对其 `manifest.json`。

### 1.3 真值晋级与分析工具

新增：

- `scripts/p5_promote_reviewed_truth.py`：把获得批准的双人复核结果晋级为冻结真值；
- `scripts/p5_exploratory_consistency.py`：探索性一致性分析；
- `scripts/p5_visual_disagreement_pack.py`：生成预测/真值分歧可视化包；
- 上述脚本对应的 P5 自动测试。

### 1.4 临时 ONNX Runtime CPU 基线

已对冻结 pilot 执行 provisional fallback baseline：

- backend：ONNX Runtime 1.20.1 / CPUExecutionProvider；
- 模型：`yanzhi20260120.onnx`；
- 模型 SHA-256：`956554A92E8E7F9338B86E2B25FAE3E40F87DDF7F702E04B213AE46C5E26D0C4`；
- confidence threshold：0.25；
- IoU：0.50；
- 20 张进入指标，10 张 REVIEW 排除；
- 未训练、未调整阈值、未调整 NMS。

结果：

| 指标 | 数值 |
|---|---:|
| TP / FP / FN | 13 / 24 / 22 |
| Precision | 0.351351 |
| Recall | 0.371429 |
| F1 | 0.361111 |
| missed-NG | 3/10（30%） |
| false-NG | 3/10（30%） |

本地结果目录：

`artifacts/p5-fallback-baseline-20260719-161114/`

关键证据：

- `evaluation-report.json` SHA-256：`2BC9B7115BA67563308FCB70FE3C4435C8849B89346962B8148D6487917B24D3`
- `manifest.json` SHA-256：`9A9FD67471413EEBCD8CD57179A8048BF7E868F313035921459046B6735968D6`

该结果只是一份小样本诊断基线，**不是正式 TensorRT 基线、完整数据集准确率、产品验收或商业性能结论**。

### 1.5 质量门

当前 P5 回归：

```powershell
python -m unittest discover -s tests\p5 -p "test_*.py"
```

结果：76/76 PASS。

本轮还经过独立 reviewer 和独立 QA：返修后均为 PASS。证据记录见：

- `docs/progress-log.md`
- `docs/evidence-matrix.md`
- `docs/review-packet.md`
- `docs/review-results.md`

## 2. 当前最重要的问题

### KI-037：正式 P4 TensorRT 基线仍未恢复

当前机器上的三个已有 `.engine` 均不能被 TensorRT 8.6.1 反序列化，错误为：

`Magic tag does not match / Engine deserialization failed`

使用现有 ONNX 重建 engine 又在 `Mod` 节点失败，当前运行环境缺少相应插件。仓库中也没有找到原来编译好的：

- `test_yanzhi20260120.exe`；
- `test_yolo_trt_v2.exe`；
- 配套 DLL 和准确的原始 TensorRT/CUDA/plugin 运行环境。

因此正式 TensorRT 状态必须保持：`BLOCKED / NOT VERIFIED`。

### 数据量与类别覆盖不足

冻结 pilot 只有 30 张，其中仅 20 张进入指标。以下类别没有可比较 GT 支持：

- `jiamo`；
- `quezui`；
- `wuzi`；
- `jietou`。

尤其 `wuzi` 和 `jietou` 仍不能根据本次 pilot 得出性能结论。当前 pilot 不得用于训练或调参。

### 当前模型的明显诊断问题

- `yanbangzangwu`：13 个 GT 框全部漏检；
- `feiyan`：存在较多误报和漏检；
- 烟支级 missed-NG 与 false-NG 均为 30%。

这些结果用于指出后续数据/模型调查方向，不代表已经确定根因。

## 3. 下一步应该做什么

### 优先级 1：恢复正式 TensorRT 推理链

通过受控渠道取得以下任意一种材料：

1. 原编译好的推理 EXE 和配套 DLL；
2. 与目标机器、CUDA、TensorRT 版本兼容的 engine；
3. 原 engine 使用的准确 TensorRT/CUDA/plugin 工具链；
4. 能重新导出 TensorRT 兼容 ONNX 的训练/导出环境。

恢复后应：

1. 核对模型、engine、detector config 和运行环境哈希；
2. 对同一批冻结 30 张 pilot 重新推理；
3. 继续排除 10 张 REVIEW；
4. 使用 `scripts/p5_dataset_tools.py evaluate --engine ...` 生成正式报告；
5. 与 provisional ONNX fallback 结果进行差异分析；
6. 重新执行 reviewer、QA 和文档证据更新。

### 优先级 2：建立独立训练集和验证集

不得使用冻结 pilot 调整模型。应另外收集并标注：

- 独立训练集；
- 独立验证集；
- 覆盖设备、批次、时间和缺陷类别的样本；
- 特别补齐零支持和低支持类别。

如过程中需要新的人工标注，应明确记录标注规范、标注人、复核人、分歧处理和版本哈希。

### 优先级 3：开展误差分析

使用 `p5_visual_disagreement_pack.py` 生成分歧材料，优先调查：

1. `yanbangzangwu` 全部漏检的模型类别映射、图像预处理和导出后处理；
2. `feiyan` 误报是否来自框重复、类别混淆或训练数据偏差；
3. missed-NG / false-NG 的逐图原因；
4. ONNX 与恢复后的 TensorRT 输出是否数值一致。

不得根据冻结 pilot 反复修改阈值并挑选最好结果。

## 4. 新接手者的启动顺序

1. 阅读 `AGENTS.md`；
2. 阅读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`；
3. 阅读 `docs/known-issues.md`、`docs/code-audit.md`；
4. 阅读 `docs/p5-source-inventory.md` 和 `docs/p5-labeling-guide.md`；
5. 阅读 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-results.md`；
6. 从受控渠道恢复 `artifacts/` 证据和数据并核对 SHA-256；
7. 运行 76 项 P5 回归；
8. 只继续 P5，不跨入 P6/P7，也不恢复真实硬件/剔除链路。

## 5. Git 与数据边界

禁止提交：

- `artifacts/`；
- `.onnx`、`.engine` 或重新生成的模型构建物；
- 原始图片、标注数据包和大压缩包；
- `07_运行环境与依赖包/`；
- 账号、许可证、设备序列号、客户数据和机器敏感信息。

本交接 README 只描述证据位置和哈希，不包含本地数据本体。
