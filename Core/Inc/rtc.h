#pragma once
#include "main.h"


uint32_t rtc_datetime_to_epoch(const RTC_DateTypeDef* d, const RTC_TimeTypeDef* t);
void RTC_ScheduleNextAlarm_AndStandby(uint32_t seconds_from_now);
