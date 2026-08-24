# 本机本地门禁运行环境(WSL2)

日期:2026-08-24

## 结论

本机的检测闭环 SDK-free 本地门禁已跑通:`./scripts/run_all_local_gates.sh --core` 与 `--full` 均 PASS。整套 SDK-free 检测闭环(有界队列、统一检测接口、离线编排、模拟剔除、产品状态、连续运行)的核心逻辑在本机可复现验证。

## 为什么用 WSL2 而不是原生 Windows

Windows 原生工具链不满足本地门禁要求:

| 缺口 | 说明 |
| --- | --- |
| `python3` | 指向微软商店空壳,不能真正执行 |
| `g++` | MinGW.org 6.3.0,无 ASan/UBSan(`-fsanitize` 找不到 `libasan`) |
| `xmllint` | 缺失(门禁用其校验 `.vcxproj` XML) |
| 行尾 | `core.autocrlf=true` 使 CRLF 污染 `git diff --check` 与 bash 脚本 |

项目本地门禁本就是在 Linux 上验证的(历史证据为 Mac + Colima/Linux),因此复用本机已有的 WSL2 Ubuntu 24.04 是最稳路径。

## 环境搭建(已在本机完成)

```bash
# WSL Ubuntu 24.04 内
apt-get install -y --no-install-recommends \
  build-essential ripgrep libxml2-utils python3-pip python3-venv ca-certificates

# Python 依赖(精确匹配 requirements-p5.txt)
python3 -m venv /opt/cigvision-venv
/opt/cigvision-venv/bin/pip install --no-cache-dir Pillow==11.3.0 PyYAML==6.0.3

# 干净克隆(LF 行尾,避免 Windows CRLF)
git clone -c core.autocrlf=false /mnt/d/zhiyan_8 /root/cigarette-inspection
```

## 复现命令

```bash
cd /root/cigarette-inspection
export PATH=/opt/cigvision-venv/bin:$PATH
export LANG=C.UTF-8 LC_ALL=C.UTF-8
./scripts/run_all_local_gates.sh --core   # 快速
./scripts/run_all_local_gates.sh --full   # 默认,含 ASan/UBSan、20×重复
```

## 验证结果(2026-08-24)

| 套件 | 结果 |
| --- | --- |
| tests/p5 | 100/100 |
| tests/p6 | 17/17 |
| tests/p8 | 81/81 |
| CigVision.Contracts | 7/7 |
| CigVision.Offline | 7/7 |
| CigVision.Simulation | 15/15 |
| CigVision.ProductState | 8/8 |
| C++17 / C++14 严格编译、ASan/UBSan、20×重复、standalone header、`git diff --check` | PASS |

## 边界

- 本结果只验证 SDK-free 检测闭环核心逻辑与 Python 工具,不涉及 Qt 产品、TensorRT/GPU、相机/IO、真实数据。
- Qt 产品(`CigVision.exe` + TensorRT)仍需目标机工具链(Qt 5.9.9 / VS2022 / HALCON 22.11 / TensorRT 10.15);本机 RTX 4050 与项目基线 RTX 4060 不一致,且未装上述工具链。
- 相关开放项:`known-issues.md` 的 KI-039、KI-040、KI-041、KI-043。

## 已知 flaky

- P8 连续运行测试 `test_healthy_continuous_runtime_passes_and_reverifies` 在首次冷启动/高负载下偶发失败一次(进程组 CPU 记账时序敏感),复跑即过,后续稳定;非阻断。
