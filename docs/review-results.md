# 评审结果

## P3

状态：首轮 reviewer findings 已全部返修；同一 reviewer/QA 最终 PASS，P3 gate 关闭。

- 实现前只读 explorer：`019f4f73-404a-79d0-b267-a0d93a5d63b6`（UI/线程边界）与 `019f4f73-5447-7e80-a70d-62ebaf522a30`（样本/故障场景）。二者未修改文件，不计为最终独立评审。
- 实现自查修正：补充空输入测试；修复 `QVariantMap` typedef 编译错误；清除 OK 帧时旧 NG 预览；P3 脚本改为动态定位中文目录并按子进程退出码判定。
- 返修证据：`artifacts/p3-offline-20260711-132348`，顶层 passed/exit 0；两配置 7/7；主工程 Release Rebuild 0；8 图完整追踪/统计/哈希一致；非法 manifest exit 2；Computer Use 单图 UI 流程通过。
- 安全边界：不连接相机、不执行 DAQNavi 读写、不生成或执行 RejectCommand；`rejectEnabled=false`。fixture detector 不计为生产算法。
- 独立 reviewer `019f4f8b-ce3e-7c31-b77a-78e6c04ec59f` 首轮 BLOCKED：停止竞态、worker UAF、协作者异常、不受控容量/停止排空、追踪不足，以及 UI/manifest/文档缺口。两轮返修复核确认全部 resolved，最终 reviewer gate PASS。
- 独立 QA `019f4f8b-f75a-7a53-8de6-25206dfa2526` 最终复算 37 项 manifest 文件记录全部一致，确认两配置 7/7、非法 manifest exit 2、完整追踪、UI QA 和 `rejectEnabled=false`，最终 QA gate PASS。
- P3 最终 gate PASS，阶段关闭并切换到 P4 待开始；不声明 TensorRT、准确率、相机、DAQNavi、HALCON 许可算子或真实剔除完成。

## P2

状态：实现自查、Debug/Release 契约测试、CigVision Release 回归构建和独立 reviewer/QA 均通过，P2 已关闭。

- 候选范围：`core/InspectionContracts.h`、`InspectionInterfaces.h`、`BoundedQueue.h`、独立契约测试工程和 Windows 取证脚本。
- 正式证据：`artifacts/p2-contracts-20260711-122211` 与 `artifacts/p2-main-regression-20260711-120926`。
- 明确排除：现有 picStruct/相机回调迁移、队列消费者、离线闭环、TensorRT、真实 IO 和剔除。
- 实现前只读 explorer：`019f4f56-4819-7a20-b9b0-16820addf9cf`（边界/迁移风险）与 `019f4f56-5c24-7712-b9fb-3c116d3ea189`（无硬件测试方案）。二者均未修改文件，不计为最终独立评审。
- 首轮 reviewer `019f4f64-2c4c-7650-b78b-ca60cc35643c` 判定 BLOCKED：安全枚举缺少闭集校验；队列拒绝会提前消费 move 所有权；并发/关闭测试不足；artifact 未绑定脚本。现已显式拒绝域外枚举、用 const/rvalue 重载在接受后才复制/移动、增加三组队列测试，并将确定命令及脚本 SHA-256 写入新 manifest。
- 首轮 QA `019f4f64-4069-7011-9a9a-0e5cdb865d6b` 独立复跑两配置 4/4 并检查 PE 依赖后给出 PASS；其非阻断命令/脚本哈希 finding 已随 reviewer 返修一起解决。新证据两配置 7/7，等待同一代理返修复核。
- 最终 reviewer 返修复核：四项 finding 全部 resolved；Debug/Release artifact 复跑并各连续执行 20 次 7/7，无新增 finding，reviewer gate PASS。
- 最终 QA 返修复核：40/40 artifact 文件及六个源码/脚本哈希一致，命令非空；两配置各连续 20 次 7/7，PE 依赖无 Qt/Halcon/MVS/DAQ/TensorRT，QA gate PASS。
- P2 最终 gate PASS。阶段关闭并切换到 P3 待开始；不声明 picStruct/相机迁移、离线闭环、TensorRT 或真实剔除完成。

## P1

状态：P1 独立 reviewer、独立 QA 与提交门均已通过，P1 可关闭。实现代理自查和静态门不计为独立复核。

### 2026-07-11 Release 构建与启动候选

