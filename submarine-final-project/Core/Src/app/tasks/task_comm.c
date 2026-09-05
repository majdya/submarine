#include "task_comm.h"
#include "app_comm.h"
#include "app_serial.h"
#include "cmsis_os2.h"
#include <stdint.h>
#include <stdio.h>

/* Minimal real wire protocol for the (still unbuilt) PC-side Submarine
   Manager app, replacing the earlier plain-text placeholder lines.
   NMEA-style ASCII sentence framing, since it's simple to parse, easy to
   eyeball on a terminal while debugging, and well understood:

     $LNC,<TYPE>,<fields...>*<CHK>\r\n

   <CHK> is 2 hex digits: the XOR of every byte between '$' and '*'
   (exclusive), the same checksum scheme NMEA GPS sentences use. TYPE is
   EVT for a forwarded app event (fields = the event name, plus a value
   for THRESHOLD) or HB for the periodic keep-alive heartbeat (field =
   uptime in ms). No PC-side parser exists yet to validate this against -
   revisit if the real app ends up wanting something different (binary
   framing, sequence numbers, etc).

   Keep-alive is folded into this task rather than being a separate one:
   it's the same "tell the PC something, periodically" responsibility,
   just with nothing queued. */
#define COMM_HEARTBEAT_PERIOD_MS 5000

static uint8_t NmeaChecksum(const char *body, int len) {
  uint8_t chk = 0;
  for (int i = 0; i < len; i++) {
    chk ^= (uint8_t)body[i];
  }
  return chk;
}

void Task_Comm(void *argument) {
  (void)argument;
  AppCommMsg_t msg;

  for (;;) {
    osStatus_t st = AppComm_Wait(&msg, COMM_HEARTBEAT_PERIOD_MS);

    char body[APP_COMM_MSG_LEN + 16];
    int blen;
    if (st == osOK) {
      blen = snprintf(body, sizeof(body), "LNC,EVT,%s", msg.text);
    } else {
      blen = snprintf(body, sizeof(body), "LNC,HB,%lu",
                       (unsigned long)osKernelGetTickCount());
    }
    if (blen <= 0) {
      continue;
    }

    uint8_t chk = NmeaChecksum(body, blen);
    char sentence[sizeof(body) + 8];
    int slen = snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, chk);
    if (slen > 0) {
      Serial_Print(sentence);
    }
  }
}
