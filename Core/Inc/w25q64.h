#ifndef W25Q64_H
#define W25Q64_H

#include "stm32l4xx_hal.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Bind the flash driver to a SPI handle and CS GPIO.
 *  The driver does not initialize SPI clocks or pins; do that in Cube or elsewhere.
 */
void W25Q64_Bind(SPI_HandleTypeDef *hspi, GPIO_TypeDef *cs_gpio, uint16_t cs_pin);

/* Low-level commands */
void W25Q64_WaitWhileBusy(void);
void W25Q64_WriteEnable(void);
void W25Q64_ReleaseFromDeepPowerDown(void);   /* 0xAB */
void W25Q64_EnterDeepPowerDown(void);         /* 0xB9 */

void W25Q64_Read(uint32_t addr, uint8_t *buf, size_t len);       /* 0x03 */
void W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, size_t len); /* 0x02, up to 256B per chunk */
void W25Q64_SectorErase4K(uint32_t addr);                         /* 0x20 */

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_H */
