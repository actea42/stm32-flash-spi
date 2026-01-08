/* cdc_cmd.c (corrected)
 * - Robust command parsing (as in your file)
 * - Reliable short replies using blocking transmit (poll TxState)
 */
#include "cdc_cmd.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "rtc_provision.h"
#include "rtc.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "usb_service_sm.h"
#include "main.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

static int CDC_WriteBlocking(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
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

static void USB_Write(const char* s)
{
    if (!s) return;
    (void)CDC_WriteBlocking((const uint8_t*)s, (uint16_t)strlen(s), 250);
}

static inline void LED_Pulse(uint32_t ms)
{ HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); HAL_Delay(ms); HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); }

static bool parse_epoch_or_iso(const char *arg, uint32_t *out_epoch)
{
    if (!arg || !out_epoch) return false;
    if (strncmp(arg, "epoch=", 6) == 0) { *out_epoch = (uint32_t)strtoul(arg+6, NULL, 10); return true; }
    if (strncmp(arg, "iso=", 4) == 0) { return (rtc_parse_iso8601_to_epoch(arg+4, out_epoch) == 0); }
    return false;
}

static void on_accept(void) { LED_Pulse(60); }

void CDC_HandleLine(const char *line)
{
    if (!line) return;
    // Copy and trim trailing CR/LF
    char buf[200];
    size_t n = strnlen(line, sizeof buf - 1);
    while (n && (line[n-1] == '\r' || line[n-1] == '\n')) n--;
    memcpy(buf, line, n); buf[n] = 0;

    // Trim leading whitespace
    char *p = buf; while (*p == ' ' || *p == '\t') p++;
    if (!*p) return; // empty line -> ignore

    // Split into cmd + arg, skipping extra spaces
    char *arg = NULL;
    for (char *s = p; *s; ++s) {
        if (*s == ' ' || *s == '\t') { *s = '\0'; arg = s + 1; while (*arg == ' ' || *arg == '\t') arg++; break; }
    }
    char *cmd = p;

    if (strcasecmp(cmd, "HELP") == 0) {
        USB_Write(
            "Commands:\r\n"
            " SETTIME epoch=<sec> | iso=YYYY-MM-DDTHH:MM:SSZ\r\n"
            " STARTLOG epoch=<sec> | iso=...\r\n"
            " ENDLOG   epoch=<sec> | iso=...\r\n"
            " STOPLOG\r\n"
            " SETINTERVAL <sec>\r\n"
            " ERASELOG\r\n"
            " GETLOG [SINCE=<sec>] | GETLOG BETWEEN=<a>,<b>\r\n"
            " STATUS\r\n"
            " QUIT\r\n"
        );
        on_accept();
        return;
    }

    if (strcasecmp(cmd, "SETTIME") == 0) {
        if (!arg) { USB_Write("ERR missing arg\r\n"); return; }
        if (strncmp(arg, "epoch=", 6) == 0) {
            uint32_t e = (uint32_t)strtoul(arg+6, NULL, 10);
            RTC_SetFromEpoch(e); RTC_MarkProvisioned(); USB_Write("OK TIME SET\r\n"); on_accept();
        } else if (strncmp(arg, "iso=", 4) == 0) {
            if (RTC_SetFromISO8601(arg+4) == 0) { RTC_MarkProvisioned(); USB_Write("OK TIME SET\r\n"); on_accept(); }
            else USB_Write("ERR bad ISO time\r\n");
        } else USB_Write("ERR arg\r\n");
        return;
    }

    if (strcasecmp(cmd, "STARTLOG") == 0) {
        if (!arg) { USB_Write("ERR missing arg\r\n"); return; }
        uint32_t epoch; if (parse_epoch_or_iso(arg, &epoch)) { RTC_SetStartEpoch(epoch); USB_Write("OK STARTLOG set\r\n"); on_accept(); }
        else USB_Write("ERR bad time\r\n");
        return;
    }

    if (strcasecmp(cmd, "ENDLOG") == 0) {
        if (!arg) { USB_Write("ERR missing arg\r\n"); return; }
        uint32_t epoch; if (parse_epoch_or_iso(arg, &epoch)) { RTC_SetEndEpoch(epoch); USB_Write("OK ENDLOG set\r\n"); on_accept(); }
        else USB_Write("ERR bad time\r\n");
        return;
    }

    if (strcasecmp(cmd, "STOPLOG") == 0) {
        RTC_ClearStartEpoch(); RTC_ClearEndEpoch(); USB_Write("OK logging disabled\r\n"); on_accept(); return;
    }

    if (strcasecmp(cmd, "SETINTERVAL") == 0 || strcasecmp(cmd, "INTERVAL") == 0) {
        if (!arg) { USB_Write("ERR missing seconds\r\n"); return; }
        uint32_t sec = (uint32_t)strtoul(arg, NULL, 10);
        RTC_SetLoggingInterval(sec); USB_Write("OK INTERVAL set\r\n"); on_accept(); return;
    }

    if (strcasecmp(cmd, "ERASELOG") == 0) { CMD_EraseLog(); on_accept(); return; }

    if (strcasecmp(cmd, "GETLOG") == 0) {
        if (!arg || !*arg) { CMD_GetLog_All(); on_accept(); return; }
        if (strncasecmp(arg, "SINCE=", 6) == 0) { uint32_t s = (uint32_t)strtoul(arg+6, NULL, 10); CMD_GetLog_Since(s); on_accept(); return; }
        if (strncasecmp(arg, "BETWEEN=", 8) == 0) {
            uint32_t a=0,b=0; const char *p = arg+8; a = (uint32_t)strtoul(p, (char**)&p, 10);
            if (*p == ',' || *p == ';') { p++; b = (uint32_t)strtoul(p, NULL, 10); }
            if (a && b && a <= b) { CMD_GetLog_Between(a,b); on_accept(); }
            else USB_Write("ERR bad range\r\n");
            return;
        }
        CMD_GetLog_All(); on_accept(); return;
    }

    if (strcasecmp(cmd, "STATUS") == 0) {
        char out[200]; int n = CDC_BuildTimeStatus(out, sizeof out);
        if (n > 0) (void)CDC_WriteBlocking((const uint8_t*)out, (uint16_t)n, 250);
        on_accept();
        return;
    }

    if (strcasecmp(cmd, "QUIT") == 0) { USB_SM_Stop(); USB_Write("OK bye\r\n"); on_accept(); return; }

    USB_Write("ERR unknown (type HELP)\r\n");
}
