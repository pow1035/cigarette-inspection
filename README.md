# 烟支检测上位机项目

这是烟厂烟支外观检测上位机项目。产品主线是新版 Qt/C++ 工程，目标是由传统视觉检测逐步升级为可验证的深度学习检测闭环。

## 当前状态

当前阶段是 **P5：本地数据与算法效果闭环（待开始）**。

- P0-P3 已完成项目控制、Windows Release 构建基线、核心契约和离线检测闭环。
- P4 已完成 TensorRT 10 技术集成：新版 Qt/C++ 可在本机使用 GPU 批量推理并保存结构化结果和带框图。
- P4 没有人工 ground truth，只证明推理链和证据链可运行，不证明商业准确率；重叠框、大框和标签遮挡等效果问题由 P5 处理。
- P5-P8 全部在本地执行：数据与算法效果、本地实时流与模拟剔除、Qt 产品功能/UI 重构、稳定性/部署/交付预验收。

当前无法到现场。真实相机、卷烟机同步、DAQNavi 输入输出和真实剔除全部冻结，不属于 P5-P8 的完成声明；`rejectEnabled=false` 必须保持默认值。

## 本地路线

| 阶段 | 目标 | 当前状态 |
| --- | --- | --- |
| P5 | 建立标注真值、冻结测试集和量化效果基线，优化阈值、NMS、异常框，必要时受控训练 | 待开始 |
| P6 | 用文件/录制流模拟多相机、编号、节拍、积压、异常和模拟剔除 | 未开始 |
| P7 | 沿用现有 Qt UI 风格，精简旧功能，增强检测、复核、统计、配置和诊断 | 未开始 |
| P8 | 本地长时运行、性能优化、故障恢复、D 盘部署和商业交付预验收 | 未开始 |

## 先从这里看

1. `AGENTS.md`：项目协作规则和文档导航。
2. `docs/task-plan.md`：当前阶段和后续主线。
3. `docs/architecture.md`：新版、老版和 TensorRT 原型的职责边界。
4. `docs/known-issues.md`：尚未解决的风险和证据缺口。
5. `docs/windows-build-baseline.md`：Windows 构建环境与取证步骤。

## 主要目录

| 目录 | 用途 |
| --- | --- |
| `01_上位机_QT_新版_CigVision/源码` | 当前产品主线，Qt/C++ 上位机 |
| `02_上位机_QT_老版_传统算法` | 老版传统算法闭环，仅作业务参考 |
| `03_深度学习模型与TensorRT` | 已接入模型的来源、历史 engine、训练和转换资料；历史结论需由当前证据复核 |
| `04_测试数据与样例图片` | P5 数据审计、标注、评估和回归的候选资料 |
| `05_项目文档与汇报` | 传统算法、业务背景和项目汇报参考；不覆盖 `docs/` 当前计划 |
| `07_运行环境与依赖包` | 本机保留的大依赖包，不进入 Git |
| `docs` | 当前执行计划、验收、证据、风险与评审记录 |
| `scripts` | 静态检查和 Windows 构建取证脚本 |

## 同伴协作

```bash
git clone git@github.com:pow1035/cigarette-inspection.git
cd cigarette-inspection
git pull origin main
```

修改前先看 `docs/task-plan.md`，只做当前阶段的工作。验证命令按阶段选择；文档和 Git 基础门至少运行：

```bash
./scripts/validate_project_docs.sh
git diff --check
```

P1-P4 的阶段验证脚本仍可用于回归。Windows 构建基线命令为：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All
```

构建产物、IDE 缓存、依赖安装包、模型/engine、大测试数据和 `artifacts/` 证据目录不得提交。不要提交账号、许可证、设备序列号、客户资料、密钥或含机器身份的未脱敏日志。
