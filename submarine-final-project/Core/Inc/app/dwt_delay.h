#ifndef APP_DWT_DELAY_H
#define APP_DWT_DELAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Microsecond delay via the Cortex-M4 DWT cycle counter. HAL_Delay() is
   1ms-resolution (TIM6-based), too coarse for protocols like DHT11 that
   need ~20-80us timing. Call DWT_Init() once before using DWT_Delay_us(). */
void DWT_Init(void);
void DWT_Delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* APP_DWT_DELAY_H */
