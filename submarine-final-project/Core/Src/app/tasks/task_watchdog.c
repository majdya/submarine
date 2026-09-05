#include "task_watchdog.h"
#include "app_state.h"
#include "cmsis_os2.h"
#include "main.h"

/* NOTE: MX_IWDG_Init() is still commented out in main.c (disabled while
   validating hardware - see the sticky-IWDG writeup). This task is wired
   and ready for when it's re-enabled: it only refreshes while
   AppState_IsHealthy() is true, so an unhealthy system reboots via IWDG
   instead of being kept alive artificially. */
#define WATCHDOG_PERIOD_MS 250

void Task_Watchdog(void *argument) {
  (void)argument;
  for (;;) {
    if (AppState_IsHealthy() && IwdgIsInitialized()) {
      HAL_IWDG_Refresh(&hiwdg);
    }
    osDelay(WATCHDOG_PERIOD_MS);
  }
}
