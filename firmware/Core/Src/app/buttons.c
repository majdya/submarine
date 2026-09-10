#include "buttons.h"
#include "main.h"

GPIO_PinState Button_ReadSilence(void) {
  return HAL_GPIO_ReadPin(SILENCE_GPIO_Port, SILENCE_Pin);
}

GPIO_PinState Button_ReadObjectDetect(void) {
  return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3);
}
