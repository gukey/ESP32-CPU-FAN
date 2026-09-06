#pragma once
#include <deque>
#include <string>
enum esp_spp_cb_event_t { ESP_SPP_CLOSE_EVT };
struct esp_spp_cb_param_t {};
class BluetoothSerial {
public:
 bool client=false, beginResult=true;
 int begins=0, acks=0;
 std::deque<char> rx;
 bool hasClient(){return client;}
 int available(){return int(rx.size());}
 int read(){if(rx.empty())return -1; ++testTime; char c=rx.front();rx.pop_front();return static_cast<unsigned char>(c);}
 void println(const char*){++acks;}
 bool begin(const char*){++begins;return beginResult;}
 void end(){client=false;rx.clear();}
 void register_callback(void (*)(esp_spp_cb_event_t,esp_spp_cb_param_t*)){}
 void input(const std::string& s){for(char c:s)rx.push_back(c);}
};
