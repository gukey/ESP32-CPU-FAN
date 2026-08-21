#include <Arduino.h>
#include "BluetoothSerial.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

// 全局变量声明
unsigned long lastConnectedTime = 0;
unsigned long lastCheckTime = 0;
unsigned long lastDataReceivedTime = 0;
const unsigned long checkInterval = 3000;
const unsigned long sleepTimeout = 5000;
const unsigned long dataTimeoutInterval = 12000;
bool isSleeping = false;
bool btConnected = false;
bool displayOn = true;
int sleepCountdown = 0;  // 休眠倒计时

// 硬件相关定义
const int gpioPin = 5;
const int ledChannel = 0;
const int resolution = 8;
int frequency = 150;
int dutyCycle = 20;
int mode = 1;  // 修改：默认模式改为Quiet模式(1)
int ROM11;

// EEPROM相关
int ROM0 = 0;
long ROM = 150507595;

// 自定义参数
int Customize = 0;
float zdya1 = 1;
float zdya2 = 0;
float zdyb1 = 1;
float zdyb2 = 0;
int pwm0wd = 30;
int pwm50wd = 50;
int pwm100wd = 70;

// OLED显示屏设置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 传感器数据
float cpuValue = 0.0;
float gpuValue = 0.0;
float maxCpuValue = 0.0;
float maxGpuValue = 0.0;
float maxValue = 50;

// 计时变量
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 30000;

// 按钮相关
const int buttonPins[] = {12, 13, 14, 15, 16};
bool buttonStates[] = {false, false, false, false, false};
unsigned long buttonPressTimes[] = {0, 0, 0, 0, 0};
const unsigned long longPressDuration = 500;
bool enableDisplay = true;

BluetoothSerial SerialBT;
const char* modechar;

void setup() {
  Serial.begin(115200);

  // EEPROM初始化
  EEPROM.begin(8);
  ROM11 = EEPROM.read(6);
  if(ROM11 == 201){
    EEPROM.get(ROM0, ROM);
  } else {
    EEPROM.put(ROM0, ROM);
    EEPROM.write(6, 201);
    EEPROM.commit();
  }

  // 参数解析
  frequency = ROM / 1000000;
  pwm0wd = (ROM % 1000000) / 10000;
  pwm50wd = (ROM % 10000) / 100;
  pwm100wd = ROM % 100;

  // 蓝牙初始化
  SerialBT.begin("esp32散热器");
  
  // PWM初始化
  SerialBT.setTimeout(80);
  ledcSetup(ledChannel, frequency*100, resolution);
  ledcAttachPin(gpioPin, ledChannel);
  ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));

  // 按钮初始化
  for(int i=0; i<5; i++){
    pinMode(buttonPins[i], INPUT_PULLDOWN);
  }

  // OLED初始化
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(1);
  }
  display.ssd1306_command(SSD1306_DISPLAYON);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  modeDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  static unsigned long lastCountdownUpdate = 0;

  // 连接状态检查
  if(currentMillis - lastCheckTime >= checkInterval){
    lastCheckTime = currentMillis;
    bool currentStatus = SerialBT.connected();
  
    // 修改蓝牙连接状态检测部分
    if(currentStatus != btConnected){
      btConnected = currentStatus;
      if(btConnected){
        lastConnectedTime = currentMillis;
        lastDataReceivedTime = currentMillis; // 重置数据接收时间
        isSleeping = false;
        sleepCountdown = 0;
        // 修改：仅重置最大值，保留当前值
        maxCpuValue = 0;
        maxGpuValue = 0;
        updatePWM(); // 立即更新风扇速度
        updateDisplay();
      }
    }
  }

  // 超时检测
  bool dataTimedOut = (currentMillis - lastDataReceivedTime) >= dataTimeoutInterval;
  bool btDisconnectTimeout = !btConnected && (currentMillis - lastConnectedTime >= sleepTimeout);
  
  // 启动倒计时 - 修改：改为3秒倒计时
  if((dataTimedOut || btDisconnectTimeout) && sleepCountdown == 0 && !isSleeping){
    sleepCountdown = 3;  // 修改：从6秒改为3秒
    lastCountdownUpdate = currentMillis;
  }

  // 处理倒计时
  if(sleepCountdown > 0){
    if(currentMillis - lastCountdownUpdate >= 1000){
      lastCountdownUpdate = currentMillis;
      sleepCountdown--;
      updateDisplay();
    
      if(sleepCountdown <= 0){
        isSleeping = true;
        dutyCycle = 0;
        ledcWrite(ledChannel, 0);
      }
    }
  }

  // 蓝牙数据处理
  if(SerialBT.available()){
    String input = SerialBT.readStringUntil('\n');
    input.trim();
    parseBluetoothData(input);
  }

  // 按钮处理
  for(int i=0; i<5; i++){
    bool buttonState = digitalRead(buttonPins[i]) == HIGH;
    if(buttonState && !buttonStates[i]){
      buttonPressTimes[i] = currentMillis;
      buttonStates[i] = true;
    } else if(!buttonState && buttonStates[i]){
      if(currentMillis - buttonPressTimes[i] >= longPressDuration){
        handleButtonLongPress(i);
      } else {
        handleButtonPress(i);
      }
      buttonStates[i] = false;
    }
  }

  // 定期更新
  if(currentMillis - lastUpdateTime >= updateInterval){
    maxValue = max(maxCpuValue, maxGpuValue);
    updatePWM();
    lastUpdateTime = currentMillis;
  }

  updateDisplay();
}