- HALCON 22.11.4.0 完整包保存于 D 盘，大小 `6516267818` 字节，SHA-512 与 MVTec 官方值一致；最小组件安装到 `D:\MVTec\HALCON-22.11-Steady`。安装器明确显示本机无许可证。
- 首次 Release MSBuild 暴露 `dllmain.cpp` 同时引入父目录和本地同内容 `paramStructs.h`，产生结构体重定义。两文件 SHA-256 相同；删除 `dllmain.cpp` 不使用的冗余 include 后，Release 构建通过。
- 当前正式证据 `artifacts/p1-windows-20260711-115033`：All 顶层 `partial`/exit 1；Debug 因 25.05 缺失 environment-check exit 1；Release environment/MSBuild exit 0，manifest 含 `CigVision.exe/process.dll` 哈希。
- CigVision 在 `rejectEnabled=false` 下启动并以 `startup-window.png` 记录主界面，没有点击任何应用控件。进程持续响应并加载 Qt 5.9.9、HALCON 22.11.4.0、MVS 4.8.0.3、DAQNavi 4.1.22.0；相机采集、实际 IO、许可算子和剔除均不声明通过。
- 当前结论：经同一 reviewer/QA 返修复核，P1 技术退出条件和提交门均通过。

### 2026-07-11 当前 Release 候选独立复核

- reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` 确认至少一个 Windows x64 配置的技术退出条件成立，并发现两项证据完整性问题：旧 manifest 仅记录 HEAD、不能绑定 dirty 工作树；脚本未捕获异常时可能错误写出 `passed`。现已在 manifest 记录 Git 状态及 `source-snapshot.diff` SHA-256，增加 `unhandled-exception` failure，并以 `115033` 重新执行 All/Release Rebuild。
- 同一 reviewer 发现 Windows 基线仍有“静态基线/工具集待 MSBuild”旧表述，已同步为 Release 实测基线。返修复核确认三项 reviewer finding 均 resolved，reviewer gate PASS。
- QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` 复核 `artifacts/p1-windows-20260711-115033`：Release environment-check 和 Rebuild exit 0，产物 SHA-256 与 manifest 一致；Debug 仅因 HALCON 25.05 Progress 缺失而阻断，All 保持 `partial`/exit 1。
- QA 确认启动证据仅支持 `CigVision.exe` 主界面可见、进程响应并加载 Qt/HALCON/MVS/DAQNavi 运行库；`process.dll` 已构建、输出和哈希，但启动模块列表未显示其已加载。没有执行 Run、相机采集、实际 IO 或剔除。
- QA 首轮发现两项文档门禁：旧 artifact 的 blocked 结论未明确归档，以及证据矩阵误把 `process.dll` 表述为已随程序加载。两项修正后，同一 QA 返修复核为 No findings，QA gate PASS。
- 最终门禁：reviewer `019f4f47-4af3-7960-bfa1-8ff7bfad0bc0` PASS；QA `019f4f47-5f24-7fa0-9632-960d0d0d5b36` PASS。P1 关闭；HALCON 许可证、Debug 25.05、MVS 相机采集、DAQNavi 实际 IO、重复启停和剔除仍按已知问题留待后续安全阶段。

### 2026-07-11 历史 `235900` 证据独立复核

- reviewer `019f4eec-cbd9-7263-bd2c-826506e81f10`：针对历史 `artifacts/p1-windows-20260710-235900`，两个 vcxproj 的属性传播、x64 配置和 XML 无新增确定性问题；发现当时的环境门主要检查文件存在，不能把已记录的实际版本表述为兼容版本门通过。当时 AC-01-01、证据矩阵和任务计划降级为进行中；该结论已被 `115033` 的 Release 实际 Rebuild 和启动证据更新，但仍不代表硬件兼容或功能通过。
- reviewer 同时确认本地 artifact 含主机名、用户名绝对路径和安装产品清单。未发现密码、令牌或密钥，且 `artifacts/` 已忽略；该目录只作本机审计证据，禁止提交，外发前必须脱敏。
- reviewer 的低严重度系统名称问题已修正：实际系统为 Windows 11 家庭中文版 23H2/build 22631；`Windows 10 Home China` 仅为注册表兼容字段值。
- QA `019f4eec-dfe0-74f0-8b1f-fcc6e0beae55`：针对历史 `235900` 首轮 no findings；返修复核发现基线仍有“审查后可能提交 artifact”的条件式措辞，已统一为只作本机审计、绝对禁止提交、外发前脱敏，并经同一 QA 确认 finding 关闭。该历史目录顶层 exit 1、两配置 environment-check exit 1、`Builds` 为空、无 MSBuild/输出/启动证据，因此当时 QA gate 为 FAIL/BLOCKED；此结论已被后续 `115033` 证据更新。

