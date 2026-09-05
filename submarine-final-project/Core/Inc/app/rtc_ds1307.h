#ifndef APP_RTC_DS1307_H
#define APP_RTC_DS1307_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reads DS1307 register 0x00 (seconds, BCD - bit 7 is the clock-halt flag)
   over I2C3. */
HAL_StatusTypeDef RTC_ReadSecondsRaw(uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_RTC_DS1307_H */
