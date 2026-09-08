# ESP32 CPU/GPU 温控风扇

一个由 Windows 上位机与 ESP32 固件组成的电脑散热风扇控制项目。上位机读取 CPU/GPU 温度，经蓝牙串口发送给 ESP32；ESP32 根据工作模式和温度调整 PWM 风扇转速，并在 OLED 上显示状态。

![实物效果](2025-11-09_204709.jpg)

## 功能

- 读取 CPU 与 NVIDIA/AMD/Intel GPU 温度
- 通过蓝牙串口向 ESP32 发送 `CPUxx.x`、`GPUxx.x` 数据
- 自动寻找、断线重连蓝牙串口，支持电脑休眠唤醒恢复
- Windows 托盘实时显示温度曲线和连接状态
- 首次运行自动添加当前用户的 Windows 开机启动项，移动 EXE 后自动更新路径并清理重复项
- 安静、正常、高速、手动、自定义五种风扇模式
- 128×64 SSD1306 OLED 状态显示
- 固定使用 Quiet 模式，根据 CPU/GPU 温度自动调速
- 无连接或长时间无数据时自动停止风扇并关闭屏幕

## 目录说明

| 文件 | 说明 |
| --- | --- |
| `sketch_nov9a2/sketch_nov9a2.ino` | ESP32 Arduino 固件 |
| `2.py` | Windows 上位机源码 |
| `CPU_fan.exe` | 已编译的 Windows 上位机 |
| `OpenHardwareMonitorLib.dll` | 默认硬件温度读取库，当前版本 1.0.9513 |
| `HidSharp.dll` | 硬件监控库依赖，当前版本 2.6.4 |
| `Only.ico` | 上位机图标 |
| `CPU_fan-可运行版/` | 整理后的 Windows 单文件可运行版本，DLL 已内置 |
| `OpenHardwareMonitorLib-源码及依赖/` | 上位机源码、打包配置、图标及全部调用 DLL |

## 快速使用

### 1. 烧录 ESP32

使用 Arduino IDE 打开 `sketch_nov9a2/sketch_nov9a2.ino`，安装以下库后编译烧录：

- Adafruit GFX Library
- Adafruit SSD1306
- ESP32 Arduino Core 2.x 或 3.x（代码已兼容两代 LEDC 接口，并使用 BluetoothSerial、EEPROM）

固件默认硬件连接：

- PWM 风扇控制：GPIO 5
- SSD1306 OLED：I²C 地址 `0x3C`，使用开发板默认 SDA/SCL
- 蓝牙设备名：`esp32散热器`

> 风扇电流通常超过 ESP32 GPIO 的驱动能力，请通过 MOSFET/驱动电路控制，并确保 ESP32 与风扇电源共地。

### 2. 配对蓝牙串口

在 Windows 蓝牙设置中与 `esp32散热器` 配对，随后在设备管理器中确认它对应的 COM 端口。

### 3. 运行上位机

下载 `CPU_fan-可运行版` 中的 `CPU_fan.exe` 即可运行。硬件监控 DLL 和运行参数均已打包到 EXE 内部，不需要其他文件。

```text
CPU_fan.exe
```

双击 `CPU_fan.exe` 后程序在系统托盘运行；右键托盘图标可查看温度、连接状态或退出。程序默认优先使用 `COM5`；COM5 无法打开、写入超时或连接后收不到 ESP32 的 `ACK` 时，会暂时跳过错误端口，并按设备特征、蓝牙端口和其他可用 COM 口的顺序继续寻找。

程序不会在 EXE 所在目录生成配置、日志或驱动文件。

首次运行会自动添加名为 `ESP32FanController` 的当前用户开机启动项，无需管理员权限。程序不会重复添加；如果移动了 EXE，再从新位置手动运行一次，启动项会自动覆盖为新路径，并清理其他名称下指向旧 `CPU_fan.exe` 的重复启动项。

## 从源码运行

建议使用 64 位 Python 3.11 或 3.12：

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install -r requirements.txt
python .\2.py
```

直接运行源码时，OpenHardwareMonitorLib 及其配套 DLL 需要与 `2.py` 位于同一目录。

## 编译 EXE

```powershell
python -m pip install -r requirements-build.txt
pyinstaller 2.spec --noconfirm --clean
```

编译结果位于 `dist/CPU_fan.exe`，发布时只需提供该 EXE。

## 通信协议

上位机按行发送 ASCII 文本：

```text
CPU52.3
GPU47.8
```

ESP32 收到有效数据后回复 `ACK`。默认串口速率为 115200。

风扇固定使用 Quiet 自动温控，参考原默认值设置连续曲线：55°C/16%、60°C/23%、65°C/31%、70°C/40%、75°C/75%、80°C/100%，锚点间线性插值。每 100ms 调整输出，升速每秒最多 10 个百分点、降速每秒最多 3 个百分点，目标与当前输出相差不足 2 个百分点时保持。80°C 及以上直接全速，断联或数据失效直接停转，不等待渐变。

上电时输出 0%，只有收到本次连接的合法温度数据后才启动；普通启动暂用 30% 并保持至少 1 秒后缓降，高温直接全速。30% 为试用启动值，无转速反馈，需根据实际风扇调整源码中的 `startupDutyPercent`。CPU/GPU 分别在 12 秒未更新后失效，使用仍有效的较高温度。蓝牙断开或两路都失效时立即停转，屏幕居中显示 3、2、1（各约 1 秒）后关闭；倒计时期间恢复合法温度数据则取消倒计时，恢复正常显示和温控。未连接时不会反复倒计时。

固件启用了 8 秒任务看门狗，主循环发生阻塞时会自动重启。已经建立有效通信但连续 60 秒收不到合法温度数据时，蓝牙栈只进行一次软恢复；电脑休眠或蓝牙正常断开时不会循环重启。软恢复失败会报告到 USB 串口，不进行定时整机重启。

接收采用固定长度缓冲区，每轮最多处理 128 字节，拒绝超长报文、非法数字和非有限值。OLED 初始化失败时继续温控，I²C 设置 50ms 超时。固件已移除全部按键、手动模式及自定义曲线代码；仅读取旧 EEPROM 中有效的 PWM 频率，默认 15kHz，不再写入配置。

固件行为测试见 `tests/README.md`。源码修改需要重新编译并通过 USB 烧录到 ESP32，更新 Windows EXE 不会更新设备固件。

## OLED 显示布局

正常运行时屏幕采用左右两栏大字布局：左侧显示 CPU 温度，右侧显示 GPU 温度；温度显示为便于远距离读取的四舍五入整数，温控计算仍使用接收到的原始小数值。底部依次显示连接状态 `CON`、当前风扇速度 `SPEED xx%` 和安静模式 `QUIET`，不再显示 PWM 频率。两位数温度使用最大字号，三位数温度自动缩小以避免越界；无效温度显示 `--`。关机倒计时仍显示居中的 `3`、`2`、`1`。

示例渲染图：

- [OLED 正常布局预览](docs/oled-preview.png)
- [OLED 高温/满速布局预览](docs/oled-preview-max.png)
- [OLED 温度缺失布局预览](docs/oled-preview-missing.png)
- [OLED 3 秒倒计时预览](docs/oled-preview-countdown.png)

## 注意事项

- 目前仅支持 Windows；温度数据默认由 OpenHardwareMonitorLib 获取。
- 某些硬件传感器可能需要管理员权限才能读取。
- Windows 或杀毒软件可能会对未签名的 PyInstaller 程序报警，请自行核对源码后运行。
- 高温控制属于辅助散热方案，请勿以本项目替代主板自身的过热保护。
