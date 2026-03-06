
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7735.h>  // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>


//Screen Setup
#define TFT_CS 22
#define TFT_RST 20
#define TFT_DC 21
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);



void setup() {
  tft.initR(INITR_144GREENTAB);
}

void loop() {
  // put your main code here, to run repeatedly:
}