### 2026-07-10 Windows 验证尝试

当前正式取证：`artifacts/p1-windows-20260710-235900`。该目录由 VS2022 Developer PowerShell 17.11.2 的 All 运行生成，包含 `full-terminal.log`、两份环境日志和 `manifest.json`；顶层 exit 1，`OverallResult=failed-before-msbuild`。Qt 5.9.9、MVS 5.0.1 / SDK 4.8.0.3、DAQNavi header 与 x64 runtime 已检出；Debug/Release 均因指定 HALCON 完整文件集缺失而 environment-check exit 1。未进入 MSBuild、未生成输出、未启动程序，不能据此关闭 P1。

依赖获取补充事实：MVTec Software Manager 1.7.2 的下载包 SHA-512 已与官网校验值一致，登录后可识别 HALCON 22.11.4.0 Steady Windows x64 完整包；多次自动下载未形成完整文件，安装未开始。当前官方 HALCON 下载目录不列出 25.05 Progress，因此 Debug 依赖继续记录为外部阻断，不用其他版本替代。

本轮独立 reviewer `019f4c33-ecab-7591-976f-52c7cc1fd243` 与 QA `019f4c34-157d-77c2-b504-f98ef223e776` 均判定 P1 不能关闭，并发现 MVS 属性传播、All 提前终止、HALCON 预检过弱、DAQ runtime 证据缺失及评审材料冲突。集中返修后，同一 reviewer/QA 确认上述高风险问题已解决；随后补充混合结果 `partial` 语义和 MVS/DAQNavi runtime 环境门并生成 `213607`。最终门禁仍因 HALCON、MSBuild 与启动证据缺失而阻断。

以下为首次 Windows 取证的历史结果：脚本首次失败于 Windows PowerShell 5.1 对中文路径字面量的编码误读；修复后历史证据为 `artifacts/p1-windows-20260710-160725`。

历史 `160725` 结果：Debug 环境检查 exit 1，Qt、目标 Halcon、MVS 和 DAQNavi 均未检出；未进入 MSBuild、未生成 `CigVision.exe/process.dll`、未启动程序。该结果已被后续依赖补齐证据更新，但仍保留为审计轨迹。

本轮独立 reviewer：`019f4b0c-ee7e-7dd2-8fb9-b6cb96375da9`。结论：P1 不能关闭，AC-01-02 仍失败/阻断；脚本改动范围符合 P1，未触碰业务源码、TensorRT 或真实剔除逻辑；`artifacts/` 被忽略，但 artifact 内含机器名、绝对路径和安装产品清单，外发需脱敏。发现一个中等问题：`review-packet.md` 残留旧验证叙述，已改为当前 Windows 失败证据与此前静态证据分段。

本轮独立 QA：`019f4b0d-224e-7043-b47c-e0d3d168523d`。结论：证据只支持“Windows 取证脚本已在 VS2022 Developer PowerShell 中执行，并在 Debug 环境检查阶段失败”；不能支持“构建成功、Release 已验证、程序启动成功、输出文件生成成功”。QA 要求补充顶层命令退出状态字段并同步 `windows-build-baseline.md`，本轮已处理。

- reviewer：独立代理 `019f49ea-1bf9-7f60-971e-fd69f6b0e57c`。
- QA：独立代理 `019f49ea-1ba5-7693-a15c-8036c6003ae7`。

