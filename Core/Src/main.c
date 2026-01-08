// main.c
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "rtc.h"
#include "w25q64.h"
#include "lfs_w25q64.h"
#include "lfs.h"
#include "i2c_on_demand.h"
#include "sht4x_ll.h"
#include "lowpower.h"
#include "usb_device.h"
#include "rtc_provision.h"

void StandbyUSB_BootPath(void);
void Standby_ArmUSBWake_AndEnter(void);
static void Configure_PA2_As_WakeupPin4(bool);

// --- Persistent LED flag in RTC backup domain ---
#define LED_FIRST_LOG_MAGIC   ((uint32_t)0x1ED0)   // any non-zero magic
#define LED_FIRST_LOG_REG     RTC_BKP_DR0          // choose DR0..DR31 per your MCU

lfs_t lfs;
lfs_file_t f;
struct lfs_config lfs_cfg;
RTC_HandleTypeDef hrtc;
SPI_HandleTypeDef hspi1;

typedef struct __attribute__((packed)) {
    uint32_t epoch;
    int16_t  t_x100;
    uint16_t rh_x100;
} logrec_t;

static void SystemClock_Config_Base_LSE_MSI2MHz(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_RTC_Init_LSE(void);
static void Enter_LowPowerRun2MHz(void);
static void Exit_LowPowerRun(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config_Base_LSE_MSI2MHz();
    MX_GPIO_Init();
    MX_RTC_Init_LSE();
    MX_SPI1_Init();

    // Allow writing to the RTC backup registers
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    // Read persistent flag: has the "first log LED" already run?
    uint32_t led_flag = HAL_RTCEx_BKUPRead(&hrtc, LED_FIRST_LOG_REG);

    // LED ON at power-up only if first log hasn't occurred yet
    if (led_flag != LED_FIRST_LOG_MAGIC) {
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    }

    HAL_DBGMCU_DisableDBGSleepMode();
    HAL_DBGMCU_DisableDBGStopMode();
    HAL_DBGMCU_DisableDBGStandbyMode();

    W25Q64_Bind(&hspi1, GPIOA, GPIO_PIN_4);
    LFS_W25Q64_InitConfig(&lfs_cfg);

    StandbyUSB_BootPath();
    Enter_LowPowerRun2MHz();

    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    uint32_t now = rtc_datetime_to_epoch(&d, &t);
    W25Q64_ReleaseFromDeepPowerDown();

    static uint8_t lfs_read_buf [LFS_W25Q128_CACHE_SIZE];
    static uint8_t lfs_prog_buf [LFS_W25Q128_CACHE_SIZE];
    static uint8_t lfs_lookahead [LFS_W25Q128_LOOKAHEAD];

    lfs_cfg.read_buffer      = lfs_read_buf;
    lfs_cfg.prog_buffer      = lfs_prog_buf;
    lfs_cfg.lookahead_buffer = lfs_lookahead;   // FIXED: use dedicated lookahead buffer

    if (LFS_W25Q64_Mount(&lfs, &lfs_cfg) != 0) {
        LFS_W25Q64_FormatAndMount(&lfs, &lfs_cfg);
    }

    // --- Filesystem near-full detection (centralized helper) ---
    // Keep 2 blocks reserved for metadata/erase safety
    uint8_t fs_full = FS_IsNearFull(2);

    // --- Read sensor and append a binary record ---
    I2C1_OnDemand_Init();
    sht4x_reading_t r = SHT4x_ReadSingleShot(SHT4X_CMD_MED_PREC);
    I2C1_OnDemand_DeInit();

    logrec_t rec = {
        .epoch  = now,
        .t_x100 = r.ok ? (int16_t)lroundf(r.temp_c*100.0f) : INT16_MAX,
        .rh_x100= r.ok ? (uint16_t)lroundf(r.rh*100.0f)    : UINT16_MAX
    };

    if (lfs_file_open(&lfs, &f, "wake.bin", LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND) == 0) {
        (void)lfs_file_write(&lfs, &f, &rec, sizeof(rec));
        lfs_file_close(&lfs, &f);
    }

    // If this was the first-ever log, mark it done and turn LED OFF
    if (led_flag != LED_FIRST_LOG_MAGIC) {
        HAL_RTCEx_BKUPWrite(&hrtc, LED_FIRST_LOG_REG, LED_FIRST_LOG_MAGIC);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    }

    LFS_W25Q64_Unmount(&lfs);
    W25Q64_EnterDeepPowerDown();
    HAL_Delay(5);

    // Quick VBUS detect: if present, offer USB service window
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_2;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLDOWN;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_Delay(5);
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2) == GPIO_PIN_SET) {
        Exit_LowPowerRun();
        W25Q64_ReleaseFromDeepPowerDown();
        USB_Service_UploadWakeLog();
        W25Q64_EnterDeepPowerDown();
        HAL_Delay(5);
    }

    // --- ENDLOG stop check ---
    uint32_t endE = 0;
    int hasEnd = (RTC_GetEndEpoch(&endE) == 0);
    uint8_t end_reached = (hasEnd && now >= endE) ? 1 : 0;

    // --- Infinite Standby policy: if memory full OR ENDLOG reached ---
    if (fs_full || end_reached) {
        SPI1_EnterLowPower();
        Pins_StandbyQuiescent_Config();
        Configure_PA2_As_WakeupPin4(true);     // VBUS rising
        Standby_ArmUSBWake_AndEnter();         // WKUP4 only, RTC wake disabled inside
        while (1) { /* sleep until USB */ }
    }

    // --- Otherwise continue with normal interval scheduling ---
    uint32_t interval = RTC_GetLoggingInterval();
    if (hasEnd) {
        uint32_t remain = endE - now;
        if (remain < interval) interval = remain;
    }

    SPI1_EnterLowPower();
    Pins_StandbyQuiescent_Config();

    RTC_ScheduleNextAlarm_AndStandby(interval);
    while (1) { }
}

static void SystemClock_Config_Base_LSE_MSI2MHz(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef pclk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI | RCC_OSCILLATORTYPE_LSE;
    osc.MSIState = RCC_MSI_ON;
    osc.MSICalibrationValue = 0;
    osc.MSIClockRange = RCC_MSIRANGE_5; // 2 MHz
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&osc);
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
    pclk.PeriphClockSelection = RCC_PERIPHCLK_RTC | RCC_PERIPHCLK_I2C1;
    pclk.RTCClockSelection = RCC_RTCCLKSOURCE_LSE; // RTC from LSE
    pclk.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&pclk);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin = CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);
}

static void MX_RTC_Init_LSE(void)
{
    __HAL_RCC_RTC_ENABLE();
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127; // 1 Hz base with LSE
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity= RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    HAL_RTC_Init(&hrtc);
}

static void Enter_LowPowerRun2MHz(void) { HAL_PWREx_EnableLowPowerRunMode(); }
static void Exit_LowPowerRun(void)      { HAL_PWREx_DisableLowPowerRunMode(); }

static void Configure_PA2_As_WakeupPin4(bool active_high)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_2; g.Mode = GPIO_MODE_INPUT; g.Pull = active_high ? GPIO_PULLDOWN : GPIO_PULLUP; g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN4);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    if (active_high) HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN4_HIGH);
    else HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN4_LOW);
}

static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7; // PA5=SCK, PA6=MISO, PA7=MOSI
    g.Mode = GPIO_MODE_AF_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_4; g.Mode = GPIO_MODE_OUTPUT_PP; g.Pull = GPIO_NOPULL; g.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // PA4=CS
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&hspi1);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { (void)file; (void)line; }
#endif
