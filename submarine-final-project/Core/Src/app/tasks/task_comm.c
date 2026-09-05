#include "task_comm.h"
#include "app_comm.h"
#include "app_serial.h"
#include "cmsis_os2.h"
#include <stdio.h>

/* NOTE: there is no PC-side Submarine Manager app yet and no defined wire
   protocol - this sends plain text lines over the same UART used for
   diagnostics, which is enough to prove the path but is a placeholder,
   not the real protocol. Revisit once the PC-side app exists.

   Keep-alive is folded into this task rather than being a separate one:
   it's the same "tell the PC something, periodically" responsibility,
   just with nothing queued. */
#define COMM_HEARTBEAT_PERIOD_MS 5000

void Task_Comm(void *argument) {
  (void)argument;
  AppCommMsg_t msg;

  for (;;) {
    osStatus_t st = AppComm_Wait(&msg, COMM_HEARTBEAT_PERIOD_MS);
    char line[APP_COMM_MSG_LEN + 16];
    int len;
    if (st == osOK) {
      len = snprintf(line, sizeof(line), "[COMM] %s\r\n", msg.text);
    } else {
      len = snprintf(line, sizeof(line), "[COMM] heartbeat tick=%lu\r\n",
                      (unsigned long)osKernelGetTickCount());
    }
    if (len > 0) {
      Serial_Print(line);
    }
  }
}
