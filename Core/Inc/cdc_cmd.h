#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call this for each complete CR/LF-terminated line received via USB CDC */
void CDC_HandleLine(const char *line);

/* Status helpers you already use elsewhere */
int  CDC_BuildTimeStatus(char *buf, int buflen);
int  CDC_TimeWasSet(void);

#ifdef __cplusplus
}
#endif
