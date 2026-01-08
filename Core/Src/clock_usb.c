#include "main.h"
#include "stm32l4xx_hal.h"

void SystemClock_Config_USBFast48(void)
{
    /* Ensure USB analog domain is powered */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddUSB();
    HAL_PWREx_DisableLowPowerRunMode();

    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI |
                         RCC_OSCILLATORTYPE_HSI48 |
                         RCC_OSCILLATORTYPE_LSE;
    osc.MSIState = RCC_MSI_ON;
    osc.MSICalibrationValue = 0;
    osc.MSIClockRange = RCC_MSIRANGE_6; // 4 MHz MSI for PLL input
    osc.HSI48State = RCC_HSI48_ON;
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_MSI;
    osc.PLL.PLLM = 1;
    osc.PLL.PLLN = 24; // 4*24=96
    osc.PLL.PLLR = RCC_PLLR_DIV2; // 96/2=48 MHz SYSCLK
    osc.PLL.PLLQ = RCC_PLLQ_DIV2;
    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2);

    pclk.PeriphClockSelection = RCC_PERIPHCLK_USB |
                                RCC_PERIPHCLK_RTC |
                                RCC_PERIPHCLK_I2C1;
    pclk.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    pclk.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    pclk.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&pclk);

    __HAL_RCC_CRS_CLK_ENABLE();

    /* Initial CRS sync to LSE: use 32.768 kHz reload for proper HSI48 trimming */
    RCC_CRSInitTypeDef crs = {0};
    crs.Prescaler = RCC_CRS_SYNC_DIV1;
    crs.Source = RCC_CRS_SYNC_SOURCE_LSE;        // LSE as initial sync source
    crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
    crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 32768);
    crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
    crs.HSI48CalibrationValue = 0x20;
    HAL_RCCEx_CRSConfig(&crs);
}
