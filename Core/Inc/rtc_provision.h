#pragma once
#include "main.h"
#include <stdint.h>
#include <stddef.h>

/* -------- Existing backup registers (from your project) -------- */
#define RTC_PROV_BKP_DR        RTC_BKP_DR0   // provisioned flag
#define RTC_PROV_MAGIC         0xA5A5

#define RTC_START_EPOCH_LO     RTC_BKP_DR1   // low 16 bits
#define RTC_START_EPOCH_HI     RTC_BKP_DR2   // high 16 bits
#define RTC_START_MAGIC_DR     RTC_BKP_DR3   // indicates start epoch stored
#define RTC_START_MAGIC        0x5A5A

/* -------- New backup registers we add -------- */
#define RTC_END_EPOCH_LO       RTC_BKP_DR4   // ENDLOG low 16 bits
#define RTC_END_EPOCH_HI       RTC_BKP_DR5   // ENDLOG high 16 bits
#define RTC_END_MAGIC_DR       RTC_BKP_DR6   // ENDLOG valid flag
#define RTC_END_MAGIC          0xE0E0

#define RTC_INTERVAL_DR        RTC_BKP_DR7   // logging interval (seconds)

/* Provisioning & time */
int  RTC_IsProvisioned(void);
void RTC_MarkProvisioned(void);
void RTC_ClearProvisioned(void);

void RTC_SetFromEpoch(uint32_t epoch);
int  RTC_SetFromISO8601(const char* iso);

/* Start epoch (deferred logging start) */
void RTC_SetStartEpoch(uint32_t epoch);
int  RTC_GetStartEpoch(uint32_t* epoch_out);
void RTC_ClearStartEpoch(void);

/* End epoch (stop logging at/after this time) */
void RTC_SetEndEpoch(uint32_t epoch);
int  RTC_GetEndEpoch(uint32_t* epoch_out);
void RTC_ClearEndEpoch(void);

/* Interval (seconds) */
void     RTC_SetLoggingInterval(uint32_t sec);
uint32_t RTC_GetLoggingInterval(void);

/* Helpers (status & eligibility) */
int  RTC_ShouldLogNow(void);
int  RTC_BuildStatus(char* out, size_t maxlen);

/* ISO-8601 parsing to epoch (used by CDC parser) */
int rtc_parse_iso8601_to_epoch(const char *iso, uint32_t *epoch_out);
