#ifndef APP_IR_RECEIVER_H
#define APP_IR_RECEIVER_H

#include "stm32l4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* D6/PB10 - the 9-in-1 shield's onboard IR receiver output. Wired as a
   plain polled GPIO input (GPIO_PULLUP, no EXTI) rather than an
   interrupt: PB10 and the Silence button (PA10/D2) both sit on EXTI line
   10, and STM32's EXTI mux can only route one GPIO port to a given line
   number at a time - configuring PB10 as an interrupt would silently
   steal Silence's line. See task_ir_detect.c for the polling logic that
   works around this, and hardware.md S7 for the full explanation.

   Idle HIGH, active (pulled) LOW when the receiver sees a 38kHz-modulated
   IR carrier - true for essentially every cheap IR receiver module of
   this type (VS1838B/HX1838/TL1838 and equivalents), matching the same
   active-low/pull-up convention already used for both push buttons. */
GPIO_PinState IrReceiver_Read(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_IR_RECEIVER_H */
