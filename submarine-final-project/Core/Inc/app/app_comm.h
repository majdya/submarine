#ifndef APP_COMM_H
#define APP_COMM_H

#include "cmsis_os2.h"
#include "comm_frame.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Derived from comm_frame.h rather than redefined, so the two can never
   drift apart - a message this module accepts is always exactly what
   CommFrame_Encode (called from task_comm.c) can frame. */
#define APP_COMM_PAYLOAD_LEN COMM_FRAME_MAX_PAYLOAD

typedef enum {
  APP_COMM_PRIO_HIGH = 0,   /* Keep-Alive */
  APP_COMM_PRIO_MEDIUM = 1, /* Events */
  APP_COMM_PRIO_LOW = 2,    /* Query replies (GET_TIME/GET_DATA/GET_EVENTS), ACKs */
  APP_COMM_PRIO_COUNT = 3,
} AppCommPriority_t;

typedef struct {
  uint8_t msg_type; /* one of comm_frame.h's MSG_TYPE_* values */
  uint16_t payload_len;
  uint8_t payload[APP_COMM_PAYLOAD_LEN]; /* raw bytes - may contain 0x00,
                                             not a C string */
} AppCommMsg_t;

/* Must be called once from MX_FREERTOS_Init's RTOS_QUEUES section, before
   any task that posts or consumes outbound messages starts. */
void AppComm_QueueCreate(void);

/* Non-blocking: drops the message if that priority's outbox is full, or
   if len exceeds APP_COMM_PAYLOAD_LEN. Spec (S2.4-ish, outbound priority
   ordering): Keep-Alive highest, Event medium, everything else low -
   pass the matching AppCommPriority_t. */
void AppComm_Post(AppCommPriority_t priority, uint8_t msg_type,
                   const uint8_t *payload, uint16_t len);

/* Communication task only: waits up to timeout_ms (or osWaitForever) for
   the next outbound message, checking APP_COMM_PRIO_HIGH down to
   APP_COMM_PRIO_LOW each pass. CMSIS-RTOS2 has no native "wait on
   several queues" primitive, so this polls all three every
   AppComm_PollStepMs while waiting - cheap (a handful of queue peeks
   every 20ms) and simple to reason about, versus wiring up FreeRTOS
   queue sets directly underneath the CMSIS wrapper for a marginal
   latency win nothing here needs. */
osStatus_t AppComm_Wait(AppCommMsg_t *out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_COMM_H */
