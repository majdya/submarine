#ifndef APP_STATE_H
#define APP_STATE_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Latest sensor readings + overall system health, shared across tasks.
   All access goes through the functions below, which take a mutex - no
   task ever touches the struct fields directly. */
typedef struct {
  uint32_t light_raw;
  uint32_t temp_raw;
  uint32_t battery_raw;
  uint32_t humidity_adc_raw;

  uint8_t dht11_humidity_int;
  uint8_t dht11_temp_int;
  uint8_t dht11_valid; /* 1 if the last DHT11_Read() succeeded */

  uint8_t rtc_seconds_raw;
  uint8_t rtc_valid;

  uint32_t last_monitor_tick;
  uint8_t system_healthy; /* Watchdog task only refreshes IWDG while this is 1 */
} AppState_t;

/* Must be called once, before any task that touches app state starts
   (from MX_FREERTOS_Init's RTOS_MUTEX section). */
void AppState_Init(void);

void AppState_SetSensorReadings(const AppState_t *readings);
void AppState_Get(AppState_t *out);

void AppState_SetHealthy(uint8_t healthy);
uint8_t AppState_IsHealthy(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_H */
