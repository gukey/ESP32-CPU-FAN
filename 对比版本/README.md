# OpenHardwareMonitorLib 发布版

目录中的 EXE 为单文件版本，已内置所需的 DLL 和配置，默认先连接 COM5，失败后自动切换其他串口。运行时不会在 EXE 旁生成配置、日志或驱动文件。

## OpenHardwareMonitorLib-1.0.9513

- OpenHardwareMonitorLib 1.0.9513（官方 NuGet 上最新可嵌入版本）
- HidSharp 2.6.4
- 优点：当前电脑实测 CPU/GPU 温度均可读取，项目仍在持续更新。
- 说明：OpenHardwareMonitor 应用的最新版本是 3.0.9705，但该发布只提供完整 EXE；最新独立 DLL 是 1.0.9513。

实机测试可读取 Intel Core i9-13900H 的 CPU 温度和 NVIDIA GPU 温度，并能在 COM5 写入超时后自动切换到 COM4、收到 ESP32 的 ACK。
