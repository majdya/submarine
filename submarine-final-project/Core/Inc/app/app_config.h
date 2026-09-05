#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Runtime-adjustable settings, mutex-protected like app_state. Task
   context only - never call these from an ISR (osMutexAcquire is not
   ISR-safe, which is the exact class of bug that used to hang this
   firmware - see HAL_GPIO_EXTI_Callback's debounce, which is
   deliberately a plain static/HAL_GetTick() check instead of going
   through here). */
typedef struct {
  uint32_t battery_low_threshold_raw; /* ADC counts (0-4095). Monitor posts
                                          EVENT_SENSOR_THRESHOLD when the
                                          battery reading drops below this. */
  uint32_t monitor_period_ms;         /* Task_Monitor's sensor-poll interval. */
} AppConfig_t;

/* Must be called once, before any task that reads config starts (from
   MX_FREERTOS_Init's RTOS_MUTEX section, alongside AppState_Init). Sets
   the defaults below. */
void AppConfig_Init(void);

void AppConfig_Get(AppConfig_t *out);

/* Task context only. Validates nothing beyond non-zero monitor_period_ms
   (0 would spin Task_Monitor with no delay) - callers are trusted to pass
   sane values. */
void AppConfig_Set(const AppConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
