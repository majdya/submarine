#include "app_comm.h"
#include <string.h>

static osMessageQueueId_t s_comm_queue;
static const osMessageQueueAttr_t s_comm_queue_attr = {
    .name = "CommOutbox",
};

#define COMM_QUEUE_LEN 8

void AppComm_QueueCreate(void) {
  s_comm_queue = osMessageQueueNew(COMM_QUEUE_LEN, sizeof(AppCommMsg_t),
                                    &s_comm_queue_attr);
}

void AppComm_Post(const char *text) {
  AppCommMsg_t msg;
  strncpy(msg.text, text, APP_COMM_MSG_LEN - 1);
  msg.text[APP_COMM_MSG_LEN - 1] = '\0';
  osMessageQueuePut(s_comm_queue, &msg, 0, 0);
}

osStatus_t AppComm_Wait(AppCommMsg_t *out, uint32_t timeout_ms) {
  return osMessageQueueGet(s_comm_queue, out, NULL, timeout_ms);
}
