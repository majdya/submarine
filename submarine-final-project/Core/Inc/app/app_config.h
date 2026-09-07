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
   through here).

   Per-parameter Normal/Warning limits below implement the Monitor
   module's Operating Mode rule (SW-FD-LNC-001 S2.1): "the module
   compares each measured value against its configured limits" and picks
   Normal/Warning/Error per parameter, then the overall mode is the
   worst of the four. These are all meant to be set by the Central
   Computer via Management Commands (Task #7) and persisted to flash
   (Task #5); until then every field below is a documented placeholder,
   not a calibrated real-world limit. */
typedef struct {
  /* Temperature, degrees C, from the DHT11 (dht11_temp_int) - the spec
     names two full ranges rather than a single lower bound, so a value
     is Normal inside [temp_normal_min, temp_normal_max], Warning inside
     [temp_warning_min, temp_warning_max] (which must fully contain the
     Normal range), and Error outside the Warning range entirely. */
  int32_t temp_normal_min;
  int32_t temp_normal_max;
  int32_t temp_warning_min;
  int32_t temp_warning_max;

  /* Humidity (%RH, DHT11 dht11_humidity_int), light (raw ADC counts,
     0-4095) and battery (raw ADC counts from the potentiometer stand-in,
     0-4095): the spec gives each of these a single lower boundary per
     mode ("a lower boundary value ... that defines the Normal/Warning
     mode condition"), so a value is Normal at/above its normal_lower,
     Warning at/above its warning_lower, Error below that. */
  uint32_t humidity_normal_lower;
  uint32_t humidity_warning_lower;
  uint32_t light_normal_lower;
  uint32_t light_warning_lower;
  uint32_t battery_normal_lower;
  uint32_t battery_warning_lower;

  uint32_t monitor_period_ms; /* Task_Monitor's sensor-poll interval. */
} AppConfig_t;

/* Must be called once, before any task that reads config starts (from
   MX_FREERTOS_Init's RTOS_MUTEX section, alongside AppState_Init). Sets
   the defaults below. */
void AppConfig_Init(void);

void AppConfig_Get(AppConfig_t *out);

/* Task context only. Validates nothing beyond non-zero monitor_period_ms
   (0 would spin Task_Monitor with no delay) - callers are trusted to pass
   sane values (e.g. warning_lower <= normal_lower, warning range wider
   than the normal range). */
void AppConfig_Set(const AppConfig_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONFIG_H */
