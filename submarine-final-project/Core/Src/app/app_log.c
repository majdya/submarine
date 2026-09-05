#include "app_log.h"
#include <string.h>

static osMessageQueueId_t s_log_queue;
static const osMessageQueueAttr_t s_log_queue_attr = {
    .name = "LogQueue",
};

#define LOG_QUEUE_LEN 8

void AppLog_QueueCreate(void) {
  s_log_queue =
      osMessageQueueNew(LOG_QUEUE_LEN, sizeof(AppLogMsg_t), &s_log_queue_attr);
}

void AppLog_Post(const char *text) {
  AppLogMsg_t msg;
  strncpy(msg.text, text, APP_LOG_MSG_LEN - 1);
  msg.text[APP_LOG_MSG_LEN - 1] = '\0';
  osMessageQueuePut(s_log_queue, &msg, 0, 0);
}

osStatus_t AppLog_Wait(AppLogMsg_t *out, uint32_t timeout_ms) {
  return osMessageQueueGet(s_log_queue, out, NULL, timeout_ms);
}
