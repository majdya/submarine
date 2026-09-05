#include "task_monitor.h"
#include "app_config.h"
#include "app_event.h"
#include "app_state.h"
#include "cmsis_os2.h"
#include "dht11.h"
#include "main.h"
#include "rtc_ds1307.h"
#include "sensors_adc.h"

void Task_Monitor(void *argument) {
  (void)argument;
  for (;;) {
    AppConfig_t cfg;
    AppConfig_Get(&cfg);

    AppState_t readings = {0};

    ADC1_ReadLightTemp(&readings.light_raw, &readings.temp_raw);
    readings.battery_raw = ADC2_ReadBatteryRaw();

    DHT11_Reading_t dht = {0, 0, 0, 0};
    if (DHT11_Read(&dht) == HAL_OK) {
      readings.dht11_valid = 1;
      readings.dht11_humidity_int = dht.humidity_int;
      readings.dht11_temp_int = dht.temp_int;
    }

    uint8_t rtc_seconds = 0xFF;
    if (RTC_ReadSecondsRaw(&rtc_seconds) == HAL_OK) {
      readings.rtc_valid = 1;
      readings.rtc_seconds_raw = rtc_seconds;
    }

    readings.last_monitor_tick = osKernelGetTickCount();
    AppState_SetSensorReadings(&readings);

    if (readings.battery_raw < cfg.battery_low_threshold_raw) {
      AppEvent_Post(EVENT_SENSOR_THRESHOLD, readings.battery_raw);
    }

    osDelay(cfg.monitor_period_ms);
  }
}
