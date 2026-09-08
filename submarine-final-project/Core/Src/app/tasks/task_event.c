#include "task_event.h"
#include "app_comm.h"
#include "app_event.h"
#include "app_log.h"
#include "app_state.h"
#include "comm_frame.h"
#include "comm_tags.h"
#include "indicators.h"
#include "rtc_ds1307.h"
#include "tlv.h"
#include <stdio.h>

/* RGB/buzzer duty range is 0-999 (TIM3's ARR - see indicators.h). */
#define RGB_MAX 999u
#define ALARM_DUTY 500u /* audible but not full-scale */

static void SetRgbGreen(void) { RGB_SetDuty(0, 0, RGB_MAX); }
static void SetRgbYellow(void) { RGB_SetDuty(RGB_MAX, 0, ALARM_DUTY); }
static void SetRgbRed(void) { RGB_SetDuty(RGB_MAX, 0, 0); }

/* Alarm state is owned entirely by this task - it is the only place that
   calls Buzzer_SetDuty, so a plain static (no mutex) is safe. */
static uint8_t s_alarm_active = 0;

static void AlarmOn(void) {
  s_alarm_active = 1;
  Buzzer_SetDuty(ALARM_DUTY);
}

static void AlarmOff(void) {
  s_alarm_active = 0;
  Buzzer_SetDuty(0);
}

/* Human-readable form of an event, for the daily log file (YYYYMMDD.TXT
   - see task_log.c) and the diagnostic serial trace. The wire-protocol
   form Task_Comm actually sends is built directly as TLV in Task_Event
   below (TAG_EVENT_TYPE/TAG_EVENT_VALUE, see comm_tags.h) - there is no
   separate text-field formatter for it any more. */
static void FormatEventName(AppEventType_t type, uint32_t value, char *out,
                            size_t out_len) {
  switch (type) {
  case EVENT_OBJECT_DETECTED:
    snprintf(out, out_len, "EVENT object_detected");
    break;
  case EVENT_OBJECT_CLEARED:
    snprintf(out, out_len, "EVENT object_cleared");
    break;
  case EVENT_SILENCE_PRESSED:
    snprintf(out, out_len, "EVENT silence_pressed");
    break;
  case EVENT_MODE_CHANGED:
    snprintf(out, out_len, "EVENT mode_changed %s->%s",
             AppMode_Name((AppMode_t)APP_EVENT_MODE_CHANGE_OLD(value)),
             AppMode_Name((AppMode_t)APP_EVENT_MODE_CHANGE_NEW(value)));
    break;
  default:
    snprintf(out, out_len, "EVENT unknown type=%d", (int)type);
    break;
  }
}

/* RGB/alarm side effects, per SW-FD-LNC-001:
   - S2.3.1 (Monitor-sourced mode transitions): Normal->Warning and
     Error->Warning both show yellow (the latter also stops the alarm and
     "resumes full operation"); Normal->Error and Warning->Error both show
     red + alarm on ("suppress non-essential operation" - that half is a
     Communication-module concern: see app_comm.h's priority queues,
     which at least ensure Keep-Alive/Events are never starved by a data
     report, though nothing here actually drops low-priority traffic
     during an Error mode yet); Warning->Normal and Error->Normal both
     show green (the latter also stops the alarm).
   - S2.3.4 (Object Detection): detected = red + alarm on; cleared = green
     + stop the alarm if it was active.
   Object Detection and Monitor both drive the same physical RGB LED
   independently with no cross-module arbitration defined in the spec
   (its flowchart treats them as parallel branches) - this is a
   deliberate, documented simplification: whichever event arrives last
   wins the LED, matching a literal reading of the spec rather than
   inventing an unspecified priority scheme. */
static void HandleModeChanged(uint32_t value) {
  AppMode_t new_mode = (AppMode_t)APP_EVENT_MODE_CHANGE_NEW(value);

  switch (new_mode) {
  case APP_MODE_NORMAL:
    SetRgbGreen();
    AlarmOff();
    break;
  case APP_MODE_WARNING:
    SetRgbYellow();
    AlarmOff();
    break;
  case APP_MODE_ERROR:
    SetRgbRed();
    AlarmOn();
    break;
  }
}

