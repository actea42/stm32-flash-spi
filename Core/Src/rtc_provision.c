#include "rtc_provision.h"
#include "rtc.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

extern RTC_HandleTypeDef hrtc;

/* ---- Date helpers (same base as your existing code) ---- */
static int is_leap(int y) {
    int year = 2000 + y;
    return ((year%4==0) && (year%100!=0)) || (year%400==0);
}
static int dim(int y, int m) {
    static const int D[12]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(m==2 && is_leap(y)) return 29;
    return D[m-1];
}

int RTC_IsProvisioned(void) {
    return (HAL_RTCEx_BKUPRead(&hrtc, RTC_PROV_BKP_DR) == RTC_PROV_MAGIC);
}
void RTC_MarkProvisioned(void) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_PROV_BKP_DR, RTC_PROV_MAGIC);
}
void RTC_ClearProvisioned(void) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_PROV_BKP_DR, 0);
}

static void epoch_to_calendar(uint32_t e, RTC_DateTypeDef* d, RTC_TimeTypeDef* t) {
    uint32_t days = e / 86400u; uint32_t rem = e % 86400u;
    t->Hours = rem / 3600u; rem %= 3600u; t->Minutes = rem / 60u; t->Seconds = rem % 60u;

    int y=0;
    while (1) {
        int yd=365 + is_leap(y);
        if (days >= (uint32_t)yd) { days -= yd; y++; }
        else break;
    }
    int m=1;
    while (1) {
        int md = dim(y,m);
        if (days >= (uint32_t)md) { days -= md; m++; }
        else break;
    }
    d->Year = y; d->Month = m; d->Date = (int)days + 1; d->WeekDay = RTC_WEEKDAY_MONDAY;
}
void RTC_SetFromEpoch(uint32_t epoch) {
    RTC_DateTypeDef d; RTC_TimeTypeDef t;
    epoch_to_calendar(epoch, &d, &t);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
}

/* -- ISO8601 (UTC 'Z') parsing to RTC -- */
static int two (const char**s){ int v=0; for(int i=0;i<2;i++){ if(!isdigit((unsigned char)(*s)[0])) return -1; v=v*10+((*s)[0]-'0'); (*s)++; } return v; }
static int four(const char**s){ int v=0; for(int i=0;i<4;i++){ if(!isdigit((unsigned char)(*s)[0])) return -1; v=v*10+((*s)[0]-'0'); (*s)++; } return v; }

int RTC_SetFromISO8601(const char* iso) {
    const char* s = iso; RTC_DateTypeDef d={0}; RTC_TimeTypeDef t={0};
    int year = four(&s); if(year<2000 || year>2099) return -1; if(*s!='-') return -1; s++;
    int mon  = two(&s);  if(mon<1 || mon>12)      return -1; if(*s!='-') return -1; s++;
    int day  = two(&s);  if(day<1 || day>31)      return -1;
    if(*s!='T' && *s!=' ') return -1; s++;
    int hh   = two(&s);  if(hh<0 || hh>23)        return -1; if(*s!=':') return -1; s++;
    int mm   = two(&s);  if(mm<0 || mm>59)        return -1; if(*s!=':') return -1; s++;
    int ss   = two(&s);  if(ss<0 || ss>59)        return -1;
    if(*s=='Z') s++;

    d.Year = year - 2000; d.Month = mon; d.Date = day;
    t.Hours = hh; t.Minutes = mm; t.Seconds = ss;

    if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) return -1;
    if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return -1;
    return 0;
}

/* ---- START epoch ---- */
void RTC_SetStartEpoch(uint32_t epoch) {
    uint16_t lo = (uint16_t)(epoch & 0xFFFFu);
    uint16_t hi = (uint16_t)((epoch >> 16) & 0xFFFFu);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_EPOCH_LO, lo);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_EPOCH_HI, hi);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_MAGIC_DR, RTC_START_MAGIC);
}
int RTC_GetStartEpoch(uint32_t* epoch_out) {
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_START_MAGIC_DR) != RTC_START_MAGIC) return -1;
    uint16_t lo = HAL_RTCEx_BKUPRead(&hrtc, RTC_START_EPOCH_LO);
    uint16_t hi = HAL_RTCEx_BKUPRead(&hrtc, RTC_START_EPOCH_HI);
    *epoch_out = ((uint32_t)hi << 16) | lo;
    return 0;
}
void RTC_ClearStartEpoch(void) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_MAGIC_DR, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_EPOCH_LO, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_START_EPOCH_HI, 0);
}

