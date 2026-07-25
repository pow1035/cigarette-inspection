# P5 项目交接说明

更新时间：2026-07-25
当前阶段：P8（本地稳定性、部署与交付预验收）；P5/P6/P7 外部阻断保留
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

当前结果：100/100 PASS（C6 历史快照为 76/76；新增 readiness 回归为 24 项）。

接手机器还必须先运行受控输入就绪检查：

```bash
python3 scripts/p5_input_readiness.py --require-reviewed --require-fallback
```

exit 0 才表示指定范围输入可开始；exit 2 表示 fresh clone 缺少被 `.gitignore` 排除的 reviewed-truth/fallback artifact；exit 3 表示已恢复输入存在错配或格式问题。当前 fresh clone 的实际结果为 exit 2。

readiness 只证明 supplied manifest 的内部一致性，不是 manifest 自身认证。受控恢复时还必须核对外部 evidence-manifest SHA-256；fallback 摘要已在上文记录，reviewed evidence-manifest 摘要当前未随 clone 提供，缺少该信任根时不得宣称 pilot ready。

本轮还经过独立 reviewer 和独立 QA：返修后均为 PASS。证据记录见：

- `docs/progress-log.md`
- `docs/evidence-matrix.md`
- `docs/review-packet.md`
- `docs/review-results.md`

## 2. 当前最重要的问题

### KI-039：正式 P4 TensorRT 基线仍未恢复

当前机器上的三个已有 `.engine` 均不能被 TensorRT 8.6.1 反序列化，错误为：

`Magic tag does not match / Engine deserialization failed`

使用现有 ONNX 重建 engine 又在 `Mod` 节点失败，当前运行环境缺少相应插件。仓库中也没有找到原来编译好的：

- `test_yanzhi20260120.exe`；
- `test_yolo_trt_v2.exe`；
- 配套 DLL 和准确的原始 TensorRT/CUDA/plugin 运行环境。

因此正式 TensorRT 状态必须保持：`BLOCKED / NOT VERIFIED`。

### KI-040：受控 P5 artifact 未随 fresh clone 提供

`artifacts/p5-reviewed-truth-20260719-124537/` 和 `artifacts/p5-fallback-baseline-20260719-161114/` 受 `.gitignore` 保护，当前接手目录没有这两份输入。恢复前只能运行 readiness 和代码回归，不能复算指定范围 pilot。

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

### 优先级 4：P8 本机收口与目标机交接（当前工作入口）

P5 的正式 TensorRT 与受控 artifact 仍由 KI-039/KI-040 阻断，不应因此暂停本地路线。P6 本地切片已完成，Qt/Windows runtime 保留为目标机任务。当前 P7-01A 已完成：

- `ProductRuntimeState` 提供有界、线程安全的运行身份、生命周期、统计、最近结果、复核和诊断状态；
- 离线 worker 已发送 frame/station/camera/cigarette/defect/error 元数据，运行页从状态快照刷新；
- “缺陷查询”按钮已进入深色最近 NG/错误复核页，支持具名确认、需修正和误报标记；
- SDK-free 产品状态当前 8/8；typed configured/applied profile、canonical/golden SHA-256、帧级漂移拒绝、TensorRT v2 配置绑定和品牌七阈值页面/持久化已接入。当前 UI 构造固定 local-only，不初始化相机或 DAQNavi；Qt/Windows 页面 runtime 和 Computer Use 尚未取得。
- 首轮独立 reviewer/QA 的 8 项发现已全部修复；修复后 reviewer 与 QA 均 PASS，另覆盖 TSan、ASan/UBSan、重复压力和启动窗口 stop 探针。

P7-01B 统计/诊断页和 P7-01C 配置身份/安全退出的本地源码已实现。P8 测试集保持 76 项（preflight 17、soak 9、fixture release 17、Windows wrapper/collector 静态契约 2、package manifest generator 7、跨机 evidence verify/import 24），PowerShell 静态门保持 13 个脚本与 7/7 CLI 契约。目标机先用 `p8_package_manifest.py` 在 package 外生成 manifest，并在 Package、EvidenceRoot、整个 DeploymentRoot 外准备 exactly 32-byte HMAC key；建议 `D:\CigVision-secure\p8-evidence.key`。不要预采集或传入历史 Windows/GPU 报告。v4 wrapper 每次生成 challenge，复制 collector 到 provenance 后以 mandatory `-RepositoryRoot $repoRoot` 立即现场采集 v2 报告；RepositoryRoot 与 OutputDirectory 不得重叠，不能由 provenance 脚本的 `$PSScriptRoot` 推导仓库根。manifest 以 `CreateNew + Flush(true)` 写入固定 UTF-8 bytes，SHA/HMAC 针对同一 bytes 计算并复读确认未漂移。可信 verifier 精确复验 host-input、preflight 9 项、采集窗口和 7 个 provenance 受信源，并要求带外 SHA、HMAC 与 bundle 外 key；receipt 为 v4。返修前 full gate 曾通过，返修后的最终 full gate/reviewer/QA 待执行。真实 Windows + Qt/HALCON/MVS/DAQNavi/CUDA/TensorRT/OpenCV、数据、许可和硬件仍是外部阻断；HMAC 只认证证据。

## 4. 新接手者的启动顺序

1. 阅读 `AGENTS.md`；
2. 阅读 `docs/requirements.md`、`docs/architecture.md`、`docs/task-plan.md`；
3. 阅读 `docs/known-issues.md`、`docs/code-audit.md`；
4. 阅读 `docs/p5-source-inventory.md` 和 `docs/p5-labeling-guide.md`；
5. 阅读 `docs/progress-log.md`、`docs/evidence-matrix.md`、`docs/review-results.md`；
6. 从受控渠道恢复 `artifacts/` 证据和数据并核对 SHA-256（若要复算 P5）；
7. 先运行 `./scripts/run_all_local_gates.sh --full`，再按 `docs/windows-target-execution.md` 与 P6/P7 目标机 Qt/Windows 验证清单执行；本机不得伪造目标机结果；
8. P5 外部输入恢复后再运行完整 P5 回归；任何情况下都不恢复真实硬件/剔除链路。

## 5. Git 与数据边界

禁止提交：

- `artifacts/`；
- `.onnx`、`.engine` 或重新生成的模型构建物；
- 原始图片、标注数据包和大压缩包；
- `07_运行环境与依赖包/`；
- 账号、许可证、设备序列号、客户数据和机器敏感信息。

本交接 README 只描述证据位置和哈希，不包含本地数据本体。
