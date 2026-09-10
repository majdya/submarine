#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

GPIO_PinState Button_ReadSilence(void);
GPIO_PinState Button_ReadObjectDetect(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_BUTTONS_H */
