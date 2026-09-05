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
