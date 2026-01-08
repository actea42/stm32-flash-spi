#pragma once
#include "main.h"
#include "stm32l4xx_hal.h"

void I2C1_OnDemand_Init(void);
void I2C1_OnDemand_DeInit(void);
extern I2C_HandleTypeDef hi2c1;
