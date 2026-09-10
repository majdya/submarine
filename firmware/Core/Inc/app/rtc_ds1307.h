#ifndef APP_RTC_DS1307_H
#define APP_RTC_DS1307_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Plain calendar date/time, binary (not BCD) - what every caller outside
   this driver should work with. year is the full year (e.g. 2026). */
typedef struct {
  uint16_t year;
  uint8_t month;  /* 1-12 */
  uint8_t day;    /* 1-31 */
  uint8_t hour;   /* 0-23 (this driver always runs the DS1307 in 24h mode) */
  uint8_t min;    /* 0-59 */
  uint8_t sec;    /* 0-59 */
} RTC_DateTime_t;

/* Reads DS1307 register 0x00 (seconds, BCD - bit 7 is the clock-halt flag)
   over I2C3. Kept for callers that only need a cheap liveness/tick source. */
HAL_StatusTypeDef RTC_ReadSecondsRaw(uint8_t *out);

/* Reads just the clock-halt (CH) bit out of register 0x00. *out is 1 if
   the oscillator is halted (clock not running - dead/removed battery, or
   never set), 0 if it is running normally. */
HAL_StatusTypeDef RTC_IsHalted(uint8_t *out_halted);

/* Reads all 7 clock registers and converts them from BCD to binary. */
HAL_StatusTypeDef RTC_GetDateTime(RTC_DateTime_t *out);

/* Writes all 7 clock registers from binary to BCD. Always writes the
   seconds register's bit 7 (CH) as 0, so this also starts the oscillator
   if it was halted. Day-of-week is derived internally (Zeller's
   congruence) - callers never need to supply it. */
HAL_StatusTypeDef RTC_SetDateTime(const RTC_DateTime_t *dt);

/* Firmware build timestamp (from the compiler's __DATE__/__TIME__), used
   as the fallback value when the RTC has no better one of its own. */
RTC_DateTime_t RTC_GetBuildDateTime(void);

/* Call once at startup (after I2C3 is initialized). If the oscillator is
   halted, sets the clock from RTC_GetBuildDateTime() (which also clears
   the halt bit and starts it); if it is already running, leaves the
   battery-backed time alone. *out_was_halted (if non-NULL) reports which
   case happened, so the caller can report it. */
HAL_StatusTypeDef RTC_Init(uint8_t *out_was_halted);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTC_DS1307_H */
