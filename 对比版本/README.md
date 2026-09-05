# 硬件监控库对比版本

两个目录中的 EXE 均为单文件版本，已内置各自所需的 DLL，默认先连接 COM5，失败后自动切换其他串口。

## LibreHardwareMonitor-0.9.3

- LibreHardwareMonitorLib 0.9.3
- HidSharp 2.6.4
- 优点：当前电脑实测 CPU/GPU 温度均可读取，依赖少，EXE 较小。

## OpenHardwareMonitorLib-1.0.9513

- OpenHardwareMonitorLib 1.0.9513（官方 NuGet 上最新可嵌入版本）
- HidSharp 2.6.4
- 优点：当前电脑实测 CPU/GPU 温度均可读取，项目仍在持续更新。
- 说明：OpenHardwareMonitor 应用的最新版本是 3.0.9705，但该发布只提供完整 EXE；最新独立 DLL 是 1.0.9513。

分别运行两个版本后，可查看同目录的 `CPU_fan.log`。测试另一个版本前，请先从托盘退出当前版本，避免两个程序争用同一个 COM 口。

## 实机测试结论

- 两版都能读取 Intel Core i9-13900H 的 CPU 温度和 NVIDIA GPU 温度。
- 两版都能在 COM5 写入超时后自动切换到 COM4，并收到 ESP32 的 ACK。
- LibreHardwareMonitorLib 0.9.6 在当前电脑上只能读取 GPU 温度，CPU 温度传感器值为空，因此不作为主版本。
- 主版本采用 OpenHardwareMonitorLib 1.0.9513：本机兼容性正常，而且比 0.9.3 方案更新；LibreHardwareMonitorLib 0.9.3 仅保留为备用对比版本。
