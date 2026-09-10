#include "app_config.h"
#include "flash_config_store.h"
#include <string.h>

/* ===== Calibrated Defaults =====
 *
 * These values should be tuned based on:
 *   1. Your specific hardware batch (potentiometer, LDR, DHT11)
 *   2. Battery technology (LiPo, Li-ion, alkaline - affects voltage curve)
 *   3. Deployment requirements (min safe voltage, desired illuminance thresholds)
 *
 * Workflow for calibration:
 *   1. Measure your battery at several known states and record the ADC raw values
 *   2. Convert raw → mV using ADC2_BatteryRawToMillivolts() from sensors_adc.c
 *   3. Decide the Warning/Error thresholds (e.g., battery < 2.0V = warning)
 *   4. Update DEFAULT_BATTERY_* below with the raw ADC counts corresponding to those voltages
 *
 * Same process for light sensor: measure illuminance at known levels, decide thresholds,
 * update DEFAULT_LIGHT_* with the corresponding ADC raw values.
 *
 * Until you calibrate, use these conservative placeholders to avoid false alarms.
 */

/* Temperature: DHT11 output range is ~0--60°C. Adjust based on deployment environment. */
#define DEFAULT_TEMP_NORMAL_MIN 15
#define DEFAULT_TEMP_NORMAL_MAX 30
#define DEFAULT_TEMP_WARNING_MIN 5
#define DEFAULT_TEMP_WARNING_MAX 40

/* Humidity: DHT11 output range is 0--100 %RH. Adjust based on your requirements. */
#define DEFAULT_HUMIDITY_NORMAL_LOWER 30u
#define DEFAULT_HUMIDITY_WARNING_LOWER 15u

/* Light Sensor (LDR, ADC1_IN6, raw ADC counts 0--4095).
 *
 * LDRs have inverse response: bright → HIGH raw counts, dark → LOW raw counts.
 * These placeholders assume:
 *   - Normal (well-lit): raw ≥ 200 ADC counts
 *   - Warning (dim): raw ≥ 50 ADC counts
 *   - Error (very dark): raw < 50 ADC counts
 *
 * TO CALIBRATE:
 *   1. Place your LDR under known illumination (e.g., desk lamp ~500 lux, phone flashlight ~1000 lux)
 *   2. Call ADC1_ReadLightTemp(&light, &temp) and note the raw value
 *   3. Repeat at several levels (bright, medium, dim, dark)
 *   4. Fit to your requirements: decide what raw counts = "normal light level" for your mission
 *   5. Update DEFAULT_LIGHT_NORMAL_LOWER and DEFAULT_LIGHT_WARNING_LOWER with those raw values
 */
#define DEFAULT_LIGHT_NORMAL_LOWER 200u
#define DEFAULT_LIGHT_WARNING_LOWER 50u

/* Battery Sensor (Potentiometer on ADC2_IN5, raw ADC counts 0--4095).
 *
 * Scaling: mV = (raw * 3300) / 4095 (adjustable in sensors_adc.h:BATT_SCALE_*)
 *
 * These placeholders assume:
 *   - Normal (good battery): raw ≥ 2500 counts → ≥ ~2.03V (3300*2500/4095)
 *   - Warning (low battery): raw ≥ 1000 counts → ≥ ~0.81V (3300*1000/4095)
 *   - Error (critically low): raw < 1000 counts → < 0.81V
 *
 * TO CALIBRATE:
 *   1. Set your potentiometer (or supply voltage) to a known voltage (e.g., 2.5V for "low")
 *   2. Call ADC2_ReadBatteryRaw() and note the raw value
 *   3. Repeat at min safe voltage (e.g., 1.8V), warning threshold (e.g., 2.0V), normal (e.g., 3.0V)
 *   4. Calculate the raw ADC count at each threshold
 *   5. Update these defaults with the raw values
 *
 * Example calibration (for typical 3.3V LiPo single-cell):
 *   - 3.0V (normal): raw ≈ 3720
 *   - 2.5V (low-warning): raw ≈ 3100
 *   - 2.0V (critical-warning): raw ≈ 2480
 *   Then set:
 *     DEFAULT_BATTERY_NORMAL_LOWER = 3100
 *     DEFAULT_BATTERY_WARNING_LOWER = 2480
 */
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
  cfg->temp_enabled = 1;
  cfg->humidity_enabled = 1;
  cfg->light_enabled = 1;
  cfg->battery_enabled = 1;
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
