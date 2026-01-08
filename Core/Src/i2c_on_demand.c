#include "i2c_on_demand.h"

I2C_HandleTypeDef hi2c1;

#ifndef I2C1_TIMING_VALUE
#warning "Using placeholder I2C timing; replace with CubeMX-generated TIMING for 2MHz base."
#define I2C1_TIMING_VALUE  0x00301D2B  // example; update via CubeMX
#endif

void I2C1_OnDemand_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;    // PA9=SCL, PA10=SDA
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_NOPULL;                // external pull-ups recommended
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOA, &g);

    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = I2C1_TIMING_VALUE; // <= update via CubeMX
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);

    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0);
}

void I2C1_OnDemand_DeInit(void)
{
    HAL_I2C_DeInit(&hi2c1);

    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);

    __HAL_RCC_I2C1_CLK_DISABLE();
}
