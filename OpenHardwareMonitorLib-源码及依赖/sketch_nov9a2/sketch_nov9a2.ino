#include <Arduino.h>
#include "BluetoothSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>
#include <esp_task_wdt.h>
#include <atomic>
#include <math.h>

// 无按键硬件：仅使用 Quiet 温控。保持原先 15 kHz PWM 默认值。
const int fanPin = 5;
const int pwmChannel = 0;
const int pwmResolution = 8;
const uint32_t dataTimeoutMs = 12000;
const uint32_t recoveryTimeoutMs = 60000;
const uint32_t rampIntervalMs = 100;
const float risePercentPerSecond = 10.0f;
const float fallPercentPerSecond = 3.0f;
const int startupDutyPercent = 30; // 试用值，需按实际风扇最低启动占空比调整
const uint32_t startupHoldMs = 1000;
float rampDuty = 0;
uint32_t lastRampTime = 0;
uint32_t fanStartTime = 0;
BluetoothSerial SerialBT;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
std::atomic<bool> disconnectedEvent(false);
bool connected = false;
bool dataValid = false;
bool recoveryArmed = false;
bool watchdogReady = false;
bool displayReady = false;
bool displayOn = false;
bool countdownActive = false;
uint32_t countdownStartTime = 0;
bool cpuValid = false;
bool gpuValid = false;
float cpuValue = 0;
float gpuValue = 0;
uint32_t cpuTime = 0;
uint32_t gpuTime = 0;
uint32_t lastDataTime = 0;
uint32_t lastDisplayTime = 0;
int dutyCycle = 0;
int frequencyHz = 15000;
char receiveBuffer[32];
size_t receiveLength = 0;
bool discardLine = false;
uint32_t lastByteTime = 0;

void writeFanDuty(int percent) {
  dutyCycle = constrain(percent, 0, 100);
  int duty = map(dutyCycle, 0, 100, 0, 255);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(fanPin, duty);
#else
  ledcWrite(pwmChannel, duty);
#endif
}

void stopFan() {
  // 只在有效通信结束时启动一次关屏倒计时，停转不等待屏幕。
  if(dataValid && displayReady && !countdownActive) {
    countdownActive = true;
    countdownStartTime = millis();
    lastDisplayTime = countdownStartTime - 200;
  }
  dataValid = false;
  cpuValid = false;
  gpuValid = false;
  writeFanDuty(0);
  rampDuty = 0;
  lastRampTime = millis();
}

// 回调仅通知主循环；不在蓝牙任务中操作屏幕或重启蓝牙。
void bluetoothEvent(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if(event == ESP_SPP_CLOSE_EVT) disconnectedEvent.store(true);
}

bool setupWatchdog() {
  esp_err_t result;
#if ESP_IDF_VERSION_MAJOR >= 5
  esp_task_wdt_config_t config = {};
  config.timeout_ms = 8000;
  config.idle_core_mask = 0;
  config.trigger_panic = true;
  result = esp_task_wdt_init(&config);
  if(result == ESP_ERR_INVALID_STATE) result = esp_task_wdt_reconfigure(&config);
#else
  result = esp_task_wdt_init(8, true);
#endif
  if(result != ESP_OK) return false;
  if(esp_task_wdt_status(NULL) != ESP_OK && esp_task_wdt_add(NULL) != ESP_OK) return false;
  return true;
}

float quietDuty(float temperature) {
  const float temperatures[] = {0, 55, 60, 65, 70, 75, 80};
  // 锚点取原 Quiet 在整数阈值处的默认输出；80°C 提前放行全速。
  const float duties[] = {5, 16, 23, 31, 40, 75, 100};
  if(temperature <= 0) return duties[0];
  for(size_t i = 1; i < 7; ++i) {
    if(temperature <= temperatures[i]) {
      float fraction = (temperature - temperatures[i-1]) / (temperatures[i] - temperatures[i-1]);
      return duties[i-1] + fraction * (duties[i] - duties[i-1]);
    }
  }
  return 100;
}

