#ifndef APP_COMMAND_H
#define APP_COMMAND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Interprets one fully-decoded incoming comm_frame.h message and acts on
   it - Task_CommRx's job stops at framing; command *meaning* lives here.
   Runs on whatever task calls it (currently always Task_CommRx) - every
   call this makes (AppConfig_Set, RTC_SetDateTime, LogQuery_Request,
   AppComm_Post) is safe from ordinary task context, nothing here is
   ISR-restricted. */
void AppCommand_Handle(uint8_t msg_type, const uint8_t *payload,
                        uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMMAND_H */
