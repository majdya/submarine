#include "task_monitor.h"
#include "app_config.h"
#include "app_event.h"
#include "app_state.h"
#include "cmsis_os2.h"
#include "dht11.h"
#include "main.h"
#include "rtc_ds1307.h"
#include "sensors_adc.h"

/* Per-parameter Normal/Warning/Error classification, per SW-FD-LNC-001
   S2.1: "the module compares each measured value against its configured
   limits." Temperature gets a full range per mode; the other three get a
   single lower boundary per mode (spec wording for those three names only
   a lower limit, not an upper one). */
static AppMode_t ClassifyTemp(const AppConfig_t *cfg, int32_t temp_c) {
  if (temp_c >= cfg->temp_normal_min && temp_c <= cfg->temp_normal_max) {
    return APP_MODE_NORMAL;
  }
  if (temp_c >= cfg->temp_warning_min && temp_c <= cfg->temp_warning_max) {
    return APP_MODE_WARNING;
  }
  return APP_MODE_ERROR;
}

static AppMode_t ClassifyLowerBound(uint32_t value, uint32_t normal_lower,
                                     uint32_t warning_lower) {
  if (value >= normal_lower) {
    return APP_MODE_NORMAL;
  }
  if (value >= warning_lower) {
    return APP_MODE_WARNING;
  }
  return APP_MODE_ERROR;
}

static AppMode_t WorstMode(AppMode_t a, AppMode_t b) {
  return (b > a) ? b : a;
}

void Task_Monitor(void *argument) {
  (void)argument;

  /* Last-known-good DHT11 reading, held across a transient failed poll so
     one dropped read (the DHT11's one-wire timing is notoriously fussy)
     doesn't zero out temp/humidity and trigger a false Error mode. Stays
     "invalid" only until the very first successful read after boot. */
  static uint8_t s_dht_valid = 0;
  static uint8_t s_dht_humidity = 0;
  static uint8_t s_dht_temp = 0;

  AppMode_t previous_mode = APP_MODE_NORMAL;

  for (;;) {
    AppConfig_t cfg;
    AppConfig_Get(&cfg);

    AppState_t readings = {0};

    ADC1_ReadLightTemp(&readings.light_raw, &readings.temp_raw);
    readings.battery_raw = ADC2_ReadBatteryRaw();

    DHT11_Reading_t dht = {0, 0, 0, 0};
    if (DHT11_Read(&dht) == HAL_OK) {
      s_dht_valid = 1;
      s_dht_humidity = dht.humidity_int;
      s_dht_temp = dht.temp_int;
    }
    readings.dht11_valid = s_dht_valid;
    readings.dht11_humidity_int = s_dht_humidity;
    readings.dht11_temp_int = s_dht_temp;

    uint8_t rtc_seconds = 0xFF;
    if (RTC_ReadSecondsRaw(&rtc_seconds) == HAL_OK) {
      readings.rtc_valid = 1;
      readings.rtc_seconds_raw = rtc_seconds;
    }

    readings.last_monitor_tick = osKernelGetTickCount();

    /* Until the DHT11 has ever produced a good reading, its two
       parameters don't get a vote - treat them as Normal rather than
       manufacturing an Error mode out of "no data yet" during the first
       couple of seconds after boot. */
    /* Per-parameter enable/disable (deliberate extension beyond the spec,
       see app_config.h's *_enabled fields): a disabled parameter is still
       sampled and stored in `readings` above exactly as before - it's
       excluded only from this vote, so it never contributes to Warning/
       Error alarm status while disabled. */
    AppMode_t mode = APP_MODE_NORMAL;
    if (s_dht_valid) {
      if (cfg.temp_enabled) {
        mode = WorstMode(mode, ClassifyTemp(&cfg, (int32_t)s_dht_temp));
      }
      if (cfg.humidity_enabled) {
        mode = WorstMode(mode, ClassifyLowerBound(s_dht_humidity,
                                                   cfg.humidity_normal_lower,
                                                   cfg.humidity_warning_lower));
      }
    }
    if (cfg.light_enabled) {
      mode = WorstMode(mode, ClassifyLowerBound(readings.light_raw,
                                                 cfg.light_normal_lower,
                                                 cfg.light_warning_lower));
    }
    if (cfg.battery_enabled) {
      mode = WorstMode(mode, ClassifyLowerBound(readings.battery_raw,
                                                 cfg.battery_normal_lower,
                                                 cfg.battery_warning_lower));
    }
    readings.mode = mode;

    AppState_SetSensorReadings(&readings);

    /* Spec S2.1: "If the measured data results in a mode change, the
       module sends a message to the Event module containing the measured
       values" - only on an actual change, not every poll. Task_Event
       reads the just-published AppState itself (light/temp/battery/dht)
       rather than this event carrying a second copy of the readings. */
    if (mode != previous_mode) {
      AppEvent_Post(EVENT_MODE_CHANGED,
                    APP_EVENT_PACK_MODE_CHANGE(previous_mode, mode));
      previous_mode = mode;
    }

    osDelay(cfg.monitor_period_ms);
  }
}
