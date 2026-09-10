#ifndef APP_INDICATORS_H
#define APP_INDICATORS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void LED1_Toggle(void);
void LED2_Toggle(void);

/* Starts TIM3's PWM channels on first call if not already running.
   duty is 0-999 (matches TIM3's ARR). */
void Buzzer_SetDuty(uint16_t duty);
void RGB_SetDuty(uint16_t red_duty, uint16_t green_duty, uint16_t blue_duty);

#ifdef __cplusplus
}
#endif

#endif /* APP_INDICATORS_H */
