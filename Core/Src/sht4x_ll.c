#include "sht4x_ll.h"

static uint8_t crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF; // Sensirion CRC-8 init (poly 0x31)
    for (uint8_t i=0; i<len; ++i) {
        crc ^= data[i];
        for (uint8_t b=0; b<8; ++b) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static float to_temp(uint16_t ticks) { return ((175.0f * ticks) / 65535.0f) - 45.0f; }
static float to_rh  (uint16_t ticks) { return ((125.0f * ticks) / 65535.0f) - 6.0f; }

sht4x_reading_t SHT4x_ReadSingleShot(uint8_t cmd)
{
    sht4x_reading_t out = {0};
    uint8_t tx = cmd;
    if (HAL_I2C_Master_Transmit(&hi2c1, SHT4X_ADDR, &tx, 1, HAL_MAX_DELAY) != HAL_OK) return out;

    switch (cmd) {
        case SHT4X_CMD_HIGH_PREC: HAL_Delay(10); break;
        case SHT4X_CMD_MED_PREC:  HAL_Delay(5);  break;
        default:                  HAL_Delay(2);  break;
    }
    uint8_t rx[6] = {0};
    if (HAL_I2C_Master_Receive(&hi2c1, SHT4X_ADDR, rx, 6, HAL_MAX_DELAY) != HAL_OK) return out;
    if (crc8(rx,2) != rx[2]) return out;
    if (crc8(rx+3,2) != rx[5]) return out;

    uint16_t t = ((uint16_t)rx[0]<<8) | rx[1];
    uint16_t h = ((uint16_t)rx[3]<<8) | rx[4];
    out.temp_c = to_temp(t);
    out.rh     = to_rh(h);
    out.ok     = 1;
    return out;
}
