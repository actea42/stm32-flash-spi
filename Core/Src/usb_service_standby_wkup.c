// usb_service_standby_wkup.c (corrected)
// - Pure binary streaming for GETLOG (no banners)
// - ZLP at end if total bytes multiple of 64
// - No reliance on TX-complete callbacks

#include "main.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "lfs.h"
#include "cdc_cmd.h"
#include "rtc_provision.h"
#include "rtc.h"
#include "usb_service_sm.h"
#include <string.h>
#include <stdio.h>

extern lfs_t lfs;
extern struct lfs_config lfs_cfg;
extern RTC_HandleTypeDef hrtc;
extern USBD_HandleTypeDef hUsbDeviceFS;

#ifndef USB_WKUP_PIN_POLARITY
#define USB_WKUP_PIN_POLARITY PWR_WAKEUP_PIN4_HIGH // PA2 rising
#endif
#ifndef USB_DETECT_GPIO
#define USB_DETECT_GPIO GPIOA
#endif
#ifndef USB_DETECT_PIN
#define USB_DETECT_PIN GPIO_PIN_2
#endif

static inline uint8_t USB_Detected(void)
{ return HAL_GPIO_ReadPin(USB_DETECT_GPIO, USB_DETECT_PIN) == GPIO_PIN_SET; }

// Wait until VBUS equals want_high and remains stable for stable_ms,
// bounded by overall timeout. Returns 1 if stable state reached, 0 on timeout.
static uint8_t WaitForVBUS(uint8_t want_high, uint32_t stable_ms, uint32_t overall_timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t last = USB_Detected();
    uint32_t stable_t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < overall_timeout_ms) {
        uint8_t now = USB_Detected();
        if (now != last) { last = now; stable_t0 = HAL_GetTick(); }
        if (now == want_high && (HAL_GetTick() - stable_t0) >= stable_ms) return 1;
        HAL_Delay(2);
    }
    return 0;
}

// Short text writes (prompts) – blocking but lightweight
static inline int USB_TxBlockingShort(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms) {
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc && hcdc->TxState == 0) {
            USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t*)data, len);
            if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK) return 0;
        }
        HAL_Delay(1);
    }
    return -1;
}
static inline void USB_Write(const char *s)
{ if (s) (void)USB_TxBlockingShort((const uint8_t*)s, (uint16_t)strlen(s), 250); }

// Packet-level blocking transmit with completion wait; supports ZLP (len==0)
static int USB_TxPacketBlocking(const uint8_t *data,
                                uint16_t len,
                                uint32_t start_timeout_ms,
                                uint32_t complete_timeout_ms)
{
    static uint8_t zlp_dummy; // used when len==0
    uint32_t t0 = HAL_GetTick();
    // Wait for ready
    while ((HAL_GetTick() - t0) < start_timeout_ms) {
        USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
        if (hcdc && hcdc->TxState == 0) {
            const uint8_t *ptr = (len == 0 && data == NULL) ? &zlp_dummy : data;
            USBD_CDC_SetTxBuffer(&hUsbDeviceFS, (uint8_t*)ptr, len);
            if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) == USBD_OK) {
                // Wait until completion
                uint32_t tw = HAL_GetTick();
                while (hcdc->TxState != 0 && (HAL_GetTick() - tw) < complete_timeout_ms) {
                    HAL_Delay(1);
                }
                if (hcdc->TxState == 0) return 0;
                return -2;
            }
        }
        HAL_Delay(1);
    }
    return -1;
}

void CMD_EraseLog(void)
{
    if (LFS_W25Q64_Mount(&lfs, &lfs_cfg) != 0) {
        LFS_W25Q64_FormatAndMount(&lfs, &lfs_cfg);
    }
    int rc = lfs_remove(&lfs, "wake.bin");
    LFS_W25Q64_Unmount(&lfs);
    USB_Write(rc == 0 ? "OK wake.bin erased\r\n" : "ERR erase failed\r\n");
}

static void stream_file_filtered(uint32_t since, uint32_t a, uint32_t b,
                                 bool use_since, bool use_between)
{
    (void)since; (void)a; (void)b; (void)use_since; (void)use_between;

    if (LFS_W25Q64_Mount(&lfs, &lfs_cfg) != 0) { USB_Write("ERR mount\r\n"); return; }

    lfs_file_t f; uint8_t buf[512];
    uint32_t usb_total_sent = 0;
    if (lfs_file_open(&lfs, &f, "wake.bin", LFS_O_RDONLY) >= 0) {
        (void)lfs_file_seek(&lfs, &f, 0, LFS_SEEK_SET);
        lfs_ssize_t r;
        while ((r = lfs_file_read(&lfs, &f, buf, sizeof buf)) > 0) {
            size_t off = 0;
            while (off < (size_t)r) {
                size_t chunk = ((r - off) > 64) ? 64 : (r - off);
                int rc = USB_TxPacketBlocking(buf + off, (uint16_t)chunk,
                                              /*start*/5000, /*complete*/2000);
                if (rc == 0) { off += chunk; usb_total_sent += chunk; }
                else { HAL_Delay(5); }
            }
        }
        lfs_file_close(&lfs, &f);
        // If last packet was exactly 64B, send a ZLP to terminate cleanly
        if ((usb_total_sent & 63) == 0) {
            (void)USB_TxPacketBlocking(NULL, 0, 2000, 2000);
        }
    } else {
        USB_Write("ERR open wake.bin\r\n");
    }
    LFS_W25Q64_Unmount(&lfs);
}