void updateFan(uint32_t now) {
  if(!connected || !dataValid || disconnectedEvent.load() || !SerialBT.hasClient()) {
    writeFanDuty(0);
    rampDuty = 0;
    lastRampTime = now;
    return;
  }
  if(cpuValid && uint32_t(now - cpuTime) >= dataTimeoutMs) cpuValid = false;
  if(gpuValid && uint32_t(now - gpuTime) >= dataTimeoutMs) gpuValid = false;
  if(!cpuValid && !gpuValid) {
    stopFan();
    return;
  }
  float temperature = cpuValid ? cpuValue : gpuValue;
  if(gpuValid && gpuValue > temperature) temperature = gpuValue;
  // 高温和断联绕过渐变；启动脉冲仅在有效通信时执行。
  if(temperature >= 80) {
    rampDuty = 100;
    lastRampTime = now;
    writeFanDuty(100);
    return;
  }
  if(rampDuty == 0) {
    rampDuty = startupDutyPercent;
    fanStartTime = now;
    lastRampTime = now;
    writeFanDuty(startupDutyPercent);
    return;
  }
  uint32_t elapsed = uint32_t(now - lastRampTime);
  if(elapsed < rampIntervalMs) return;
  lastRampTime = now;
  float target = quietDuty(temperature);
  if(uint32_t(now - fanStartTime) < startupHoldMs && target < startupDutyPercent) target = startupDutyPercent;
  float difference = target - rampDuty;
  if(fabsf(difference) < 2.0f) return;
  // 长时间阻塞后也不一次跳过整个渐变过程。
  float seconds = (elapsed > 250 ? 250 : elapsed) / 1000.0f;
  float step = (difference > 0 ? risePercentPerSecond : fallPercentPerSecond) * seconds;
  if(difference > 0) rampDuty += difference < step ? difference : step;
  else rampDuty -= -difference < step ? -difference : step;
  writeFanDuty(static_cast<int>(roundf(rampDuty)));
}

void parseLine() {
  receiveBuffer[receiveLength] = '\0';
  bool isCpu = strncmp(receiveBuffer, "CPU", 3) == 0;
  bool isGpu = strncmp(receiveBuffer, "GPU", 3) == 0;
  if(receiveLength <= 3 || (!isCpu && !isGpu) || !SerialBT.hasClient()) return;
  char *end = nullptr;
  float value = strtof(receiveBuffer + 3, &end);
  if(end == receiveBuffer + 3 || *end != '\0' || !isfinite(value) || value < 0 || value > 120) return;
  // 使用接收时刻；后续超时检查必须重新获取 millis()。
  uint32_t now = millis();
  if(isCpu) { cpuValue = value; cpuTime = now; cpuValid = true; }
  else { gpuValue = value; gpuTime = now; gpuValid = true; }
  connected = true;
  lastDataTime = now;
  dataValid = true;
  if(countdownActive) lastDisplayTime = now - 200;
  countdownActive = false;
  recoveryArmed = true;
  SerialBT.println("ACK");
}

void receiveData() {
  // 超长行整行丢弃；分包等待不阻塞主循环，每轮最多读取 128 字节。
  if((receiveLength || discardLine) && uint32_t(millis() - lastByteTime) >= 1000) {
    receiveLength = 0;
    discardLine = false;
  }
  for(int count = 0; count < 128 && SerialBT.available(); ++count) {
    int value = SerialBT.read();
    if(value < 0) break;
    lastByteTime = millis();
    char c = static_cast<char>(value);
    if(c == '\n') {
      if(!discardLine) parseLine();
      receiveLength = 0;
      discardLine = false;
    } else if(c != '\r' && !discardLine) {
      if(c == '\0' || receiveLength >= sizeof(receiveBuffer) - 1) {
        discardLine = true;
        receiveLength = 0;
      } else receiveBuffer[receiveLength++] = c;
    }
  }
}

