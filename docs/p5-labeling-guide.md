# P5 标注与效果评估规范

## 真值门

- `unlabeled`：只有数据清单，不允许评估。
- `preannotated`：来自模型或历史带框图，只用于加速人工修订，`is_ground_truth=false`。
- `annotated`：首轮人工标注已完成、尚未独立复核，`is_ground_truth=false`，不得进入准确率报告。
- `reviewed`：人工完成并复核，`is_ground_truth=true`，才允许进入准确率报告。
- reviewed 图必须记录不同的 `annotated_by` 和 `reviewed_by`，授权状态必须为 `approved`；预测特有的 score、detector version、source bbox 或 P4 标记必须清除并由人工重新确认。
- 同一 SHA-256 的重复图只保留一个 canonical 标注对象；重复文件不得跨训练、验证或测试划分。
- 未确认来源、授权或业务类别的样本保持 `REVIEW`，不能强行纳入准确率分母。

## 类别目录

机器可读目录为 `config/p5-class-catalog.json`。前七类来自现有 Qt `DeepLearningParams` 和 UI 文案，但仍需业务批准；后两类不得靠拼音猜测。

| ID | 模型名 | 当前中文 | 标注状态 |
| --- | --- | --- | --- |
| 0 | `dakoucuoya` | 搭口错牙 | 可试标，需确认“错牙/搓牙”统一术语 |
| 1 | `feiyan` | 飞烟 | 可试标 |
| 2 | `jiamo` | 夹末 | 可试标，需区分烟支/水松纸位置 |
| 3 | `lvzuizhezhou` | 滤嘴皱褶 | 可试标，需确认是否包含水松纸泡皱 |
| 4 | `quezui` | 缺滤嘴 | 可试标 |
| 5 | `yanbangposun` | 烟棒破损 | 可试标，需确认是否包含刺破 |
| 6 | `yanbangzangwu` | 烟棒脏污 | 可试标，需确认黄斑/污渍边界 |
| 7 | `wuzi` | 待业务确认 | 可框选为待确认，不得标为正式 NG |
| 8 | `jietou` | 待业务确认 | 可框选为待确认，不得标为正式 NG |

## COCO 扩展字段

标准 `images/categories/annotations` 之外，每个 image 必须包含：

- `sha256`：原图哈希。
- `source_group`：文件名前缀/日期来源组。
- `annotation_status`：`unlabeled`、`preannotated`、`annotated` 或 `reviewed`。
- `is_ground_truth`：只有 reviewed 可为 true。
- `cigarette_decision`：`OK`、`NG` 或 `REVIEW`；REVIEW 图保留在冻结 split 的完整性检查中，但排除在准确率分母外并单独计数。
- `authorization_status`：当前默认为 `unverified`，确认后才能更新。

每个 annotation 使用 COCO `[x, y, width, height]`，并保留 `category_id`、`area`、`iscrowd=0`。预标注可额外记录 `score`、`detector_version` 和 `source=prediction`。

正式评估还必须提供基于 `config/p5-ground-truth-attestation.template.json` 的双人复核声明。声明绑定 ground truth、数据 manifest 和类别目录 SHA-256；评估报告另外绑定 prediction、模型、engine 和 detector config SHA-256。该本地声明提供审计链，但不等同于不可伪造的数字签名，最终商业验收仍需责任人签署。

### 旧工作台单名导出的审计处理

若实际流程由不同人员逐页标注与检查，但旧工作台只持久化一个人员名，不得原地修改 pass1 或直接调用 reviewed 导出。项目负责人必须补充确认标注人、复核人、复核过程和数据授权；晋级工具必须绑定原 pass1、pilot manifest、预测包和类别目录哈希，另行生成 approved/reviewed 数据与 attestation。原 pass1 继续保持 is_ground_truth=false 作为历史证据。REVIEW 图片可记录为已复核 split 成员，但其参考框仅留在 pass1，正式真值中不保留，评估时整图排除。

## 框标注规则

1. 框紧贴可见缺陷，不把整支烟或整片背景作为缺陷框。
2. 同一物理缺陷、同一类别只保留一个框；模型产生的重叠候选由人工合并或删除。
3. 两个空间分离的缺陷分别标框；不同类别确实同时存在时可分别标注。
4. 缺陷跨出图像、边界无法判断或类别不确定时，将整图 decision 设为 `REVIEW` 并写备注；类别 7/8 可保留定位框作为待确认信息，并可与类别 0–6 的已确认缺陷框在同一张 `REVIEW` 图片中共存；填写备注后可完成首标，但该图不得标为正式 `NG`，也不得进入准确率分母。
5. 无九类缺陷的完整可判图可标 `OK` 且 annotations 为空；不能把“没有标框”自动解释为 OK。
6. 报告中出现但九类目录未覆盖的长短、双层水松纸等缺陷先记为 `REVIEW`，不强塞入近似类别。
7. 历史带框图、传统算法区域和 P4 TensorRT 结果都不是人工真值。

## 试点与划分

1. 先从不同文件名前缀、日期、模型输出类别和空检候选中分层选择 20-30 个唯一样本。
2. 至少两轮：首标和复核；有分歧的图片保持 REVIEW，直到形成一致规则。
3. 试点结束后再冻结完整 113 图的 split；同一哈希、近邻连拍和同一事件组不得跨 split。
4. 冻结测试集只用于最终比较，不用于逐次调阈值。

## 指标

- 框级：逐类 TP/FP/FN、Precision、Recall、F1，固定 IoU 阈值并记录。
- 烟支级：missed-NG（真 NG 判 OK）和 false-NG（真 OK 判 NG）必须单独报告。
- 效率：detector P50/P95/P99 与完整流水线时延分开记录。
- 所有报告必须绑定模型哈希、engine 哈希、类别目录、阈值配置、真值文件和输入清单哈希。

没有 reviewed ground truth 时，工具必须拒绝输出准确率结论。
