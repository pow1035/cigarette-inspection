# Windows 构建基线

版本：2026-07-11，P1 Windows Release 实测基线

## 当前结论

新版工程已在 Windows/VS2022 Developer PowerShell 完成 Release x64 实际构建和安全启动；Debug 因精确的 HALCON 25.05 Progress 缺失仍停在环境检查。本文只声明当前机器的 Release 基线，不推断客户现场或硬件功能已验证。

取证主机为 Microsoft Windows 11 家庭中文版 23H2、64 位，内核版本 `10.0.22631.0`（build 22631；注册表兼容字段仍返回 `Windows 10 Home China`）；Visual Studio Community 2022 `17.11.2`（`17.11.35222.181`）安装于 `E:\visual studio`。这些是当前机器事实，不推断客户现场环境。

## 工具链矩阵

| 项目 | Debug x64 | Release x64 | 证据/状态 |
| --- | --- | --- | --- |
| Visual Studio | VS Community 2022 17.11.2 / v143 | VS Community 2022 17.11.2 / v143 | `E:\visual studio`；manifest、vswhere 与 DevShell 输出 |
| Windows SDK | 10.0 | 10.0 | 工程文件显式配置；Release 已由 VS2022/v143 实际 Rebuild 验证，Debug 未进入 MSBuild |
| Qt | 5.9.9_msvc2017_64 | 5.9.9_msvc2017_64 | `D:\smokeqt\5.9.9\msvc2017_64`；qmake 5.9.9 |
| Halcon | 25.05 Progress，目标 `D:\MVTec\HALCON-25.05-Progress` | 22.11.4.0 Steady，已安装于 `D:\MVTec\HALCON-22.11-Steady` | 版本分裂，不擅自统一；Release header/lib/runtime、构建和 DLL 加载通过；25.05 当前官方目录未提供 |
| 海康 MVS | include + win64 lib | include + win64 lib | MVS 5.0.1 / SDK 4.8.0.3；开发目录 `D:\Hikrobot\MVS\MVS\Development` |
| DAQNavi | `$(CIGVISION_DAQNAVI_ROOT)\Inc` | 同左 | 实装根 `D:\advantexh\DAQNavi`；`biodaq.dll` 4.1.22.0 |
| process DLL | 与主程序同一 x64\Configuration 输出 | 同左 | 已增加 ProjectReference 和统一 OutDir |
| OpenCV/CUDA/TensorRT | 不属于 P1 主程序构建 | 不属于 P1 主程序构建 | P4 再接入 |

## P1 已处理的静态阻断

- Release 增加 MVS include、win64 library 和 MvCameraControl.lib。
- 主程序增加对 process.vcxproj 的 ProjectReference。
- process x64 输出改到解决方案统一目录，移除个人 E: 盘 include 路径。
- process 的 Debug/Release x64 均加入 `$(ProjectDir)..`，使 `<paramStructs.h>` 可从父目录解析。
- testWrite.cpp/.h 保留为历史文件，但不再进入新版工程构建。
- DAQNavi 使用 bdaqctrl.h，由 `CIGVISION_DAQNAVI_ROOT` 解析；本机用户变量指向 `D:\advantexh\DAQNavi`。
- 当前 config.ini 默认 `rejectEnabled=false`，且新版尚无 DO 输出实现；这不是 P6 所需的运行时硬件安全联锁。

## 仍未确定

- 目标 Windows、VS2022 和 v143 Release 组合已由 `artifacts/p1-windows-20260711-115033` 的 manifest/MSBuild 证明。
- Debug/Release 为什么使用不同 Halcon 版本，目标机是否同时安装两者。
- MVS 相机实际连接、DAQNavi PCIE-1730 设备识别与 IO 读取行为。
- GPU、CUDA、TensorRT 和 OpenCV 在 P4 的最终组合。

## Windows 检查

在 VS 2022 Developer PowerShell 中，从仓库根目录运行：

~~~powershell
powershell -ExecutionPolicy Bypass -File .\scripts\check_windows_build_env.ps1 -Configuration Debug
powershell -ExecutionPolicy Bypass -File .\scripts\check_windows_build_env.ps1 -Configuration Release
~~~

推荐使用取证脚本一次完成环境检查、Debug/Release 重建、输出核对和 SHA-256 清单。构建动作本身不会启动上位机或写 IO：

~~~powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_windows_p1_build.ps1 -Configuration All
~~~

脚本默认把日志和 `manifest.json` 写入仓库下的 `artifacts\p1-windows-时间戳`。该目录只作本机审计证据，禁止提交；外发前必须生成脱敏副本。也可以分别手工构建：

~~~powershell
msbuild .\01_上位机_QT_新版_CigVision\源码\CigVision.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild .\01_上位机_QT_新版_CigVision\源码\CigVision.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
~~~

需保存完整命令输出、退出码、`CigVision.exe`、`process.dll`、生成文件哈希和实际依赖版本。当前 Release 已满足这些证据并成功显示主界面；Debug 仍因 25.05 缺失阻断。HALCON 许可证、相机和实际 IO 不在本次成功声明内。

## 非 Windows 静态检查

~~~bash
./scripts/validate_p1_static.sh
~~~

该命令检查 XML、映射、帧元数据所有权、线程停止入口、安全配置和工程引用，只能证明静态不变量，不能替代 MSBuild、应用启动或硬件 QA。
