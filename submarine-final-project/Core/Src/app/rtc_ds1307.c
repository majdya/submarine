#include "rtc_ds1307.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define DS1307_I2C_ADDR (0x68 << 1)
#define DS1307_REG_SECONDS 0x00
#define DS1307_NUM_CLOCK_REGS 7 /* seconds..year, registers 0x00-0x06 */
#define DS1307_CH_BIT 0x80u     /* clock-halt bit, register 0x00 bit 7 */

static uint8_t bcd2bin(uint8_t val) { return (uint8_t)((val >> 4) * 10 + (val & 0x0F)); }
static uint8_t bin2bcd(uint8_t val) { return (uint8_t)(((val / 10) << 4) | (val % 10)); }

/* Zeller's congruence -> 0=Saturday..6=Friday, remapped to the DS1307's
   day-of-week register as 1=Sunday..7=Saturday. The DS1307 never uses
   this value for anything itself (it's just carried alongside the date),
   so any consistent 1-7 mapping is fine - this one matches common
   convention if anyone reads register 0x03 directly. */
static uint8_t WeekdayRegisterValue(uint16_t year, uint8_t month, uint8_t day) {
  int y = year;
  int m = month;
  if (m < 3) {
    m += 12;
    y -= 1;
  }
  int K = y % 100;
  int J = y / 100;
  int h = (day + (13 * (m + 1)) / 5 + K + K / 4 + J / 4 + 5 * J) % 7;
  /* h: 0=Saturday,1=Sunday,...,6=Friday. Map to 1=Sunday..7=Saturday. */
  uint8_t sunday_based = (uint8_t)((h + 1) % 7); /* 0=Saturday -> ... */
  return (uint8_t)(sunday_based == 0 ? 7 : sunday_based);
}

HAL_StatusTypeDef RTC_ReadSecondsRaw(uint8_t *out) {
  uint8_t reg = DS1307_REG_SECONDS;
  uint8_t seconds = 0xFF;
  HAL_StatusTypeDef st =
      HAL_I2C_Mem_Read(&hi2c3, (uint16_t)DS1307_I2C_ADDR, reg,
                        I2C_MEMADD_SIZE_8BIT, &seconds, 1, 50);
  if (out) {
    *out = seconds;
  }
  return st;
}

HAL_StatusTypeDef RTC_IsHalted(uint8_t *out_halted) {
  uint8_t raw = 0xFF;
  HAL_StatusTypeDef st = RTC_ReadSecondsRaw(&raw);
  if (st != HAL_OK) {
    return st;
  }
  if (out_halted) {
    *out_halted = (raw & DS1307_CH_BIT) ? 1 : 0;
  }
  return HAL_OK;
}

HAL_StatusTypeDef RTC_GetDateTime(RTC_DateTime_t *out) {
  if (!out) {
    return HAL_ERROR;
  }
  uint8_t regs[DS1307_NUM_CLOCK_REGS];
  uint8_t start_reg = DS1307_REG_SECONDS;
  HAL_StatusTypeDef st =
      HAL_I2C_Mem_Read(&hi2c3, (uint16_t)DS1307_I2C_ADDR, start_reg,
                        I2C_MEMADD_SIZE_8BIT, regs, sizeof(regs), 50);
  if (st != HAL_OK) {
    return st;
  }
  out->sec = bcd2bin(regs[0] & 0x7F);
  out->min = bcd2bin(regs[1] & 0x7F);
  /* Register 2, bit 6 selects 12h/24h mode. RTC_SetDateTime always writes
     24h mode, but tolerate 12h mode if something else ever set the clock. */
  if (regs[2] & 0x40) {
    uint8_t hour12 = bcd2bin(regs[2] & 0x1F);
    uint8_t is_pm = (regs[2] & 0x20) ? 1 : 0;
    out->hour = (uint8_t)((hour12 % 12) + (is_pm ? 12 : 0));
  } else {
    out->hour = bcd2bin(regs[2] & 0x3F);
  }
  /* regs[3] is day-of-week - not surfaced in RTC_DateTime_t, nothing needs it. */
  out->day = bcd2bin(regs[4] & 0x3F);
  out->month = bcd2bin(regs[5] & 0x1F);
  out->year = (uint16_t)(2000 + bcd2bin(regs[6]));
  return HAL_OK;
}

HAL_StatusTypeDef RTC_SetDateTime(const RTC_DateTime_t *dt) {
  if (!dt) {
    return HAL_ERROR;
  }
  uint8_t regs[DS1307_NUM_CLOCK_REGS];
  regs[0] = bin2bcd(dt->sec) & 0x7F; /* bit7=0: clears CH, starts oscillator */
  regs[1] = bin2bcd(dt->min);
  regs[2] = bin2bcd(dt->hour) & 0x3F; /* bit6=0: 24h mode */
  regs[3] = WeekdayRegisterValue(dt->year, dt->month, dt->day);
  regs[4] = bin2bcd(dt->day);
  regs[5] = bin2bcd(dt->month);
  regs[6] = bin2bcd((uint8_t)(dt->year >= 2000 ? dt->year - 2000 : 0));

  uint8_t start_reg = DS1307_REG_SECONDS;
  return HAL_I2C_Mem_Write(&hi2c3, (uint16_t)DS1307_I2C_ADDR, start_reg,
                            I2C_MEMADD_SIZE_8BIT, regs, sizeof(regs), 100);
}

RTC_DateTime_t RTC_GetBuildDateTime(void) {
  static const char *const kMonths[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  char month_str[4] = {0};
  int day = 1, year = 2026, hour = 0, min = 0, sec = 0;

  /* __DATE__ is "Mmm dd yyyy" (day is space-padded, not zero-padded);
     __TIME__ is "hh:mm:ss". Both are fixed-format compiler macros. */
  sscanf(__DATE__, "%3s %d %d", month_str, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

  uint8_t month = 1;
  for (uint8_t i = 0; i < 12; i++) {
    if (strcmp(month_str, kMonths[i]) == 0) {
      month = (uint8_t)(i + 1);
      break;
    }
  }

  RTC_DateTime_t dt;
  dt.year = (uint16_t)year;
  dt.month = month;
  dt.day = (uint8_t)day;
  dt.hour = (uint8_t)hour;
  dt.min = (uint8_t)min;
  dt.sec = (uint8_t)sec;
  return dt;
}

HAL_StatusTypeDef RTC_Init(uint8_t *out_was_halted) {
  uint8_t halted = 1;
  HAL_StatusTypeDef st = RTC_IsHalted(&halted);
  if (st != HAL_OK) {
    if (out_was_halted) {
      *out_was_halted = 0;
    }
    return st;
  }
  if (out_was_halted) {
    *out_was_halted = halted;
  }
  if (!halted) {
    /* Already running off the battery - the time it holds is more
       trustworthy than a fallback, so leave it alone. */
    return HAL_OK;
  }
  RTC_DateTime_t build_dt = RTC_GetBuildDateTime();
  return RTC_SetDateTime(&build_dt);
}
