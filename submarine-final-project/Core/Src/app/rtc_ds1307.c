#include "rtc_ds1307.h"
#include "main.h"

#define DS1307_I2C_ADDR (0x68 << 1)

HAL_StatusTypeDef RTC_ReadSecondsRaw(uint8_t *out) {
  uint8_t seconds_reg = 0x00;
  uint8_t seconds = 0xFF;
  HAL_StatusTypeDef st =
      HAL_I2C_Mem_Read(&hi2c3, (uint16_t)DS1307_I2C_ADDR, seconds_reg,
                       I2C_MEMADD_SIZE_8BIT, &seconds, 1, 50);
  if (out) {
    *out = seconds;
  }
  return st;
}