| 严重度 | 首轮发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | process 工程缺少父目录 include，`<paramStructs.h>` 无法解析 | 已给 Debug/Release x64 增加 `$(ProjectDir)..`；静态复核通过，仍待 MSBuild |
| 阻断 | Stop/析构与活动相机回调存在竞态 | 已增加活动回调计数、进程期守卫和等待屏障；静态复核通过，仍待 Windows/MVS QA |
| 高 | 多个 atomic 编号字段不能形成同一触发周期快照 | 已改为互斥保护的单次结构体提交/读取；静态复核通过 |
| 高 | IO 初始化失败仍被上层报告成功，用户 Stop 不结束读取任务 | 已改同步 initialize、Run 时 prepareStart、Stop 时 requestStop + 等待；静态复核通过 |
| 高 | 组件 1 可产生 100-255，组件位置无效时静默按 0 处理 | 原始编号大于 99 明确丢弃；派生编号使用 0-99 环绕；静态复核通过 |
| 高 | Stop 和启动回滚忽略 SDK 返回值 | 已记录返回码；Close 返回 Stop/Close/Destroy 首个失败；仍待 Windows QA |
| 高 | 临时灰度缓冲的 Halcon 所有权缺少证明 | 增加 `CopyImage` 形成队列对象副本并捕获回调异常；仍待目标 Halcon 验证 |
| 高 | Stop 失败后 shutdown 仍可能释放裸回调 `pUser`，且运行中读取可变工位参数 | 回调改用进程期守卫并在 Stop/shutdown detach；Run 前校验并冻结 10-30/20-40 工位参数；静态复核通过，晚到帧仍待 Windows QA |
| 中 | 静态脚本遗漏 process include、回调屏障、快照及构建基线文档 | 已扩充两个验证脚本，返修后运行通过 |
| 后续阶段 | 队列无消费者、回调仍做整帧转换、真实剔除门不存在 | 分别由 KI-015、KI-023、KI-024 跟踪到 P2/P3/P5/P6，不作为 P1 已实现能力 |

首轮 review 明确指出：没有 Windows/MSBuild、Qt、Halcon、MVS 或 DAQNavi 运行证据。即使返修复核通过，AC-01-02 和 P1 阶段仍必须保持未验证，直到目标机完成构建与启动。

## P1 QA 返修复核

独立 QA 重新读取最终工作树并实跑 P1 静态门、文档门、`git diff --check`、`xmllint` 和 skill 严格门，全部 exit 0。未发现确定的成员引用、信号槽签名、C++ 语法或必现死锁问题；确认 Stop 失败锁定再次 Run、IO 故障 queued signal 安全停止、完整编号快照和 process 双配置 include 路径均存在。

返修期间又发现 Stop 失败 shutdown 的裸回调上下文风险和运行中工位参数竞争。第二轮返修把 SDK `pUser` 改为进程期存活的回调守卫，Stop/shutdown 会先 detach 窗口并等待活动回调；IO 任务在 Run 前校验范围并冻结工位参数。MVS 是否在 Stop 后产生会污染下一次 Run 的晚到帧仍由 KI-026 追踪，不能以静态通过替代运行证据。

## P0

状态：P0 独立评审与返修复核已通过。

源码审计结论已写入 `docs/code-audit.md`，覆盖新版 Qt、老版闭环和 TensorRT 原型。审计均为静态只读检查，不作为 Windows、GPU、相机或 IO 的运行通过证据。

## 独立评审记录

- reviewer：独立代理 `019f49b6-62f7-7320-8f43-228c21bfca93`。
- documentation maintenance：独立代理 `019f49b6-62b5-7511-9b1c-93f055de83bc`。
- 范围：P0 入口、需求、架构、计划、验收、证据、代码审计、验证脚本和提交门。

| 严重度 | 发现 | 处理状态 |
| --- | --- | --- |
| 阻断 | 错误声称 `test_yolo_trt_v2.cpp` 不存在 | 已纠正、加入反证说明并经 reviewer 复核通过 |
| 阻断 | 评审包和证据矩阵未记录本次评审/最新门禁 | 已同步并经 reviewer 复核通过 |
| 高 | `git diff --check` 不检查未跟踪 P0 文件 | 验证脚本已增加未跟踪文件检查和业务目录变更检查，返修后运行通过 |
| 高 | 历史 TensorRT 文档的完成/性能声明缺少当前证据 | 已在 `code-audit.md` 明确历史资料边界，KI-022 追踪后续整理 |
| 中 | AC 状态和 QA/证据记录不同步 | 已同步为最终通过状态 |
| 中 | `AGENTS.md` 对深层文档导航不完整 | 已补充紧凑导航行 |
| 中 | 大范围历史文档重复和命名漂移 | P0 不做大清理；P4 指定唯一入口，P8 前强制清理 |

## 最终复核

原 reviewer 对四项返修逐项复核：错误 CMake 结论 resolved；最新评审与命令输出在写入本结论后 resolved；验证脚本覆盖 resolved；AC/QA/证据状态在本次同步后 resolved。documentation maintenance 同时确认过期 warning、AC-00-07、AGENTS 导航、历史 TensorRT 声明边界和状态一致性均已解决，历史目录大清理留到 P4/P8。

P0 已通过独立评审和提交门，可以关闭；Windows、GPU、相机、IO 和算法效果不属于 P0 通过声明。

