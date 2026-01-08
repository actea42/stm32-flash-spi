#include "rtc.h"

extern RTC_HandleTypeDef hrtc;

static int is_leap(int y)
{ int year = 2000 + y; return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0); }
static int dim(int y, int m)
{ static const int D[12] = {31,28,31,30,31,30,31,31,30,31,30,31}; if (m==2 && is_leap(y)) return 29; return D[m-1]; }

uint32_t rtc_datetime_to_epoch(const RTC_DateTypeDef* d, const RTC_TimeTypeDef* t)
{
    uint32_t days = 0;
    for (int yy=0; yy<d->Year; ++yy) days += 365 + is_leap(yy);
    for (int mm=1; mm<d->Month; ++mm) days += dim(d->Year, mm);
    days += (d->Date - 1);
    return days*86400u + t->Hours*3600u + t->Minutes*60u + t->Seconds;
}

void RTC_ScheduleNextAlarm_AndStandby(uint32_t seconds_from_now)
{
    HAL_RTC_GetTime(&hrtc, &(RTC_TimeTypeDef){0}, RTC_FORMAT_BIN);
    RTC_TimeTypeDef now_t; RTC_DateTypeDef now_d;
    HAL_RTC_GetTime(&hrtc, &now_t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &now_d, RTC_FORMAT_BIN);

    uint32_t now = rtc_datetime_to_epoch(&now_d, &now_t);
    uint32_t target = now + seconds_from_now;

    uint32_t days = target / 86400u; uint32_t rem = target % 86400u;
    RTC_TimeTypeDef at = {0}; RTC_DateTypeDef ad = {0};
    at.Hours = rem / 3600u; rem %= 3600u; at.Minutes = rem / 60u; at.Seconds = rem % 60u;
    int y=0; while (1) { int yd=365 + is_leap(y); if (days >= (uint32_t)yd) { days -= yd; y++; } else break; }
    int m=1; while (1) { int md=dim(y, m); if (days >= (uint32_t)md) { days -= md; m++; } else break; }
    ad.Year = y; ad.Month = m; ad.Date = (int)days + 1;

    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    RTC_AlarmTypeDef a = {0};
    a.Alarm = RTC_ALARM_A;
    a.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    a.AlarmDateWeekDay = ad.Date;
    a.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    a.AlarmMask = RTC_ALARMMASK_NONE;
    a.AlarmTime = at;
    HAL_RTC_SetAlarm_IT(&hrtc, &a, RTC_FORMAT_BIN);
    /* HAL helper: disables SRAM2 content retention in Standby */
    HAL_PWREx_DisableSRAM2ContentRetention();
    HAL_PWR_EnterSTANDBYMode();
}
