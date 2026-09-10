#ifndef APP_EVENT_H
#define APP_EVENT_H

#include "cmsis_os2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  EVENT_OBJECT_DETECTED = 1, /* Object Detection module: object entered range */
  EVENT_OBJECT_CLEARED = 2,  /* Object Detection module: object left range */
  EVENT_SILENCE_PRESSED = 3, /* Silence button (D2/PA10): stop the alarm */
  /* Monitor module's Operating Mode changed. value packs both the old and
     new AppMode_t so the Event module can react per the spec's five
     defined transitions (S2.3.1): high byte = old mode, low byte = new
     mode. Only ever posted on an actual change, never every poll. */
  EVENT_MODE_CHANGED = 4,
} AppEventType_t;

/* Packs old/new AppMode_t values into an EVENT_MODE_CHANGED value. */
#define APP_EVENT_PACK_MODE_CHANGE(old_mode, new_mode) \
  (((uint32_t)(old_mode) << 8) | (uint32_t)(new_mode))
#define APP_EVENT_MODE_CHANGE_OLD(value) ((uint8_t)(((value) >> 8) & 0xFFu))
#define APP_EVENT_MODE_CHANGE_NEW(value) ((uint8_t)((value)&0xFFu))

typedef struct {
  AppEventType_t type;
  uint32_t value; /* meaning depends on type - see AppEventType_t comments */
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
