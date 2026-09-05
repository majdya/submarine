#ifndef APP_COMM_H
#define APP_COMM_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_COMM_MSG_LEN 80

typedef struct {
  char text[APP_COMM_MSG_LEN];
} AppCommMsg_t;

/* Must be called once from MX_FREERTOS_Init's RTOS_QUEUES section, before
   any task that posts or consumes outbound messages starts. */
void AppComm_QueueCreate(void);

/* Non-blocking: drops the message if the outbox is full. Truncates to
   APP_COMM_MSG_LEN-1 chars. Called by Event to forward things worth
   telling the PC side about. */
void AppComm_Post(const char *text);

/* Communication task only: waits up to timeout_ms for an outbound
   message (returns osErrorTimeout if none - Communication still needs to
   run its own periodic heartbeat even with nothing queued). */
osStatus_t AppComm_Wait(AppCommMsg_t *out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_H */
