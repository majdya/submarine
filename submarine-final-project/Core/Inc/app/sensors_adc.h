#ifndef APP_SENSORS_ADC_H
#define APP_SENSORS_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pure driver reads - no printing, no side effects beyond ADC1/ADC2. */
void ADC1_ReadLightTemp(uint32_t *light_raw, uint32_t *temp_raw);
uint32_t ADC2_ReadBatteryRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SENSORS_ADC_H */
