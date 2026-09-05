#include "task_comm_rx.h"
#include "app_command.h"
#include "cmsis_os2.h"
#include "comm_frame.h"
#include "uart_rx.h"

/* Reassembles bytes from uart_rx.c's ring buffer into whole comm_frame.h
   messages, and hands each complete, checksum-valid one to
   app_command.c for interpretation. Pure plumbing - this task doesn't
   know what any command means. */
void Task_CommRx(void *argument) {
  (void)argument;

  CommFrameDecoder_t decoder;
  CommFrame_DecoderInit(&decoder);

  for (;;) {
    uint8_t byte;
    if (!UartRx_Pop(&byte, osWaitForever)) {
      continue;
    }
    if (CommFrame_DecoderFeed(&decoder, byte)) {
      AppCommand_Handle(decoder.type, decoder.payload, decoder.len);
    }
  }
}
