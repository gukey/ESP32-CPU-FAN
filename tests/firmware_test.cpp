// 直接包含真实固件，用模拟硬件验证状态变化；不替代实机测试。
#include <cassert>
#include <iostream>
#include "../sketch_nov9a2/sketch_nov9a2.ino"
void send(const std::string& s){SerialBT.input(s);loop();}
int main(){
 setup();
 assert(dutyCycle==0 && pwmOutput==0 && !displayReady && watchdogReady);
 SerialBT.client=true;loop();
 assert(dutyCycle==0);
 testTime=1234;send("CPU70.5\nGPU60\n");
 assert(dutyCycle==30 && recoveryArmed && SerialBT.begins==1);
 int ack=SerialBT.acks;
 send("CPUnan\nCPUinf\nCPU121\nCPU-1\nCPUabc\n");
 assert(SerialBT.acks==ack && cpuValue==70.5f);
 send("CPU"+std::string(80,'9')+"\nCPU60\n");
 assert(cpuValue==60 && dutyCycle>=23);
 send("CPU");send("65");send("\n");
 assert(cpuValue==65 && dutyCycle>0);
 send("CPU");testTime+=1001;send("99\n");assert(cpuValue==65);
 // 正常断联与长时间休眠不恢复蓝牙，也不让旧温度启动风扇。
 SerialBT.client=false;bluetoothEvent(ESP_SPP_CLOSE_EVT,nullptr);loop();
 assert(dutyCycle==0 && !recoveryArmed);
 testTime+=3600000;loop();assert(SerialBT.begins==1);
 SerialBT.client=true;loop();assert(dutyCycle==0);
 send("GPU80\n");assert(dutyCycle==100 && !cpuValid);
 // 毫秒计数回绕时，新数据仍有效。
 testTime=0xfffffff0u;send("CPU60\n");
 testTime=20;loop();assert(dutyCycle>0);
 // 两个传感器均过期则停转；连接状态无数据仅软恢复一次。
 testTime=lastDataTime+12000;loop();assert(dutyCycle==0);
 testTime=lastDataTime+60000;loop();assert(SerialBT.begins==2 && !recoveryArmed);
 SerialBT.client=true;testTime+=600000;loop();
 assert(SerialBT.begins==2 && dutyCycle==0);
 send("CPU60\n");assert(dutyCycle==30 && recoveryArmed);
 uint32_t cpuSample=cpuTime;
 testTime=cpuSample+11000;send("GPU50\n");
 testTime=cpuSample+12000;loop();assert(!cpuValid && gpuValid && dutyCycle>0);
 // 快速断开重连事件也必须清除旧温度和积压帧。
 SerialBT.input("CPU99\n");bluetoothEvent(ESP_SPP_CLOSE_EVT,nullptr);loop();
 assert(dutyCycle==0 && !cpuValid && !gpuValid);
 // 连续曲线不在 70 度跳速；计时限速保持浮点精度。
 assert(fabsf(quietDuty(70.1f)-quietDuty(70))<0.8f);
 assert(quietDuty(70)==40 && quietDuty(75)==75 && quietDuty(80)==100);
 send("CPU75\n");assert(dutyCycle==30);
 for(int i=0;i<10;++i){testTime+=100;loop();}
 assert(dutyCycle>=39 && dutyCycle<=41);
 send("CPU55\n");int before=dutyCycle;
 for(int i=0;i<10;++i){testTime+=100;loop();}
 assert(before-dutyCycle>=2 && before-dutyCycle<=4);
 send("CPU80\n");assert(dutyCycle==100);
 SerialBT.client=false;loop();assert(dutyCycle==0 && rampDuty==0);
 assert(feeds>0);
 std::cout<<"PASS: startup, parsing, reconnect, sleep, rollover, expiry, one-shot recovery, OLED failure\n";
}
