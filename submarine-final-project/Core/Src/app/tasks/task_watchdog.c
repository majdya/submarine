#include "task_watchdog.h"
#include "app_state.h"
#include "cmsis_os2.h"
#include "main.h"

/* Found via a real boot-loop: gating the refresh on AppState_IsHealthy()
   (as this used to) creates a race between IWDG's countdown - which
   starts the instant MX_IWDG_Init() runs in main(), very early - and
   AppState_SetHealthy(1), which doesn't happen until deep into
   StartInitTask (after RTC_Init()'s I2C round-trips). Nothing refreshes
   IWDG anywhere in that window, so any latency added to it (like the RTC
   I2C calls added this session) can burn through the whole timeout
   before the first refresh ever happens - IWDG fires on a slow-but-
   perfectly-fine boot, not a real hang. That's what "IWDG PIN/NRST reset
   on every boot, cut off at the same early point" was.

   IWDG's job is narrower than "is the app healthy" - it's "is the
   scheduler still alive and running tasks at all". That's what refreshing
   unconditionally (as long as IWDG is initialized) actually checks: this
   task only gets CPU time if the scheduler is genuinely functioning, so a
   real deadlock/kernel corruption still stops the refresh and reboots the
   board, same as before. An app-level "unhealthy" condition needs its own
   explicit handling (e.g. logging it, or a deliberate reset) rather than
   silently starving the last-resort hardware safety net. */
#define WATCHDOG_PERIOD_MS 250

void Task_Watchdog(void *argument) {
  (void)argument;
  for (;;) {
    if (IwdgIsInitialized()) {
      HAL_IWDG_Refresh(&hiwdg);
    }
    osDelay(WATCHDOG_PERIOD_MS);
  }
}
