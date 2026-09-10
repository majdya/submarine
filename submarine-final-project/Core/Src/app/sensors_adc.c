#include "sensors_adc.h"
#include "main.h"

void ADC1_ReadLightTemp(uint32_t *light_raw, uint32_t *temp_raw) {
  uint32_t light = 0, temp = 0;
  if (HAL_ADC_Start(&hadc1) == HAL_OK) {
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
      light = HAL_ADC_GetValue(&hadc1);
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
      temp = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);
  }
  if (light_raw) {
    *light_raw = light;
  }
  if (temp_raw) {
    *temp_raw = temp;
  }
}

uint32_t ADC2_ReadBatteryRaw(void) {
  uint32_t battery = 0;
  if (HAL_ADC_Start(&hadc2) == HAL_OK) {
    if (HAL_ADC_PollForConversion(&hadc2, 10) == HAL_OK) {
      battery = HAL_ADC_GetValue(&hadc2);
    }
    HAL_ADC_Stop(&hadc2);
  }
  return battery;
}

/* ===== Calibration Functions =====
 *
 * These functions use integer-only division to avoid floating-point
 * overhead in embedded code. Calibration constants are defined in
 * sensors_adc.h and can be tuned per hardware batch/assembly.
 */

uint32_t ADC2_BatteryRawToMillivolts(uint32_t raw_adc) {
  /* Linear scaling: mV = (raw * BATT_SCALE_NUM) / BATT_SCALE_DEN
   *
   * Default: (raw * 3300) / 4095 maps 0--4095 counts to 0--3300 mV.
   *
   * To calibrate:
   *   1. Apply a known voltage (e.g., 1.65V) to the potentiometer
   *   2. Read raw_adc with ADC2_ReadBatteryRaw()
   *   3. Calculate the slope: expected_mV / raw_adc
   *   4. Set SCALE_NUM/DEN so that (raw_adc * SCALE_NUM) / SCALE_DEN = expected_mV
   *
   * If your measurement shows the potentiometer is only 0--3.0V, adjust DEN down.
   * If the divider is different (e.g., 1:1 instead of 10k:20k), adjust NUM/DEN.
   */
  if (raw_adc > 4095) {
    raw_adc = 4095; /* Clamp to ADC range. */
  }
  return (raw_adc * BATT_SCALE_NUM) / BATT_SCALE_DEN;
}

uint32_t ADC1_LightRawToScaled(uint32_t raw_adc) {
  /* Linear scaling: scaled = (raw * LIGHT_SCALE_NUM) / LIGHT_SCALE_DEN
   *
   * Placeholder: SCALE_NUM=1, SCALE_DEN=1 returns raw counts unchanged.
   *
   * To calibrate to lux:
   *   1. Measure illuminance (lux) at several reference points using a
   *      light meter app or reference source (e.g., 10 lux, 100 lux, 500 lux)
   *   2. Read raw_adc with ADC1_ReadLightTemp(&light, &temp)
   *   3. Fit a linear regression: lux = m * raw_adc + b
   *      (many LDRs have inverse response: bright → high raw, but this
   *       depends on your specific sensor)
   *   4. Solve for SCALE_NUM and SCALE_DEN to approximate the slope m
   *
   * Note: LDR response curves are often nonlinear. If you see poor accuracy
   * across the range, consider a piecewise-linear fit or a lookup table.
   */
  if (raw_adc > 4095) {
    raw_adc = 4095; /* Clamp to ADC range. */
  }
  return (raw_adc * LIGHT_SCALE_NUM) / LIGHT_SCALE_DEN;
}
