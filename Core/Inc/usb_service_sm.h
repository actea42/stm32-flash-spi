#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    USB_SM_IDLE = 0,
    USB_SM_INIT,
    USB_SM_READY,
    USB_SM_RX_CMD,
    USB_SM_EXIT
} usb_sm_state_t;

void USB_SM_Start(void);
void USB_SM_Stop(void);
bool USB_SM_IsActive(void);
void USB_SM_PostCmdLine(const char *line);
void USB_SM_RunStep(void);

#ifdef __cplusplus
}
#endif
