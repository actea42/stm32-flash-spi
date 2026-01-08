
// w25q64.c (UPDATED OPTIONAL)
#include "w25q64.h"
#include <string.h>

// Winbond command set
#define CMD_WREN 0x06
#define CMD_RDSR 0x05
#define CMD_PP   0x02
#define CMD_READ 0x03
#define CMD_SECTOR_ERASE 0x20
#define CMD_DP   0xB9
#define CMD_RELEASE 0xAB
#define CMD_RDID 0x9F

static struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *cs_gpio;
    uint16_t cs_pin;
} w25_ctx = {0};

static inline void CS_L(void){ HAL_GPIO_WritePin(w25_ctx.cs_gpio, w25_ctx.cs_pin, GPIO_PIN_RESET); }
static inline void CS_H(void){ HAL_GPIO_WritePin(w25_ctx.cs_gpio, w25_ctx.cs_pin, GPIO_PIN_SET); }

void W25Q64_Bind(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_gpio, uint16_t cs_pin){
    w25_ctx.hspi = hspi; w25_ctx.cs_gpio = cs_gpio; w25_ctx.cs_pin = cs_pin;
    CS_H();
}

void W25Q64_WaitWhileBusy(void){
    uint8_t cmd = CMD_RDSR, sr;
    do {
        CS_L(); HAL_SPI_Transmit(w25_ctx.hspi, &cmd, 1, HAL_MAX_DELAY);
        HAL_SPI_Receive (w25_ctx.hspi, &sr, 1, HAL_MAX_DELAY); CS_H();
    } while (sr & 0x01); // WIP
}

void W25Q64_WriteEnable(void){
    uint8_t cmd = CMD_WREN; CS_L();
    HAL_SPI_Transmit(w25_ctx.hspi, &cmd, 1, HAL_MAX_DELAY);
    CS_H();
}

void W25Q64_ReleaseFromDeepPowerDown(void){
    uint8_t cmd = CMD_RELEASE; CS_L();
    HAL_SPI_Transmit(w25_ctx.hspi, &cmd, 1, HAL_MAX_DELAY);
    CS_H();
    HAL_Delay(1); // tRES safety
}

void W25Q64_EnterDeepPowerDown(void){
    uint8_t cmd = CMD_DP; CS_L();
    HAL_SPI_Transmit(w25_ctx.hspi, &cmd, 1, HAL_MAX_DELAY);
    CS_H();
}

void W25Q64_Read(uint32_t addr, uint8_t *buf, size_t len){
    uint8_t cmd[4] = { CMD_READ, (uint8_t)(addr>>16), (uint8_t)(addr>>8), (uint8_t)addr };
    CS_L(); HAL_SPI_Transmit(w25_ctx.hspi, cmd, 4, HAL_MAX_DELAY);
    HAL_SPI_Receive (w25_ctx.hspi, buf, (uint16_t)len, HAL_MAX_DELAY); CS_H();
}

void W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, size_t len){
    while (len){
        uint32_t page_off = addr & 0xFF;
        uint32_t chunk = ((256 - page_off) < len ? (256 - page_off) : len);
        W25Q64_WriteEnable();
        uint8_t cmd[4] = { CMD_PP, (uint8_t)(addr>>16), (uint8_t)(addr>>8), (uint8_t)addr };
        CS_L(); HAL_SPI_Transmit(w25_ctx.hspi, cmd, 4, HAL_MAX_DELAY);
        HAL_SPI_Transmit(w25_ctx.hspi, (uint8_t*)buf, (uint16_t)chunk, HAL_MAX_DELAY); CS_H();
        W25Q64_WaitWhileBusy();
        addr += chunk; buf += chunk; len -= chunk;
    }
}

void W25Q64_SectorErase4K(uint32_t addr){
    W25Q64_WriteEnable();
    uint8_t cmd[4] = { CMD_SECTOR_ERASE, (uint8_t)(addr>>16), (uint8_t)(addr>>8), (uint8_t)addr };
    CS_L(); HAL_SPI_Transmit(w25_ctx.hspi, cmd, 4, HAL_MAX_DELAY); CS_H();
    W25Q64_WaitWhileBusy();
}

// Optional presence check
int W25Q64_ReadJedecID(uint8_t id[3]) {
    if (!id) return -1;
    uint8_t cmd = CMD_RDID;
    CS_L();
    if (HAL_SPI_Transmit(w25_ctx.hspi, &cmd, 1, HAL_MAX_DELAY) != HAL_OK) { CS_H(); return -1; }
    if (HAL_SPI_Receive (w25_ctx.hspi, id, 3, HAL_MAX_DELAY)    != HAL_OK) { CS_H(); return -1; }
    CS_H();
    return 0;
}
