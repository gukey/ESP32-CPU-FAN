#pragma once
struct EepromMock {
 bool begin(int){return true;} int read(int){return 0;}
 void get(int,int32_t& v){v=150507595;} void end(){}
};
inline EepromMock EEPROM;
