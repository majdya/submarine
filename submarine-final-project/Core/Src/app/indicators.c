#include "indicators.h"
#include "main.h"

static uint8_t s_pwm_started = 0;

static void EnsurePwmStarted(void) {
  if (!s_pwm_started) {
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1); /* buzzer, PB4 */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2); /* RGB Red, PC7 */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3); /* RGB Green, PC8 */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4); /* RGB Blue, PB1 */
    s_pwm_started = 1;
  }
}

void LED1_Toggle(void) { HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin); }

void LED2_Toggle(void) { HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); }

void Buzzer_SetDuty(uint16_t duty) {
  EnsurePwmStarted();
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
}

void RGB_SetDuty(uint16_t red_duty, uint16_t green_duty, uint16_t blue_duty) {
  EnsurePwmStarted();
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, red_duty);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, green_duty);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, blue_duty);
}
