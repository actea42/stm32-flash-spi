// usb_service_sm.c (unchanged core; robust blocking TX helper inside)
#include "usb_service_sm.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "cdc_cmd.h"
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceFS;

// Robust transmit: retry while USBD_BUSY, bounded by timeout
static int USB_TxBlocking(const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (CDC_Transmit_FS((uint8_t*)data, len) == USBD_OK) {
            return 0;
        }
        HAL_Delay(1); // give USB ISR/LL time to progress
    }
    return -1; // timeout
}

static void USB_Write(const char *s) {
    if (!s) return;
    (void)USB_TxBlocking((const uint8_t*)s, (uint16_t)strlen(s), 250);
}

static usb_sm_state_t s_state = USB_SM_IDLE;
static volatile bool s_has_line = false;
static char s_line[200];

void USB_SM_Start(void) { s_state = USB_SM_INIT; }
void USB_SM_Stop(void)  { s_state = USB_SM_EXIT; }
bool USB_SM_IsActive(void){ return s_state != USB_SM_EXIT; }

void USB_SM_PostCmdLine(const char *line)
{
    if (!line) return;
    size_t n = strnlen(line, sizeof s_line - 1);
    memcpy(s_line, line, n); s_line[n] = 0;
    s_has_line = true;
}

extern void SystemClock_Config_USBFast48(void);

void USB_SM_RunStep(void)
{
    switch (s_state) {
    case USB_SM_IDLE:
        break;
    case USB_SM_INIT:
        // Fast 48 MHz + CRS initial sync on LSE
        SystemClock_Config_USBFast48(); // existing clock setup
        MX_USB_DEVICE_Init();           // init stack
        // Make sure device is started
        if (hUsbDeviceFS.dev_state == USBD_STATE_DEFAULT) {
            USBD_Start(&hUsbDeviceFS);
        }
        // Switch CRS to USB SOF once active to refine HSI48
        {
            RCC_CRSInitTypeDef crs = {0};
            crs.Prescaler = RCC_CRS_SYNC_DIV1;
            crs.Source = RCC_CRS_SYNC_SOURCE_USB; // use SOF now
            crs.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
            crs.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000, 1000);
            crs.ErrorLimitValue = RCC_CRS_ERRORLIMIT_DEFAULT;
            crs.HSI48CalibrationValue = 0x20;
            HAL_RCCEx_CRSConfig(&crs);
        }
        USB_Write("Ready. Type HELP for commands.\r\n");
        s_state = USB_SM_READY;
        break;
    case USB_SM_READY:
        s_state = USB_SM_RX_CMD;
        break;
    case USB_SM_RX_CMD:
        if (s_has_line) {
            s_has_line = false;
            CDC_HandleLine(s_line);
        }
        break;
    case USB_SM_EXIT:
        break;
    }
}
