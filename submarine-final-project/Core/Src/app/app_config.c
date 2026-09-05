#include "app_config.h"
#include <string.h>

/* Same placeholder battery threshold and poll period Task_Monitor always
   used, just centralized here instead of a local #define, so they can be
   changed at runtime instead of only at compile time. */
#define DEFAULT_BATTERY_LOW_THRESHOLD_RAW 200u
#define DEFAULT_MONITOR_PERIOD_MS 1000u

static AppConfig_t s_config;
static osMutexId_t s_config_mutex;
static const osMutexAttr_t s_config_mutex_attr = {
    .name = "AppConfigMutex",
};

void AppConfig_Init(void) {
  memset(&s_config, 0, sizeof(s_config));
  s_config.battery_low_threshold_raw = DEFAULT_BATTERY_LOW_THRESHOLD_RAW;
  s_config.monitor_period_ms = DEFAULT_MONITOR_PERIOD_MS;
  s_config_mutex = osMutexNew(&s_config_mutex_attr);
}

void AppConfig_Get(AppConfig_t *out) {
  if (osMutexAcquire(s_config_mutex, osWaitForever) != osOK) {
    /* Fall back to safe defaults rather than leaving *out uninitialized -
       a zeroed monitor_period_ms would spin Task_Monitor with no delay. */
    out->battery_low_threshold_raw = DEFAULT_BATTERY_LOW_THRESHOLD_RAW;
    out->monitor_period_ms = DEFAULT_MONITOR_PERIOD_MS;
    return;
  }
  *out = s_config;
  osMutexRelease(s_config_mutex);
}

void AppConfig_Set(const AppConfig_t *cfg) {
  if (!cfg || osMutexAcquire(s_config_mutex, osWaitForever) != osOK) {
    return;
  }
  s_config = *cfg;
  if (s_config.monitor_period_ms == 0) {
    s_config.monitor_period_ms = DEFAULT_MONITOR_PERIOD_MS;
  }
  osMutexRelease(s_config_mutex);
}
