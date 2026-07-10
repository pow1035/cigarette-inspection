# 评审包

## P1 当前评审

状态：静态实现与自查完成，等待独立代码 reviewer。

### 范围

- 新版 CigVision 的相机映射、帧元数据所有权、有界队列和线程同步。
- Run/Stop、析构、MyCamera 和 readIOTask 生命周期。
- Debug/Release 的 MVS、DAQNavi、process DLL 工程关系。
- testWrite 编译排除和 rejectEnabled=false 安全配置。
- P1 静态/Windows 环境检查脚本及构建基线文档。

### Validation

~~~text
./scripts/validate_p1_static.sh
PASS P1 static project and source invariants

./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

xmllint --noout 三个 vcxproj
exit 0

bash -n 两个 shell 脚本
exit 0

git diff --check
exit 0

light_gate.py . --strict
no obvious doc/evidence warnings
~~~

### Test count

P1 当前没有可在 macOS 执行的 C++ 代码测试。上述结果是静态不变量和工程 XML 检查，不声称 tests 0 为代码测试通过。

### Git status

P1 修改新版源码/工程、config.ini、项目文档和 scripts；没有修改老版、TensorRT 原型或硬件资料。完整清单由 git status --short 和 reviewer 读取。

### 验收映射

见 evidence-matrix.md 的 AC-01-01 至 AC-01-07。

### 已知缺口

- 当前 macOS 没有 pwsh，Windows 环境检查脚本未实跑。
- 尚无 Debug/Release MSBuild、应用启动、重复启停、相机或 IO 运行证据。
- Debug/Release Halcon 版本仍分裂，等待目标机事实决定。

## P0 归档

状态：P0 提交门已通过；未执行 Git commit。

## 评审范围

- 新增 `AGENTS.md`、`docs/*.md` 和 `scripts/validate_project_docs.sh`。
- 检查这些文档是否准确描述现状、主线、依赖方向、阶段退出条件、风险、证据和提交门。
- P0 不评审业务源码改动，因为本阶段不应修改业务源码。

## 变更文件

当前未跟踪新增项：`AGENTS.md`、`docs/`、`scripts/`。P0 没有修改 `01`、`02`、`03` 或硬件业务源码。完整文件清单以 `git status --short` 和 reviewer 的仓库读取为准。

## Validation / 验证命令

```bash
./scripts/validate_project_docs.sh
python3 /Users/c/.codex/skills/codex-long-task-architecture/scripts/light_gate.py .
git diff --check
git status --short --branch
```

## Raw output / 原始输出

最新一次返修前输出：

```text
./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

git diff --check
exit 0, no output

python3 .../light_gate.py .
codex-long-task-architecture light gate: no obvious doc/evidence warnings

python3 .../light_gate.py . --strict
codex-long-task-architecture light gate: no obvious doc/evidence warnings

bash -n scripts/validate_project_docs.sh
exit 0, no output
```

独立 reviewer 指出原脚本的 `git diff --check` 不覆盖未跟踪文件；脚本已增强并完成返修后重跑：

```text
./scripts/validate_project_docs.sh
PASS project documentation structure and diff checks

python3 .../light_gate.py . --strict
codex-long-task-architecture light gate: no obvious doc/evidence warnings

bash -n scripts/validate_project_docs.sh
exit 0, no output

git diff --check
exit 0, no output

git status --short --untracked-files=all -- 01... 02... 03... 06...
exit 0, no output

rg -n '不存在的 test_yolo_trt_v2|CMake 源文件名错误|CMake 指向不存在' AGENTS.md docs scripts
exit 1, no matches
```

最终 reviewer 复核确认：错误 CMake 结论、验证脚本覆盖和事实证据问题均已 resolved。复核要求的最后一步是把结论和 AC/QA/证据状态落盘，本次更新已完成。

## Test count / 测试数量

P0 是文档引导阶段，代码测试不在范围内，未运行代码测试。这里不把 tests 0 声称为测试通过；P0 的有效检查是文档结构、Git diff、静态代码审计和独立文档评审。

## Git status

```text
## main...origin/main
?? AGENTS.md
?? docs/
?? scripts/
```

## 验收映射

见 `docs/evidence-matrix.md` 的 AC-00-01 至 AC-00-07。

## 已知缺口

- 本轮运行环境是 macOS，不能证明 Windows Qt 工程可构建或可运行。
- P0 不连接 GPU、相机或 IO，不声明运行 QA、UI QA 或硬件 QA 通过。
- 现场参数和模型验收基准仍需用户或项目方提供。