void parseBluetoothData(const String& data) {
  if(data.length() == 0){
    return;
  }
  lastDataReceivedTime = millis();
  if(data.startsWith("CPU")){
    float value = data.substring(3).toFloat();
    if(value > maxCpuValue) maxCpuValue = value;
    cpuValue = value;
  } else if(data.startsWith("GPU")){
    float value = data.substring(3).toFloat();
    if(value > maxGpuValue) maxGpuValue = value;
    gpuValue = value;
  }
  
    // 强制唤醒设备
  isSleeping = false;
  sleepCountdown = 0;
  if(SerialBT.hasClient()){
    SerialBT.println("ACK");
  }
  updatePWM(); // 触发PWM更新
}

void handleButtonPress(int buttonIndex) {
  if(sleepCountdown > 0){
    sleepCountdown = 0;
    isSleeping = false;
    lastDataReceivedTime = millis();
    lastConnectedTime = millis();
    updatePWM();
    updateDisplay();
    return;
  }

  if(isSleeping){
    isSleeping = false;
    lastDataReceivedTime = millis();
    lastConnectedTime = millis();
    updatePWM();
    updateDisplay();
    return;
  }

  if(!enableDisplay){
    switch(buttonIndex){
      case 0: enableDisplay = !enableDisplay; break;
      case 1: 
        switch(Customize){
          case 0: pwm0wd = min(pwm0wd+1, pwm50wd-1); break;
          case 1: pwm50wd = min(pwm50wd+1, pwm100wd-1); break;
          case 2: pwm100wd = min(pwm100wd+1, 99); break;
          case 3: frequency = min(frequency+1, 999); break;
        }
        break;
      case 2: 
        switch(Customize){
          case 0: pwm0wd = max(pwm0wd-1, 0); break;
          case 1: pwm50wd = max(pwm50wd-1, pwm0wd+1); break;
          case 2: pwm100wd = max(pwm100wd-1, pwm50wd+1); break;
          case 3: frequency = max(frequency-1, 1); break;
        }
        break;
      case 3: Customize = min(Customize+1, 3); break;
      case 4: Customize = max(Customize-1, 0); break;
    }
    return;
  }

  switch(buttonIndex){
    case 0: mode = (mode % 5) + 1; break;
    case 1: 
      mode = 4;
      dutyCycle = min(dutyCycle+1, 100);
      ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));
      break;
    case 2: 
      mode = 4;
      dutyCycle = max(dutyCycle-1, 0);
      ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));
      break;
    case 3: 
      mode = 4;
      dutyCycle = min(dutyCycle+20, 100);
      ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));
      break;
    case 4: 
      mode = 4;
      dutyCycle = max(dutyCycle-20, 0);
      ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));
      break;
  }
  modeDisplay();
}

