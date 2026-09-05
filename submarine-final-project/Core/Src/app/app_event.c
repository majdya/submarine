#include "app_event.h"

static osMessageQueueId_t s_event_queue;
static const osMessageQueueAttr_t s_event_queue_attr = {
    .name = "EventQueue",
};

/* 8 pending events is generous for two button EXTI sources plus Monitor's
   threshold checks - if it ever fills, the event is dropped rather than
   blocking. */
#define EVENT_QUEUE_LEN 8

void AppEvent_QueueCreate(void) {
  s_event_queue = osMessageQueueNew(EVENT_QUEUE_LEN, sizeof(AppEvent_t),
                                     &s_event_queue_attr);
}

void AppEvent_Post(AppEventType_t type, uint32_t value) {
  AppEvent_t evt = {.type = type, .value = value};
  /* Timeout 0 (non-blocking) always - this is the one call in the queue
     API that's legal from both task and ISR context with the same
     argument, so a full queue drops the event instead of ever stalling
     an ISR (Object Detection/Silence post from HAL_GPIO_EXTI_Callback)
     or a task (Monitor's threshold checks). */
  osMessageQueuePut(s_event_queue, &evt, 0, 0);
}

osStatus_t AppEvent_Wait(AppEvent_t *out) {
  return osMessageQueueGet(s_event_queue, out, NULL, osWaitForever);
}
