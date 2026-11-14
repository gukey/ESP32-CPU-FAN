# 固件回归测试

新增心跳测试覆盖随机数原样应答、非法报文拒绝、无温度时心跳不启动风扇、心跳不延长温度有效期、持续心跳避免误触发蓝牙恢复，以及 millis 回绕。

## 心跳版本实机烧录记录

2026-11-14，Arduino-ESP32 3.3.7、ESP32 Dev Module、Adafruit SSD1306 2.5.17、GFX 1.13.0 编译通过。程序 1,102,443 字节（84%），静态内存 43,636 字节（13%）。2.x/3.x 模拟分支均通过。

通过 COM6 向 ESP32-D0WD-V3、4MB Flash 写入，波特率 460800，各段哈希校验通过。复位后 USB 调试输出读到 `ESP32 FAN LINK 1.1; data=Bluetooth; heartbeat=PING/PONG; curve=Quiet`。按用户要求跳过备份后直接烧录。

烧录时风扇 12V 断开、ESP32 由 USB 连接。此记录验证了编译、写入和启动，尚不代表蓝牙 COM5 实时联调、实际风扇转速或满载长期稳定性已通过。

`firmware_test.cpp` 直接包含实际 `.ino`，以模拟时间、蓝牙队列、PWM 和屏幕执行状态测试。硬件替身不能验证真实蓝牙栈、看门狗复位、电气输出或长时间运行稳定性。

在仓库根目录使用支持 C++17 的本机编译器运行：

```powershell
python -m pip install ziglang==0.14.1
python -m ziglang c++ -std=c++17 -I tests/firmware_stubs tests/firmware_test.cpp -o "$env:TEMP/esp32-fan-tests.exe"
& "$env:TEMP/esp32-fan-tests.exe"
```

覆盖：上电停转、无屏幕启动、合法/非法/分包/超长报文、断联与快速重连、睡眠期间不重试、温度过期、计时回绕、单次蓝牙恢复及收到新数据重新放行。

实际固件编译使用 `arduino-cli compile --fqbn esp32:esp32:esp32 sketch_nov9a2`，需要安装 ESP32 核心、Adafruit SSD1306 及其依赖。无论本机测试是否通过，都需要烧录后验证电脑休眠/唤醒和持续运行。

## 基础固件验证记录（平滑调速改动之前）

- 真实核心编译通过：Arduino-ESP32 3.3.7，ESP32 Dev Module 默认分区；Adafruit SSD1306 2.5.17、GFX 1.12.6、BusIO 1.17.4。
- 程序 1,101,855 字节（84%），静态内存 43,612 字节（13%）。
- 本机模拟测试通过，包括接收期间时间前进、计数回绕、超时残包及传感器分别失效；2.x/3.x 两套条件编译分支均通过模拟测试。
- 尚未进行 Arduino-ESP32 2.x 真实工具链编译、实机烧录或长时间运行测试。模拟看门狗只验证调用流程，不能验证实际复位。

## 平滑调速源码验证

关屏倒计时回归测试覆盖：3、2、1 顺序、倒计时期间停转、到期关屏、休眠不重复倒计时、数据超时触发、新数据取消及字体恢复、毫秒计数回绕。此次同样只修改源码，不编译发布固件或烧录。

平滑调速版本的 2.x/3.x 条件分支均通过本机模拟测试，新增默认曲线连续性、每秒升 10/降 3 个百分点、高温直达 100%、断联跳过渐变直接停转测试。按用户要求只修改源码，本次真实核心编译已停止，不发布新的固件二进制。

## OLED 布局渲染预览

`render_oled.py` 使用 Adafruit GFX 的 `glcdfont.c` 按实际 5×7 点阵绘制 128×64 屏幕预览，检查字号、边界和底部信息是否重叠：

```powershell
python tests/render_oled.py
```

输出文件为 `docs/oled-preview.png`、`docs/oled-preview-max.png`、`docs/oled-preview-missing.png` 和 `docs/oled-preview-countdown.png`。预览仅用于核对排版，不代表实时硬件读数。
