#include "log_query.h"
#include "cmsis_os2.h"

static osMessageQueueId_t s_query_queue;
static const osMessageQueueAttr_t s_query_queue_attr = {
    .name = "LogQueryQueue",
};

void LogQuery_QueueCreate(void) {
  s_query_queue =
      osMessageQueueNew(1, sizeof(LogQueryRequest_t), &s_query_queue_attr);
}

void LogQuery_Request(const LogQueryRequest_t *req) {
  if (!req) {
    return;
  }
  osMessageQueuePut(s_query_queue, req, 0, 0);
}

uint8_t LogQuery_Poll(LogQueryRequest_t *out) {
  return osMessageQueueGet(s_query_queue, out, NULL, 0) == osOK;
}