void handleButtonLongPress(int buttonIndex) {
  if(!enableDisplay){
    switch(buttonIndex){
      case 0: 
        ROM = (pwm100wd) + (pwm50wd*100) + (pwm0wd*10000) + (frequency*1000000);
        EEPROM.put(ROM0, ROM);
        EEPROM.commit();
        enableDisplay = !enableDisplay;
        ledcWrite(gpioPin, frequency*100);
        break;
      case 1: 
        switch(Customize){
          case 0: pwm0wd = min(pwm0wd+20, pwm50wd-1); break;
          case 1: pwm50wd = min(pwm50wd+20, pwm100wd-1); break;
          case 2: pwm100wd = min(pwm100wd+20, 99); break;
          case 3: frequency = min(frequency+10, 999); break;
        }
        break;
      case 2: 
        switch(Customize){
          case 0: pwm0wd = max(pwm0wd-20, 0); break;
          case 1: pwm50wd = max(pwm50wd-20, pwm0wd+1); break;
          case 2: pwm100wd = max(pwm100wd-20, pwm50wd+1); break;
          case 3: frequency = max(frequency-10, 1); break;
        }
        break;
    }
    return;
  }

  switch(buttonIndex){
    case 0: enableDisplay = !enableDisplay; break;
    case 1: 
      mode = 4;
      dutyCycle = 100;
      ledcWrite(ledChannel, 255);
      break;
    case 2: 
      mode = 4;
      dutyCycle = 0;
      ledcWrite(ledChannel, 0);
      break;
  }
  modeDisplay();
}

void updateDisplay() {
  // 屏幕电源管理
  if(isSleeping){
    if(displayOn){
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      displayOn = false;
      Serial.println("Screen OFF");
    }
    return;
  }else{
    if(!displayOn){
      display.ssd1306_command(SSD1306_DISPLAYON);
      displayOn = true;
      display.clearDisplay();
      Serial.println("Screen ON");
      delay(100);
    }
  }

  // 显示倒计时 - 修改：适配3秒倒计时
  // 显示倒计时 - 最大化显示
if(sleepCountdown > 0){
  display.clearDisplay();
  
  // 只显示正中间的超大倒计时数字
  display.setTextSize(9); // 最大字体
  String countdownStr = String(sleepCountdown);
  
  // 计算超大字体位置（完全居中）
  int charWidth = 6 * 7; // 7号字体每个字符约42像素宽
  int charHeight = 8 * 7; // 7号字体每个字符约56像素高
  int textWidth = countdownStr.length() * charWidth;
  int xPos = (128 - textWidth) / 2; // 水平居中
  int yPos = (64 - charHeight) / 2; // 垂直居中
  
  // 如果字体太大超出屏幕，调整到合适大小
  if(textWidth > 128 || charHeight > 64){
    display.setTextSize(6); // 如果7号太大，使用6号
    charWidth = 6 * 6; // 重新计算宽度
    charHeight = 8 * 6; // 重新计算高度
    textWidth = countdownStr.length() * charWidth;
    xPos = (128 - textWidth) / 2;
    yPos = (64 - charHeight) / 2;
  }
  
  display.setCursor(xPos, yPos);
  display.print(sleepCountdown);
  
  display.display();
  return;
}

  // 正常显示内容
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  if(!enableDisplay){
    display.print("Customize ");
    display.println();
    switch(Customize){
      case 0: display.println(" select:0% "); break;
      case 1: display.println(" select:50% "); break;
      case 2: display.println(" select:100% "); break;
      case 3: display.println(" select:Frequency "); break;
    }
    display.println(" 0%   50%  100%");
    display.print("[");
    display.print(pwm0wd);
    display.print("] [");
    display.print(pwm50wd);
    display.print("] [");
    display.print(pwm100wd);  
    display.print("]");
    display.println();
    display.print(F("Frequency: "));
    display.print(frequency*100);
    display.println("Hz");
  } else {
    display.print(F("Mode: "));
    display.println(modechar);
    display.print(F("Speed: "));
    display.print(dutyCycle);
    display.println("%");
    display.print(F("Freq: "));
    display.print(frequency*100);
    display.println("Hz");
    display.print(F("CPU:"));
    display.print(cpuValue);
    display.print(("  GPU:"));
    display.println(gpuValue);
    display.setCursor(80,56);
    if(btConnected){
      display.print(F("CONNECTED"));
    } else {
      unsigned long remain = sleepTimeout - (millis() - lastConnectedTime);
      display.print("WAIT:");
      display.print(remain/1000);
    }
  }
  display.display();
}

void updatePWM() {
  if(isSleeping) return;
   // 新增：仅保留最大值记录
  maxCpuValue = max(maxCpuValue, cpuValue);
  maxGpuValue = max(maxGpuValue, gpuValue);

  if(millis() - lastDataReceivedTime > dataTimeoutInterval){
    dutyCycle = pwm0wd; 
  } else {
    dutyCycle = processBluetoothValue(maxValue);
  }
  
  ledcWrite(ledChannel, map(dutyCycle, 0, 100, 0, 255));
}

