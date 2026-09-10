#ifndef APP_SENSORS_ADC_H
#define APP_SENSORS_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure driver reads - no printing, no side effects beyond ADC1/ADC2. */
void ADC1_ReadLightTemp(uint32_t *light_raw, uint32_t *temp_raw);
uint32_t ADC2_ReadBatteryRaw(void);

/* ===== Calibration & Scaling Functions =====
 *
 * Hardware Setup (per hardware.md §5):
 *   Battery (Potentiometer): A0 / PA0 on ADC2_IN5
 *     - Divider: 10k/20k scales 0--5V (shield) → 0--3.3V (STM32)
 *     - Raw range: 0--4095 ADC counts
 *     - Mapped range: 0--3300 mV (after scaling)
 *
 *   Light (LDR): A1 / PA1 on ADC1_IN6
 *     - Divider: 10k/20k scales 0--5V (shield) → 0--3.3V (STM32)
 *     - Raw range: 0--4095 ADC counts
 *     - Typically represents inverse illuminance (higher raw = darker)
 *
 * Calibration workflow:
 *   1. Measure battery voltage at known reference points (e.g. 0V, 1.65V, 3.3V)
 *      and record the corresponding ADC raw values.
 *   2. Fit linear regression: millivolts = (raw_adc * BATT_SCALE_NUM) / BATT_SCALE_DEN
 *   3. Do the same for light sensor if mapping to lux/illuminance.
 *   4. Update the scale constants below with calibrated values.
 */

/* ===== Battery Calibration (Potentiometer ADC2_IN5) =====
 *
 * Linear scaling: millivolts = (raw_adc * BATT_SCALE_NUM) / BATT_SCALE_DEN
 *
 * Typical calibration (for 0--3.3V with divider):
 *   - At 0V (potentiometer = 0Ω): raw ≈ 0, expected mV = 0
 *   - At 3.3V (potentiometer = max): raw ≈ 4095, expected mV = 3300
 *   - Linear fit: mV = (raw * 3300) / 4095 ≈ (raw * 3300) / 4095
 *
 * To calibrate:
 *   - Use a precision multimeter or voltage reference
 *   - Apply known voltages to the potentiometer (0V, 1.65V, 3.3V)
 *   - Read the raw ADC values with ADC2_ReadBatteryRaw()
 *   - Fit a line through the points, then set SCALE_NUM/DEN
 *
 * SCALE_NUM and SCALE_DEN are chosen to avoid floating point
 * (all integer division). Adjust as needed for your specific potentiometer.
 */
#define BATT_SCALE_NUM 3300u
#define BATT_SCALE_DEN 4095u

/* Convert raw ADC battery count to millivolts (0--3300 mV). */
uint32_t ADC2_BatteryRawToMillivolts(uint32_t raw_adc);

/* ===== Light Sensor Calibration (LDR ADC1_IN6) =====
 *
 * Linear scaling: illuminance_lux = (raw_adc * LIGHT_SCALE_NUM) / LIGHT_SCALE_DEN
 *
 * LDRs (light-dependent resistors) typically have inverse response:
 *   - High illuminance (bright) → low resistance → high ADC count
 *   - Low illuminance (dark) → high resistance → low ADC count
 *
 * To calibrate:
 *   - Measure illuminance at several points using a lux meter app or reference
 *   - Record the raw ADC value at each known lux level
 *   - Fit a line through the points (or use inverse model if needed)
 *   - Set SCALE_NUM/DEN to the slope of your fit
 *
 * Placeholder: assumes 1:1 mapping (raw count ≈ relative brightness).
 * Real calibration may need inverse scaling or a lookup table if the
 * LDR's response curve is highly nonlinear.
 *
 * For now, we keep raw counts directly (no scaling). Update LIGHT_SCALE_*
 * once you have real lux calibration data.
 */
#define LIGHT_SCALE_NUM 1u
#define LIGHT_SCALE_DEN 1u

/* Convert raw ADC light count to a scaled value (e.g., relative brightness 0--4095).
 * If you calibrate to lux, update the scaling constants above. */
uint32_t ADC1_LightRawToScaled(uint32_t raw_adc);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSORS_ADC_H */