当前不把实现代理自查计为独立评审。

## P4 评审状态

- 两名独立只读 explorer 已完成前置审计：确认 `[1,3,992,992] -> [1,300,6]`、9 类、旧 API 风险、无 ground truth 和 116/113 样本事实。
- 实现 worker 自查只计实现自查，不计独立 reviewer；其初版默认输出名错误已由编排代理依据 ONNX 本体改为 `output0`。
- 编排代理运行/视觉自查发现自写 resize 与 ONNX 差异，已切换 OpenCV 标准预处理并用 FP16/FP32/ONNX 对照复核。
- P4 独立 reviewer 与独立 QA 均已完成返修复核，最终 gate PASS；P4 技术集成关闭并切换到 P5 本地数据与算法效果闭环待开始。

### P4 首轮独立门禁

- reviewer `019f4ff6-8554-7700-992b-ab2773108818`：gate FAIL。阻断为残缺批处理 CLI 会回落普通硬件 UI；高优先级为配置在校验前可能参与巨量分配、正式证据未绑定未跟踪 P4 源码；中低项为 detector latency 命名错误、评审包顶层陈旧、`noexcept` logger 可抛。
- QA `019f4ff6-9972-7b73-83a5-a288e6714ab5`：首轮运行 gate PASS，独立复跑 116/116、361 个证据文件哈希一致、engine 负路径 exit 4、P2/P3 回归通过；视觉独立确认 frame 1/3/6/12 的重叠框、大框和文字遮挡。该 PASS 不声明准确率。
- 返修：批处理参数只要残缺/重复/冲突即 exit 2；JSON 尺寸和禁用类别先做精确整数/范围校验；detector 构造在任何 host buffer 分配前验证 config；P4 manifest 新增 10 个源码/工程/脚本 SHA-256；时延更名 detector latency；logger 内部捕获异常；评审包顶层状态已更新。
- 返修正式证据：`artifacts/p4-tensorrt-20260711-150510` 顶层 PASS，engine/尺寸 exit 4、残缺/冲突 CLI exit 2，源码哈希复算 0 差异。

### P4 最终复核

- 原 reviewer 逐项确认首轮 6 项均 resolved：CLI 不再回落硬件 UI；配置在分配前严格校验；10 个 P4 源文件哈希绑定；detector latency 命名准确；评审包顶层一致；logger `noexcept` 兜底。技术 gate PASS。
- 原 QA 在 `artifacts/p4-qa-independent-postfix-20260711` 独立复跑：外层/内层 exit 0；invalid engine/shape exit 4；missing/conflicting CLI exit 2；116/116、246 框、错误 0；364 个证据文件和 10 个 source hash 全匹配；最终 gate PASS。
- QA 再次视觉确认 frame 1/3/6/12 的重叠框、大框和标签遮挡。无 ground truth，因此这类观察转入 KI-030/P5 的本地标注、量化和优化闭环，不转换成未经证实的误检率或漏检率。

P4 关闭后的路线调整属于用户产品规划决定，不改变 P4 reviewer/QA 结论，也不被计为新的实现通过声明。P5-P8 的代码、效果、UI 和稳定性必须在各自阶段重新接受独立评审与 QA。
- P4 最终结论：技术集成与可复现效果审查通过；不声明准确率，不证明相机、DAQNavi、HALCON 许可、在线节拍或真实剔除。

## 2026-07-11 P5 路线 documentation maintenance

- documentation maintenance agent 已检查 README、AGENTS 和完整记录系统，确认此前只有主计划部分完成路线切换，README、可观测性和后续 QA 场景仍存在旧 P1/P5/P6 表述。
- 已统一为 P5 数据与算法效果、P6 本地实时流与模拟剔除、P7 沿用现有风格的 Qt 产品功能/UI 重构、P8 本地稳定性/部署/交付预验收；历史 P0-P4 条目保留并标注历史边界。
- `CURRENT_PHASE:P5` 唯一；P5 仍为未开始，未虚构标注、准确率、UI、实时流、稳定性或硬件证据。
- `light_gate.py`、全工作树 `git diff --check`、旧路线扫描和 PowerShell 等价文档结构检查通过。`validate_project_docs.sh` 可由 Git Bash 启动，但其未跟踪文件检查与当前 Windows CRLF 工作树不兼容，明确记录为 degraded validator compatibility。
- 本轮只维护文档，不是 P5 实现评审，也不改变 P4 reviewer/QA 的既有结论。未提交、未推送。
