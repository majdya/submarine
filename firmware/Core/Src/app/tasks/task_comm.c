#include "task_comm.h"
#include "app_comm.h"
#include "app_serial.h"
#include "comm_frame.h"
#include "cmsis_os2.h"
#include <stdint.h>

/* Task_Comm is a pure drain-and-send loop: it owns none of the outbound
   message content (Task_Event, Task_KeepAlive, and app_command.c's query
   replies each build their own TLV payload and post it via
   AppComm_Post), it just pulls the next message in priority order
   (AppComm_Wait already implements Keep-Alive > Event > everything else)
   and puts it on the wire framed per comm_frame.h. */
void Task_Comm(void *argument) {
  (void)argument;
  AppCommMsg_t msg;

  for (;;) {
    if (AppComm_Wait(&msg, osWaitForever) != osOK) {
      continue;
    }

    uint8_t frame[COMM_FRAME_MAX_ENCODED];
    size_t frame_len = CommFrame_Encode(frame, sizeof(frame), msg.msg_type,
                                         msg.payload, msg.payload_len);
    if (frame_len > 0) {
      Serial_Write(frame, frame_len);
    }
  }
}