int processBluetoothValue(int value) {
  // 定义6个温度阈值
  bool highTemp1 = (cpuValue > 80 || gpuValue > 80);
  bool highTemp2 = (cpuValue > 75 || gpuValue > 75);
  bool highTemp3 = (cpuValue > 70 || gpuValue > 70);
  bool highTemp4 = (cpuValue > 65 || gpuValue > 65);
  bool mediumTemp1 = (cpuValue > 60 || gpuValue > 60);
  bool mediumTemp2 = (cpuValue > 55 || gpuValue > 55);
  
  //使用constrain()函数限制范围：
  //所有计算出的速度都通过constrain(calculatedSpeed, 0, 100)限制在0-100范围内
  int calculatedSpeed = 0; // 用于存储计算出的速度
  
  switch(mode){
    case 1: 
      // 安静模式：根据6个温度阈值分段控制
      if (highTemp1) {
        // 温度>80℃：使用1.2*温度+30，但限制最大值为100
        calculatedSpeed = value * 1.2 + 30;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp2) {
        // 温度>75℃：使用0.9*温度+25
        calculatedSpeed = value * 0.9 + 25;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp3) {
        // 温度>70℃：使用0.8*温度+15
        calculatedSpeed = value * 0.8 + 15;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp4) {
        // 温度>65℃：使用0.5*温度+5
        calculatedSpeed = value * 0.5 + 5;
        return constrain(calculatedSpeed, 0, 100);
      } else if (mediumTemp1) {
        // 温度>60℃：使用0.4*温度+5
        calculatedSpeed = value * 0.4 + 5;
        return constrain(calculatedSpeed, 0, 100);
      } else if (mediumTemp2) {
        // 温度>55℃：使用0.3*温度+5
        calculatedSpeed = value * 0.3 + 5;
        return constrain(calculatedSpeed, 0, 100);
      } else {
        // 温度≤55℃：使用默认值
        calculatedSpeed = value * 0.2 + 5;
        return constrain(calculatedSpeed, 0, 100);
      }
    case 2: 
      // 正常模式
      if (highTemp1) {
        calculatedSpeed = value * 1.3 + 35;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp2) {
        calculatedSpeed = value * 1.1 + 25;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp3) {
        calculatedSpeed = value * 0.95 + 15;
        return constrain(calculatedSpeed, 0, 100);
      } else {
        calculatedSpeed = value * 0.8 + 10;
        return constrain(calculatedSpeed, 0, 100);
      }
    case 3: 
      // 高速模式
      if (highTemp1) {
        calculatedSpeed = value * 1.5 + 40;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp2) {
        calculatedSpeed = value * 1.3 + 30;
        return constrain(calculatedSpeed, 0, 100);
      } else if (highTemp3) {
        calculatedSpeed = value * 1.15 + 20;
        return constrain(calculatedSpeed, 0, 100);
      } else {
        calculatedSpeed = value * 1.0 + 15;
        return constrain(calculatedSpeed, 0, 100);
      }
    case 4: 
      return dutyCycle; // 手动模式保持不变，dutyCycle应该在0-100范围内
    case 5: 
      // 自定义模式：根据温度调整
      float pwm = value * zdya1 + zdya2;
      if (highTemp1) {
        pwm = pwm * 1.4;
      } else if (highTemp2) {
        pwm = pwm * 1.3;
      } else if (highTemp3) {
        pwm = pwm * 1.2;
      } else if (highTemp4) {
        pwm = pwm * 1.1;
      }
      if(pwm >= 50) pwm = value * zdyb1 + zdyb2;
      return constrain(pwm, 0, 100);
  }
  return value;
}

void modeDisplay() {
  zdya1 = 50.0/(pwm50wd-pwm0wd);
  zdya2 = -50.0*pwm0wd/(pwm50wd-pwm0wd);
  zdyb1 = 50.0/(pwm100wd-pwm50wd);
  zdyb2 = 100 - pwm100wd*50.0/(pwm100wd-pwm50wd);
  
  switch(mode){
    case 1: modechar = "1(._.)  Quiet"; break;
    case 2: modechar = "2(o_o) Normal"; break;
    case 3: modechar = "3(O_O)  Speed"; break;
    case 4: modechar = "S(=_=) Manual"; break;
    case 5: modechar = "z(^_^) Custom"; break;
  }
  updateDisplay();
}