static void HandleObjectDetected(void) {
  SetRgbRed();
  AlarmOn();
}

static void HandleObjectCleared(void) {
  SetRgbGreen();
  if (s_alarm_active) {
    AlarmOff();
  }
}

static void HandleSilencePressed(void) {
  /* Silence stops the alarm only - it does not touch the RGB LED or the
     mode/object state that caused the alarm (spec: "pressing the button
     stops the alarm", nothing more). */
  if (s_alarm_active) {
    AlarmOff();
  }
}

/* Event is a dispatcher: it decides what an event means, drives the
   RGB/buzzer side effects above, and hands the "record this" duty off to
   Log (always) and Comm (when it's worth telling the PC side).
   AppState_Get() and RTC_GetDateTime() are just reads of data other
   tasks already gathered. */
void Task_Event(void *argument) {
  (void)argument;
  AppEvent_t evt;

  for (;;) {
    if (AppEvent_Wait(&evt) != osOK) {
      continue;
    }

    switch (evt.type) {
    case EVENT_MODE_CHANGED:
      HandleModeChanged(evt.value);
      break;
    case EVENT_OBJECT_DETECTED:
      HandleObjectDetected();
      break;
    case EVENT_OBJECT_CLEARED:
      HandleObjectCleared();
      break;
    case EVENT_SILENCE_PRESSED:
      HandleSilencePressed();
      break;
    default:
      break;
    }

    RTC_DateTime_t dt = {0};
    uint8_t have_time = (RTC_GetDateTime(&dt) == HAL_OK);

    AppState_t state;
    AppState_Get(&state);

    char name[48];
    FormatEventName(evt.type, evt.value, name, sizeof(name));

    /* TLV payload for the PC side: timestamp (if the RTC is up), which
       event fired and its value, and the mode in effect when it fired. */
    uint8_t payload[COMM_FRAME_MAX_PAYLOAD];
    size_t offset = 0;
    if (have_time) {
      TLV_EncodeU16(payload, sizeof(payload), &offset, TAG_TIMESTAMP_YEAR,
                    dt.year);
      TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_TIMESTAMP_MONTH,
                   dt.month);
      TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_TIMESTAMP_DAY,
                   dt.day);
      TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_TIMESTAMP_HOUR,
                   dt.hour);
      TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_TIMESTAMP_MIN,
                   dt.min);
      TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_TIMESTAMP_SEC,
                   dt.sec);
    }
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_EVENT_TYPE,
                 (uint8_t)evt.type);
    TLV_EncodeU32(payload, sizeof(payload), &offset, TAG_EVENT_VALUE,
                  evt.value);
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_MODE,
                 (uint8_t)state.mode);
    AppComm_Post(APP_COMM_PRIO_MEDIUM, MSG_TYPE_EVENT, payload,
                 (uint16_t)offset);

    char log_line[APP_LOG_MSG_LEN];
    if (have_time) {
      snprintf(log_line, sizeof(log_line),
               "%04u-%02u-%02u %02u:%02u:%02u %s light=%lu tempADC=%lu "
               "batt=%lu dht=%u/%u%s mode=%s",
               dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, name,
               (unsigned long)state.light_raw, (unsigned long)state.temp_raw,
               (unsigned long)state.battery_raw, state.dht11_temp_int,
               state.dht11_humidity_int, state.dht11_valid ? "" : "(invalid)",
               AppMode_Name(state.mode));
    } else {
      snprintf(log_line, sizeof(log_line),
               "[no-RTC] %s light=%lu tempADC=%lu batt=%lu dht=%u/%u%s mode=%s",
               name, (unsigned long)state.light_raw,
               (unsigned long)state.temp_raw, (unsigned long)state.battery_raw,
               state.dht11_temp_int, state.dht11_humidity_int,
               state.dht11_valid ? "" : "(invalid)", AppMode_Name(state.mode));
    }
    AppLog_Post(log_line);
  }
}
