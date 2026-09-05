#ifndef APP_LOG_QUERY_H
#define APP_LOG_QUERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A request to replay historical log lines back over the comm link, by
   calendar-date range - see app_command.c (GET_DATA/GET_EVENTS) for the
   producer and task_log.c for the consumer. This lives in its own tiny
   module rather than inside app_command.c or task_log.c directly because
   FatFs in this project is built without FF_FS_REENTRANT (ffconf.h) -
   only Task_Log is allowed to touch the filesystem, so a query "request"
   has to cross a queue to reach it exactly like a fs write does, rather
   than being called directly from whichever task received the command. */
typedef struct {
  uint32_t start_ymd; /* inclusive, YYYYMMDD */
  uint32_t end_ymd;   /* inclusive, YYYYMMDD */
} LogQueryRequest_t;

/* Must be called once from MX_FREERTOS_Init's RTOS_QUEUES section. */
void LogQuery_QueueCreate(void);

/* Non-blocking. If a query is already pending (queue depth is 1 - only
   one can be serviced at a time), this one is silently dropped; the
   Central Computer sees no reply and can simply ask again. */
void LogQuery_Request(const LogQueryRequest_t *req);

/* Task_Log only: non-blocking poll for a pending request. Returns 1 and
   fills *out if one was waiting, 0 otherwise. */
uint8_t LogQuery_Poll(LogQueryRequest_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_LOG_QUERY_H */