/* ---- END epoch ---- */
void RTC_SetEndEpoch(uint32_t epoch) {
    uint16_t lo = (uint16_t)(epoch & 0xFFFFu);
    uint16_t hi = (uint16_t)((epoch >> 16) & 0xFFFFu);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_EPOCH_LO, lo);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_EPOCH_HI, hi);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_MAGIC_DR, RTC_END_MAGIC);
}
int RTC_GetEndEpoch(uint32_t* epoch_out) {
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_END_MAGIC_DR) != RTC_END_MAGIC) return -1;
    uint16_t lo = HAL_RTCEx_BKUPRead(&hrtc, RTC_END_EPOCH_LO);
    uint16_t hi = HAL_RTCEx_BKUPRead(&hrtc, RTC_END_EPOCH_HI);
    *epoch_out = ((uint32_t)hi << 16) | lo;
    return 0;
}
void RTC_ClearEndEpoch(void) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_MAGIC_DR, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_EPOCH_LO, 0);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_END_EPOCH_HI, 0);
}

/* ---- Interval ---- */
void RTC_SetLoggingInterval(uint32_t sec) {
    if (sec < 5) sec = 5;
    if (sec > 86400) sec = 86400; // 24h
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_INTERVAL_DR, sec);
}
uint32_t RTC_GetLoggingInterval(void) {
    uint32_t v = HAL_RTCEx_BKUPRead(&hrtc, RTC_INTERVAL_DR);
    if (v == 0) v = 30; // default
    return v;
}

/* ---- Should log now? ---- */
int RTC_ShouldLogNow(void) {
    uint32_t startE=0;
    if (RTC_GetStartEpoch(&startE) != 0) return 1; // no start => log immediately
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    uint32_t now = rtc_datetime_to_epoch(&d, &t);
    return (now >= startE) ? 1 : 0;
}

/* ---- Status ---- */
int RTC_BuildStatus(char* out, size_t maxlen) {
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);

    uint32_t startE=0; int hasStart = (RTC_GetStartEpoch(&startE) == 0);
    RTC_DateTypeDef sd; RTC_TimeTypeDef st;
    char startbuf[32];
    if (hasStart) { epoch_to_calendar(startE, &sd, &st);
        snprintf(startbuf, sizeof startbuf, "%04d-%02d-%02d %02d:%02d:%02d",
                 2000+sd.Year, sd.Month, sd.Date, st.Hours, st.Minutes, st.Seconds);
    } else snprintf(startbuf, sizeof startbuf, "none");

    uint32_t endE=0; int hasEnd = (RTC_GetEndEpoch(&endE) == 0);
    RTC_DateTypeDef ed; RTC_TimeTypeDef et;
    char endbuf[32];
    if (hasEnd) { epoch_to_calendar(endE, &ed, &et);
        snprintf(endbuf, sizeof endbuf, "%04d-%02d-%02d %02d:%02d:%02d",
                 2000+ed.Year, ed.Month, ed.Date, et.Hours, et.Minutes, et.Seconds);
    } else snprintf(endbuf, sizeof endbuf, "none");

    uint32_t interval = RTC_GetLoggingInterval();

    return snprintf(out, maxlen,
        "time=%04d-%02d-%02d %02d:%02d:%02d provisioned=%d start=%s end=%s interval=%lu\r\n",
        2000+d.Year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds,
        RTC_IsProvisioned(), startbuf, endbuf, (unsigned long)interval);
}

/* ---- ISO-8601 to epoch helper ---- */
static uint32_t calendar_to_epoch(int y2000, int mon, int day, int hh, int mm, int ss) {
    uint32_t days = 0;
    for (int y=0; y<y2000; ++y) days += 365 + is_leap(y);
    for (int m=1; m<mon; ++m)  days += dim(y2000, m);
    days += (uint32_t)(day - 1);
    return days*86400u + (uint32_t)hh*3600u + (uint32_t)mm*60u + (uint32_t)ss;
}

int rtc_parse_iso8601_to_epoch(const char *iso, uint32_t *epoch_out) {
    const char* s = iso;
    int year = four(&s); if(year<2000 || year>2099) return -1; if(*s!='-') return -1; s++;
    int mon  = two(&s);  if(mon<1 || mon>12)      return -1; if(*s!='-') return -1; s++;
    int day  = two(&s);  if(day<1 || day>31)      return -1;
    if(*s!='T' && *s!=' ') return -1; s++;
    int hh   = two(&s);  if(hh<0 || hh>23)        return -1; if(*s!=':') return -1; s++;
    int mm   = two(&s);  if(mm<0 || mm>59)        return -1; if(*s!=':') return -1; s++;
    int ss   = two(&s);  if(ss<0 || ss>59)        return -1;
    if(*s=='Z') s++;

    *epoch_out = calendar_to_epoch(year-2000, mon, day, hh, mm, ss);
    return 0;
}
