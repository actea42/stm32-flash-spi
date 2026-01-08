/* usbd_cdc_if_fixed2.c
 * CDC interface with robust RX line handling:
 *  - Accumulate bytes, treat CR, LF, or CRLF as end-of-line (post only once for CRLF).
 *  - Ignore empty lines.
 *  - Proper TX-complete callback to notify application.
 */
#include "usbd_cdc_if.h"
#include <string.h>
#include "usb_service_sm.h"

extern USBD_HandleTypeDef hUsbDeviceFS;
static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS
};

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    return (USBD_OK);
}

static int8_t CDC_DeInit_FS(void)
{
    return (USBD_OK);
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
    (void)cmd; (void)pbuf; (void)length; return (USBD_OK);
}

/* -------- RX line collector with CRLF coalescing -------- */
#define RX_LINE_MAX 256
static char     s_linebuf[RX_LINE_MAX];
static uint16_t s_linepos   = 0;
static uint8_t  s_cr_pending = 0;   /* remember a CR to suppress the following LF */

static inline void rx_reset(void) { s_linepos = 0; }

static void rx_finish_and_post(void)
{
    /* Trim trailing spaces/tabs */
    while (s_linepos && (s_linebuf[s_linepos - 1] == ' ' || s_linebuf[s_linepos - 1] == '\t'))
        s_linepos--;

    if (s_linepos == 0) {
        /* Empty line: ignore */
        return;
    }

    s_linebuf[s_linepos] = '\0';
    USB_SM_PostCmdLine(s_linebuf);
    rx_reset();
}

static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
    for (uint32_t i = 0; i < *Len; ++i) {
        uint8_t b = Buf[i];
        if (b == '\r') {
            s_cr_pending = 1;    /* mark CR so next LF is ignored */
            rx_finish_and_post();
        } else if (b == '\n') {
            if (s_cr_pending) {
                s_cr_pending = 0; /* ignore LF that follows CR */
            } else {
                rx_finish_and_post();
            }
        } else {
            s_cr_pending = 0;    /* any non-CR resets the CR pending state */
            if (s_linepos < RX_LINE_MAX - 1) s_linebuf[s_linepos++] = (char)b;
            else rx_reset();
        }
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (USBD_OK);
}

/* -------- TX helpers -------- */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    if (hcdc == NULL) return USBD_FAIL;
    if (hcdc->TxState != 0) return USBD_BUSY;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

void CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf; (void)Len; (void)epnum;
    extern void USB_CDC_TxCplt(void);
    USB_CDC_TxCplt();
}

void CDC_TransmitCpltCallback(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
    (void)Buf; (void)Len; (void)epnum;
    extern void USB_CDC_TxCplt(void);
    USB_CDC_TxCplt();
}

