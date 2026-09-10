#include "task_ir_detect.h"
#include "app_event.h"
#include "cmsis_os2.h"
#include "ir_receiver.h"
#include "main.h"

/* D6/PB10's IR receiver can't be wired as an EXTI interrupt - it shares
   EXTI line 10 with the Silence button (PA10, EXTI15_10_IRQn already
   enabled and in use), and STM32's EXTI mux routes only one GPIO port to
   a given line number at a time. This task polls the pin instead, which
   needs no wiring change and, since IR receiver output is a burst of
   ~38kHz pulses lasting tens of milliseconds rather than one clean edge,
   is actually a better match than a single-edge interrupt would be
   anyway: OBJECT_DETECTED fires on the first active read, and
   OBJECT_CLEARED only once the receiver has been idle continuously for
   IR_CLEAR_TIMEOUT_MS, so gaps between individual pulses within one
   burst never look like the object went away and came back. */

#define IR_POLL_PERIOD_MS 20u
#define IR_CLEAR_TIMEOUT_MS 400u

void Task_IrDetect(void *argument) {
  (void)argument;

  uint8_t object_present = 0;
  uint32_t last_active_tick = 0;

  for (;;) {
    /* Active-low, same convention as SILENCE_Pin/PB3 (GPIO_PULLUP: idle
       HIGH, pulled LOW while the receiver sees a modulated IR carrier). */
    if (IrReceiver_Read() == GPIO_PIN_RESET) {
      last_active_tick = osKernelGetTickCount();
      if (!object_present) {
        object_present = 1;
        AppEvent_Post(EVENT_OBJECT_DETECTED, 0);
      }
    } else if (object_present) {
      uint32_t idle_ms = osKernelGetTickCount() - last_active_tick;
      if (idle_ms >= IR_CLEAR_TIMEOUT_MS) {
        object_present = 0;
        AppEvent_Post(EVENT_OBJECT_CLEARED, 0);
      }
    }

    osDelay(IR_POLL_PERIOD_MS);
  }
}
