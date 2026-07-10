# 烟支检测上位机项目

这是烟厂烟支外观检测上位机项目。产品主线是新版 Qt/C++ 工程，目标是由传统视觉检测逐步升级为可验证的深度学习检测闭环。

## 当前状态

当前处于 **P1：新版工程构建基线与硬化**。

- 已完成：项目文档与阶段管理、相机/IO 生命周期静态硬化、工程依赖关系整理、P1 静态验证和独立 QA 返修复核。
- 尚未完成：目标 Windows 的 Debug/Release 实际构建、程序启动、相机/MVS/DAQNavi/Halcon 运行验证。
- 尚未开始：P2 数据契约与检测器接口、P3 离线检测闭环、P4 TensorRT 正式接入、P5/P6 在线和真实剔除联调。

不要把当前静态通过理解为现场可运行或可剔除。真实 IO 剔除默认关闭。

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
| `03_深度学习模型与TensorRT` | YOLO/TensorRT 原型和历史资料，P4 再正式接入 |
| `04_测试数据与样例图片` | 样例和测试资料 |
| `05_项目文档与汇报` | 原始项目文档与汇报材料 |
| `07_运行环境与依赖包` | 本机保留的大依赖包，不进入 Git |
| `docs` | 当前执行计划、验收、证据、风险与评审记录 |
| `scripts` | 静态检查和 Windows 构建取证脚本 |

## 同伴协作

```bash
git clone git@github.com:pow1035/cigarette-inspection.git
cd cigarette-inspection
git pull origin main
```

修改前先看 `docs/task-plan.md`，只做当前阶段的工作。提交前至少运行：

```bash
./scripts/validate_project_docs.sh
./scripts/validate_p1_static.sh
git diff --check
```

Windows 目标机需要在 VS 2022 Developer PowerShell 中运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All
```

构建产物、IDE 缓存、依赖安装包、完整测试压缩包和现场证据目录均已在 `.gitignore` 中排除。请不要提交现场账号、许可证、设备序列号、客户资料或密钥。
