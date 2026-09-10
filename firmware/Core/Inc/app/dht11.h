#ifndef APP_DHT11_H
#define APP_DHT11_H

#include "stm32l4xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t humidity_int;
  uint8_t humidity_dec;
  uint8_t temp_int;
  uint8_t temp_dec;
} DHT11_Reading_t;

/* Bit-bangs one DHT11 read on PB5. Returns HAL_OK with *out filled and
   checksum verified, HAL_ERROR on checksum mismatch (raw bytes still
   filled into *out), or HAL_TIMEOUT if the sensor never responded.
   Requires DWT_Init() to have been called once already. */
HAL_StatusTypeDef DHT11_Read(DHT11_Reading_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_DHT11_H */
