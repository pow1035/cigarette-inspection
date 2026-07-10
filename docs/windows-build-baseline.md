# Windows 构建基线

版本：2026-07-10，P1 静态基线

## 当前结论

新版工程已具备 VS2022/v143 的 x64 解决方案骨架，但本轮运行在 macOS，尚未执行 Windows 编译。本文记录的是工程文件事实和目标机检查方法，不是“已构建成功”声明。

## 工具链矩阵

| 项目 | Debug x64 | Release x64 | 证据/状态 |
| --- | --- | --- | --- |
| Visual Studio | VS 17 / v143 | VS 17 / v143 | CigVision.sln 和两个 vcxproj |
| Windows SDK | 10.0 | 10.0 | 工程文件显式配置 |
| Qt | 5.9.9_msvc2017_64 | 5.9.9_msvc2017_64 | 真实安装目录待 Windows 检查 |
| Halcon | 25.05 Progress | 22.11 Steady | 版本分裂，P1 不擅自统一 |
| 海康 MVS | include + win64 lib | include + win64 lib | Release 路径已补齐；SDK 版本待查 |
| DAQNavi | C:\Advantech\DAQNavi\Inc | 同左 | 版本待查；旧资料的 3.2.1.0 不能当新版证据 |
| process DLL | 与主程序同一 x64\Configuration 输出 | 同左 | 已增加 ProjectReference 和统一 OutDir |
| OpenCV/CUDA/TensorRT | 不属于 P1 主程序构建 | 不属于 P1 主程序构建 | P4 再接入 |

## P1 已处理的静态阻断

- Release 增加 MVS include、win64 library 和 MvCameraControl.lib。
- 主程序增加对 process.vcxproj 的 ProjectReference。
- process x64 输出改到解决方案统一目录，移除个人 E: 盘 include 路径。
- process 的 Debug/Release x64 均加入 `$(ProjectDir)..`，使 `<paramStructs.h>` 可从父目录解析。
- testWrite.cpp/.h 保留为历史文件，但不再进入新版工程构建。
- DAQNavi 使用 bdaqctrl.h，由工程 include 目录解析。
- 当前 config.ini 默认 `rejectEnabled=false`，且新版尚无 DO 输出实现；这不是 P6 所需的运行时硬件安全联锁。

## 仍未确定

- 目标 Windows 版本、VS2022 实际安装版本和 v143 组件。
- Qt 5.9.9 的实际安装位置及 Qt VS Tools 中的 QtInstall 映射。
- Debug/Release 为什么使用不同 Halcon 版本，目标机是否同时安装两者。
- MVS、DAQNavi 的准确版本和运行时 DLL。
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

脚本默认把日志和 `manifest.json` 写入仓库下的 `artifacts\p1-windows-时间戳`。该目录是本地证据，不应在未审查机器信息前提交。也可以分别手工构建：

~~~powershell
msbuild .\01_上位机_QT_新版_CigVision\源码\CigVision.sln /m /t:Rebuild /p:Configuration=Debug /p:Platform=x64
msbuild .\01_上位机_QT_新版_CigVision\源码\CigVision.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
~~~

需保存完整命令输出、退出码、`CigVision.exe`、`process.dll`、生成文件哈希和实际依赖版本。没有这些证据时，AC-01-02 保持未验证。

## 非 Windows 静态检查

~~~bash
./scripts/validate_p1_static.sh
~~~

该命令检查 XML、映射、帧元数据所有权、线程停止入口、安全配置和工程引用，只能证明静态不变量，不能替代 MSBuild、应用启动或硬件 QA。
