#ifndef APP_SDCARD_H
#define APP_SDCARD_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Raw single-byte SPI3 exchange with CS (PD2) asserted - proves
   clocking/wiring only, NOT a real SD init sequence. See note in
   sdcard.c about SPI3's current clock being too fast for real SD init. */
HAL_StatusTypeDef SD_RawByteExchange(uint8_t tx, uint8_t *rx);

#ifdef __cplusplus
}
#endif

#endif /* APP_SDCARD_H */
