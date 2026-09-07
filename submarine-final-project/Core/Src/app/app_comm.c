#include "app_comm.h"
#include <string.h>

#define COMM_QUEUE_LEN 8
#define COMM_POLL_STEP_MS 20u

static osMessageQueueId_t s_queues[APP_COMM_PRIO_COUNT];
static const osMessageQueueAttr_t s_queue_attrs[APP_COMM_PRIO_COUNT] = {
    {.name = "CommOutboxHigh"},
    {.name = "CommOutboxMedium"},
    {.name = "CommOutboxLow"},
};

void AppComm_QueueCreate(void) {
  for (int p = 0; p < APP_COMM_PRIO_COUNT; p++) {
    s_queues[p] = osMessageQueueNew(COMM_QUEUE_LEN, sizeof(AppCommMsg_t),
                                     &s_queue_attrs[p]);
  }
}

void AppComm_Post(AppCommPriority_t priority, uint8_t msg_type,
                   const uint8_t *payload, uint16_t len) {
  if ((unsigned)priority >= APP_COMM_PRIO_COUNT || len > APP_COMM_PAYLOAD_LEN ||
      (len > 0 && !payload)) {
    return;
  }
  AppCommMsg_t msg;
  msg.msg_type = msg_type;
  msg.payload_len = len;
  if (len > 0) {
    memcpy(msg.payload, payload, len);
  }
  osMessageQueuePut(s_queues[priority], &msg, 0, 0);
}

osStatus_t AppComm_Wait(AppCommMsg_t *out, uint32_t timeout_ms) {
  uint32_t waited = 0;

  for (;;) {
    for (int p = 0; p < APP_COMM_PRIO_COUNT; p++) {
      if (osMessageQueueGet(s_queues[p], out, NULL, 0) == osOK) {
        return osOK;
      }
    }

    if (timeout_ms != osWaitForever && waited >= timeout_ms) {
      return osErrorTimeout;
    }

    uint32_t step = COMM_POLL_STEP_MS;
    if (timeout_ms != osWaitForever) {
      uint32_t remaining = timeout_ms - waited;
      if (step > remaining) {
        step = remaining;
      }
    }
    osDelay(step);
    waited += step;
  }
}
