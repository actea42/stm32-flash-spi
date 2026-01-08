#pragma once
#include "main.h"
#include "i2c_on_demand.h"

#define SHT4X_ADDR           (0x44 << 1)
#define SHT4X_CMD_HIGH_PREC  0xFD
#define SHT4X_CMD_MED_PREC   0xF6
#define SHT4X_CMD_LOW_PREC   0xE0

typedef struct { float temp_c; float rh; int ok; } sht4x_reading_t;

sht4x_reading_t SHT4x_ReadSingleShot(uint8_t cmd);