void CMD_GetLog_All(void)      { stream_file_filtered(0,0,0,false,false); }
void CMD_GetLog_Since(uint32_t s){ stream_file_filtered(s,0,0,true,false); }
void CMD_GetLog_Between(uint32_t a, uint32_t b){ stream_file_filtered(0,a,b,false,true); }

int CDC_BuildTimeStatus(char *buf, int buflen)
{
    if (!buf || buflen <= 0) return -1;
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    uint32_t startE=0, endE=0;
    int hasStart = (RTC_GetStartEpoch(&startE) == 0);
    int hasEnd = (RTC_GetEndEpoch(&endE) == 0);
    uint32_t ivl = RTC_GetLoggingInterval();
    uint32_t fs_used = 0, fs_total = 0, fs_full = 0;
    if (LFS_W25Q64_Mount(&lfs, &lfs_cfg) == 0) {
        lfs_ssize_t u = lfs_fs_size(&lfs);
        fs_total = lfs_cfg.block_count;
        fs_used = (u > 0) ? (uint32_t)u : 0;
        fs_full = (fs_total && fs_used >= fs_total - 2) ? 1 : 0; // 2-block reserve
        LFS_W25Q64_Unmount(&lfs);
    }
    return snprintf(buf, buflen,
        "time=%04d-%02d-%02d %02d:%02d:%02d provisioned=%d "
        "start=%s(%lu) end=%s(%lu) interval=%lu fs_used=%lu fs_total=%lu full=%u\r\n",
        2000 + d.Year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds,
        RTC_IsProvisioned(),
        hasStart ? "set" : "none", hasStart ? (unsigned long)startE : 0ul,
        hasEnd ? "set" : "none", hasEnd ? (unsigned long)endE : 0ul,
        (unsigned long)ivl,
        (unsigned long)fs_used, (unsigned long)fs_total, (unsigned)fs_full);
}

void Standby_ArmUSBWake_AndEnter(void)
{
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_2);
    HAL_PWREx_EnableGPIOPullUp   (PWR_GPIO_A, PWR_GPIO_BIT_4);
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_5);
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_7);
    HAL_PWREx_EnableGPIOPullDown (PWR_GPIO_A, PWR_GPIO_BIT_6);
    HAL_PWREx_EnablePullUpPullDownConfig();
#ifdef PWR_WAKEUP_PIN1
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN1);
#endif
#ifdef PWR_WAKEUP_PIN2
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN2);
#endif
#ifdef PWR_WAKEUP_PIN3
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN3);
#endif
#ifdef PWR_WAKEUP_PIN5
    HAL_PWR_DisableWakeUpPin(PWR_WAKEUP_PIN5);
#endif
    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    HAL_PWR_EnableWakeUpPin(USB_WKUP_PIN_POLARITY);
    HAL_PWREx_DisableSRAM2ContentRetention();
    HAL_PWR_EnterSTANDBYMode();
}

extern void SystemClock_Config_USBFast48(void);

void USB_Service_UploadWakeLog(void)
{
    if (!WaitForVBUS(1, 20, 1000)) return;

    // Bring up USB state machine
    USB_SM_Start();
    const uint32_t ENUM_MAX_MS = 60000;
    uint32_t t0 = HAL_GetTick();
    while (((HAL_GetTick() - t0) < ENUM_MAX_MS) && (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)) {
        USB_SM_RunStep();
        HAL_Delay(5);
    }
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
        USB_SM_Stop();
        USBD_Stop(&hUsbDeviceFS);
        USBD_DeInit(&hUsbDeviceFS);
        return;
    }
    while (USB_SM_IsActive() && (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) && USB_Detected()) {
        USB_SM_RunStep();
        HAL_Delay(1);
    }
    USB_SM_Stop();
    USBD_Stop(&hUsbDeviceFS);
    USBD_DeInit(&hUsbDeviceFS);
}

void StandbyUSB_BootPath(void)
{
    if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) != RESET) {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    }
    if (__HAL_PWR_GET_FLAG(PWR_FLAG_WU) != RESET) {
        __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
        if (USB_Detected()) {
            if (!WaitForVBUS(1, 20, 1000)) return;
            W25Q64_ReleaseFromDeepPowerDown();
            USB_Service_UploadWakeLog();
            W25Q64_EnterDeepPowerDown();
            HAL_Delay(5);
            (void)WaitForVBUS(0, /*stable_ms=*/300, /*overall_timeout_ms=*/5000);
            Standby_ArmUSBWake_AndEnter();
        }
    }
}
