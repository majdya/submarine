#ifndef APP_LOG_H
#define APP_LOG_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_LOG_MSG_LEN 144

typedef struct {
  char text[APP_LOG_MSG_LEN];
} AppLogMsg_t;

/* Must be called once from MX_FREERTOS_Init's RTOS_QUEUES section, before
   any task that posts or consumes log messages starts. */
void AppLog_QueueCreate(void);

/* Non-blocking: drops the message if the log queue is full rather than
   ever stalling the caller. Truncates to APP_LOG_MSG_LEN-1 chars. */
void AppLog_Post(const char *text);

/* Log task only: blocks up to timeout_ms (or osWaitForever) for a
   message - a finite timeout lets Task_Log also poll LogQuery_Poll()
   in between, since CMSIS-RTOS2 has no "wait on either of two queues"
   primitive. */
osStatus_t AppLog_Wait(AppLogMsg_t *out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_LOG_H */
