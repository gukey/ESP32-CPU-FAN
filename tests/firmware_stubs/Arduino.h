#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
using std::isfinite;
inline uint32_t testTime = 0;
inline int pwmOutput = 0;
inline uint32_t millis() { return testTime; }
inline void delay(uint32_t ms) { testTime += ms; }
template<class T> T constrain(T x, T low, T high) { return std::min(high, std::max(low, x)); }
inline int map(int v,int a,int b,int c,int d) { return (v-a)*(d-c)/(b-a)+c; }
constexpr int OUTPUT=1, LOW=0;
inline void pinMode(int,int) {}
inline void digitalWrite(int,int v) { pwmOutput=v; }
inline void ledcWrite(int,int v) { pwmOutput=v; }
inline bool ledcAttach(int,int,int) { return true; }
inline double ledcSetup(int,int,int) { return 15000; }
inline void ledcAttachPin(int,int) {}
struct SerialMock { void begin(int) {} template<class T> void println(T) {} };
inline SerialMock Serial;
struct WireMock { void begin() {} void setTimeOut(int) {} void beginTransmission(int) {} int endTransmission(){return 1;} };
inline WireMock Wire;