void recoverBluetooth(uint32_t now) {
  if(!connected || !recoveryArmed || uint32_t(now - lastDataTime) < recoveryTimeoutMs) return;
  // 一个有效通信周期只恢复一次；断开/休眠后静候 PC 连接，不周期性重启。
  recoveryArmed = false;
  stopFan();
  connected = false;
  receiveLength = 0;
  discardLine = false;
  SerialBT.end();
  delay(300);
  if(!SerialBT.begin("esp32散热器")) Serial.println("Bluetooth recovery failed; waiting.");
}

void updateDisplay(uint32_t now) {
  if(!displayReady || uint32_t(now - lastDisplayTime) < 200) return;
  lastDisplayTime = now;
  uint32_t countdownElapsed = uint32_t(now - countdownStartTime);
  if(countdownActive && countdownElapsed >= 3000) countdownActive = false;
  bool wantOn = (connected && dataValid) || countdownActive;
  if(wantOn != displayOn) {
    display.ssd1306_command(wantOn ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
    displayOn = wantOn;
  }
  if(!displayOn) return;
  display.clearDisplay();
  if(countdownActive) {
    // 默认字体 6x8，放大 6 倍后在 128x64 屏幕居中显示。
    display.setTextSize(6);
    display.setCursor(46, 8);
    display.print(3 - static_cast<int>(countdownElapsed / 1000));
    display.display();
    return;
  }
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Mode: Quiet Auto");
  display.print("Speed: "); display.print(dutyCycle); display.println("%");
  display.print("Freq: "); display.print(frequencyHz); display.println("Hz");
  display.print("CPU: "); if(cpuValid) display.println(cpuValue); else display.println("--");
  display.print("GPU: "); if(gpuValid) display.println(gpuValue); else display.println("--");
  display.println("CONNECTED");
  display.display();
}

void setup() {
  // 先确保输出低电平；蓝牙、OLED 初始化和看门狗复位都不启动风扇。
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);
  Serial.begin(115200);
  // 仅读取旧 EEPROM 保存的 PWM 频率，不再写入按键/曲线配置。
  if(EEPROM.begin(8)) {
    if(EEPROM.read(6) == 201) {
      int32_t packed = 0;
      EEPROM.get(0, packed);
      int savedFrequency = packed / 1000000;
      if(savedFrequency >= 1 && savedFrequency <= 999) frequencyHz = savedFrequency * 100;
    }
    EEPROM.end();
  }
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if(!ledcAttach(fanPin, frequencyHz, pwmResolution)) {
    frequencyHz = 15000;
    ledcAttach(fanPin, frequencyHz, pwmResolution);
  }
#else
  if(ledcSetup(pwmChannel, frequencyHz, pwmResolution) == 0) {
    frequencyHz = 15000;
    ledcSetup(pwmChannel, frequencyHz, pwmResolution);
  }
  ledcAttachPin(fanPin, pwmChannel);
#endif
  writeFanDuty(0);
  watchdogReady = setupWatchdog();
  if(!watchdogReady) Serial.println("Watchdog initialization failed");
  SerialBT.register_callback(bluetoothEvent);
  if(!SerialBT.begin("esp32散热器")) Serial.println("Bluetooth initialization failed");
  Wire.begin();
  Wire.setTimeOut(50);
  Wire.beginTransmission(0x3C);
  if(Wire.endTransmission() == 0) {
    displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false);
  }
  if(displayReady) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  } else Serial.println("OLED unavailable; fan control continues");
}

void loop() {
  // hasClient() 不等待连接。断开事件即使发生于快速重连期间也清除旧温度。
  bool client = SerialBT.hasClient();
  if(disconnectedEvent.exchange(false) || (connected && !client)) {
    stopFan();
    recoveryArmed = false;
    receiveLength = 0;
    discardLine = false;
    // 清空断开之前残留的数据，下一轮只接收新帧。
    for(int i = 0; i < 512 && SerialBT.available(); ++i) SerialBT.read();
  }
  connected = client;
  if(connected) receiveData();
  updateFan(millis());
  recoverBluetooth(millis());
  updateDisplay(millis());
  // 只在主循环完成后喂狗，同时让出 CPU 给蓝牙及空闲任务。
  delay(1);
  if(watchdogReady) esp_task_wdt_reset();
}
