#ifndef APP_STATE_H
#define APP_STATE_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Operating Mode per the spec's Monitor module (SW-FD-LNC-001 S2.1):
   Normal/Warning/Error, ordered so a plain integer compare gives the
   "worse of two modes" (Error > Warning > Normal). */
typedef enum {
  APP_MODE_NORMAL = 0,
  APP_MODE_WARNING = 1,
  APP_MODE_ERROR = 2,
} AppMode_t;

/* Latest sensor readings + overall system health, shared across tasks.
   All access goes through the functions below, which take a mutex - no
   task ever touches the struct fields directly. */
typedef struct {
  uint32_t light_raw;
  uint32_t temp_raw;
  uint32_t battery_raw;

  uint8_t dht11_humidity_int;
  uint8_t dht11_temp_int;
  uint8_t dht11_valid; /* 1 once at least one DHT11_Read() has ever
                           succeeded - Task_Monitor holds the last known
                           good reading across transient failures instead
                           of zeroing it, so this only goes back to 0 on a
                           fresh boot that hasn't read the sensor yet. */

  uint8_t rtc_seconds_raw;
  uint8_t rtc_valid;

  uint32_t last_monitor_tick;
  uint8_t system_healthy; /* Watchdog task only refreshes IWDG while this is 1 */

  AppMode_t mode; /* Overall Operating Mode, computed by Task_Monitor as
                      the worst-case across all monitored parameters. */
} AppState_t;

/* Must be called once, before any task that touches app state starts
   (from MX_FREERTOS_Init's RTOS_MUTEX section). */
void AppState_Init(void);

void AppState_SetSensorReadings(const AppState_t *readings);
void AppState_Get(AppState_t *out);

void AppState_SetHealthy(uint8_t healthy);
uint8_t AppState_IsHealthy(void);

/* Human-readable name for a mode, e.g. for log lines and PC-side display.
   Never returns NULL. */
const char *AppMode_Name(AppMode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_H */
