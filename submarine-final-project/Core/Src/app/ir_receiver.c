#include "ir_receiver.h"

GPIO_PinState IrReceiver_Read(void) {
  return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10);
}
