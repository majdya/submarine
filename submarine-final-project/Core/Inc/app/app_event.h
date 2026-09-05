#ifndef APP_EVENT_H
#define APP_EVENT_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EVENT_OBJECT_DETECTED = 1,
  EVENT_SILENCE_PRESSED = 2,
  EVENT_SENSOR_THRESHOLD = 3,
} AppEventType_t;

typedef struct {
  AppEventType_t type;
  uint32_t value; /* meaning depends on type, e.g. which threshold */
} AppEvent_t;

/* Must be called once from MX_FREERTOS_Init's RTOS_QUEUES section, before
   any task that posts or consumes events starts. */
void AppEvent_QueueCreate(void);

/* Safe to call from either task or ISR context (checked internally). */
void AppEvent_Post(AppEventType_t type, uint32_t value);

/* Event task only: blocks until an event arrives. */
osStatus_t AppEvent_Wait(AppEvent_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_H */
