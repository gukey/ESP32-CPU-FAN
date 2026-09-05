# OpenHardwareMonitorLib 上位机源码及依赖

本目录包含构建 Windows 单文件版所需的上位机源码、图标、PyInstaller 配置和全部 DLL 依赖。

构建方法：

```powershell
python -m pip install -r requirements-build.txt
pyinstaller 2.spec --noconfirm --clean
```

输出文件为 `dist/CPU_fan.exe`。发布时只需分发该 EXE；运行参数已内置，程序不会在 EXE 旁生成 `com.ini`、日志或硬件驱动文件。

硬件监控库版本为 OpenHardwareMonitorLib 1.0.9513，DLL 来自官方 NuGet 包。
