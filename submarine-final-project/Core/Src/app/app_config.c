#include "app_config.h"
#include "flash_config_store.h"
#include <string.h>

/* Placeholder defaults - not calibrated against real hardware limits.
   Documented as placeholders in app_config.h; meant to be overwritten via
   Management Commands (Task #7) and then persisted to flash (Task #5). */
#define DEFAULT_TEMP_NORMAL_MIN 15
#define DEFAULT_TEMP_NORMAL_MAX 30
#define DEFAULT_TEMP_WARNING_MIN 5
#define DEFAULT_TEMP_WARNING_MAX 40

#define DEFAULT_HUMIDITY_NORMAL_LOWER 30u
#define DEFAULT_HUMIDITY_WARNING_LOWER 15u

#define DEFAULT_LIGHT_NORMAL_LOWER 200u
#define DEFAULT_LIGHT_WARNING_LOWER 50u

#define DEFAULT_BATTERY_NORMAL_LOWER 2500u
#define DEFAULT_BATTERY_WARNING_LOWER 1000u

#define DEFAULT_MONITOR_PERIOD_MS 5000u /* spec: "once every 5 seconds" */

static AppConfig_t s_config;
static osMutexId_t s_config_mutex;
static const osMutexAttr_t s_config_mutex_attr = {
    .name = "AppConfigMutex",
};

static void ApplyDefaults(AppConfig_t *cfg) {
  memset(cfg, 0, sizeof(*cfg));
  cfg->temp_normal_min = DEFAULT_TEMP_NORMAL_MIN;
  cfg->temp_normal_max = DEFAULT_TEMP_NORMAL_MAX;
  cfg->temp_warning_min = DEFAULT_TEMP_WARNING_MIN;
  cfg->temp_warning_max = DEFAULT_TEMP_WARNING_MAX;
  cfg->humidity_normal_lower = DEFAULT_HUMIDITY_NORMAL_LOWER;
  cfg->humidity_warning_lower = DEFAULT_HUMIDITY_WARNING_LOWER;
  cfg->light_normal_lower = DEFAULT_LIGHT_NORMAL_LOWER;
  cfg->light_warning_lower = DEFAULT_LIGHT_WARNING_LOWER;
  cfg->battery_normal_lower = DEFAULT_BATTERY_NORMAL_LOWER;
  cfg->battery_warning_lower = DEFAULT_BATTERY_WARNING_LOWER;
  cfg->monitor_period_ms = DEFAULT_MONITOR_PERIOD_MS;
}

void AppConfig_Init(void) {
  /* Task #5: load whatever was last persisted to flash; if there's
     nothing valid there yet (first boot, or a blank/corrupted page),
     fall back to defaults and write them out immediately so the flash
     copy and the in-RAM copy always agree from here on. */
  if (!FlashConfigStore_Load(&s_config, sizeof(s_config))) {
    ApplyDefaults(&s_config);
    (void)FlashConfigStore_Save(&s_config, sizeof(s_config));
  }
  s_config_mutex = osMutexNew(&s_config_mutex_attr);
}

void AppConfig_Get(AppConfig_t *out) {
  if (osMutexAcquire(s_config_mutex, osWaitForever) != osOK) {
    /* Fall back to safe defaults rather than leaving *out uninitialized -
       a zeroed monitor_period_ms would spin Task_Monitor with no delay. */
    ApplyDefaults(out);
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
  /* Persist while still holding the mutex - a reader calling
     AppConfig_Get() during the flash write (a few ms) just blocks
     briefly, same as any other Set/Get race; there is no reader on an
     ISR or other latency-sensitive path. If the write fails (e.g. worn
     page, write-protected flash) the new settings still take effect for
     this power cycle - only persistence across reboot is lost, which
     Task #7's command handler should surface back to the Central
     Computer rather than fail the command outright. */
  (void)FlashConfigStore_Save(&s_config, sizeof(s_config));
  osMutexRelease(s_config_mutex);
}
