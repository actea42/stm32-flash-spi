#include "lowpower.h"

void SPI1_EnterLowPower(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_ANALOG;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
}

void Pins_StandbyQuiescent_Config(void)
{
    HAL_PWREx_EnableGPIOPullUp   (PWR_GPIO_A, PWR_GPIO_BIT_4); // CS high
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_5); // SCK low
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_7); // MOSI low
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_6); // MISO low
    HAL_PWREx_EnablePullUpPullDownConfig();
}
