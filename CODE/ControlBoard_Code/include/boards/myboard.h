#pragma once

#include "boards/pico2.h"
#define TFT_DRIVER 1
/* Pins connected to the TFT */
#define TFT_CS_PIN   22
#define TFT_SCK_PIN  18
#define TFT_MOSI_PIN 19
#define TFT_RS_PIN   21
#define TFT_RST_PIN  20

/* SPI peripheral */
#define TFT_SPI_DEV spi0

/* SPI clock speed */
#define TFT_BAUDRATE (24000000)

/* Orientation options */
#define TFT_SWAP_XY 0
#define TFT_FLIP_X  1
#define TFT_FLIP_Y  1