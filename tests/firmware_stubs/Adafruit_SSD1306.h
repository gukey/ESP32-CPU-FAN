#pragma once
constexpr int SSD1306_DISPLAYON=1,SSD1306_DISPLAYOFF=0,SSD1306_SWITCHCAPVCC=2,SSD1306_WHITE=1;
class Adafruit_SSD1306 {
public:
 int lastNumber=-1, textSize=1;
 Adafruit_SSD1306(int,int,WireMock*,int){}
 bool begin(int,int,bool,bool){return false;}
 void ssd1306_command(int){}
 void clearDisplay(){} void setCursor(int,int){} void setTextSize(int size){textSize=size;} void setTextColor(int){}
 void print(int value){lastNumber=value;}
 template<class T> void print(T){} template<class T> void println(T){}
 void display(){}
 void drawFastHLine(int,int,int,int){}
};
