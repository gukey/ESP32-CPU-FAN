# OpenHardwareMonitorLib 上位机源码及依赖

本目录包含构建 Windows 单文件版所需的上位机源码、图标、PyInstaller 配置和全部 DLL 依赖。

构建方法：

```powershell
python -m pip install -r requirements-build.txt
pyinstaller 2.spec --noconfirm --clean
```

输出文件为 `dist/CPU_fan.exe`。发布时只需分发该 EXE；运行参数已内置，程序不会在 EXE 旁生成 `com.ini`、日志或硬件驱动文件。

硬件监控库版本为 OpenHardwareMonitorLib 1.0.9513，DLL 来自官方 NuGet 包。

`sketch_nov9a2/sketch_nov9a2.ino` 是与仓库主目录同步的 ESP32 固件源码，固定 Quiet 自动温控、断联停转，并包含看门狗与单次蓝牙恢复保护。使用 Arduino ESP32 核心及 Adafruit SSD1306 依赖编译，通过 USB 烧录后才会在设备上生效。
