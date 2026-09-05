#include "task_event.h"
#include "app_comm.h"
#include "app_event.h"
#include "app_log.h"
#include "app_state.h"
#include "rtc_ds1307.h"
#include <stdio.h>

/* Comm messages stay short placeholder text for the (not-yet-designed)
   PC-side protocol - AppCommMsg_t's buffer is unchanged. The log line is
   the one that needs real detail (timestamp + sensor snapshot), since
   that's what actually lands in LOG.TXT and in the serial trace people
   read for diagnosis. */
static void FormatEventName(AppEventType_t type, uint32_t value, char *out,
                             size_t out_len) {
  switch (type) {
    case EVENT_OBJECT_DETECTED:
      snprintf(out, out_len, "EVENT object_detected");
      break;
    case EVENT_SILENCE_PRESSED:
      snprintf(out, out_len, "EVENT silence_pressed");
      break;
    case EVENT_SENSOR_THRESHOLD:
      snprintf(out, out_len, "EVENT threshold value=%lu", (unsigned long)value);
      break;
    default:
      snprintf(out, out_len, "EVENT unknown type=%d", (int)type);
      break;
  }
}

/* Event is a pure dispatcher: it decides what an event means, and hands
   the "what to do about it" off to Log (always) and Comm (when it's worth
   telling the PC side). It does no hardware I/O itself - AppState_Get()
   and RTC_GetDateTime() are just reads of data other tasks already
   gathered. */
void Task_Event(void *argument) {
  (void)argument;
  AppEvent_t evt;

  for (;;) {
    if (AppEvent_Wait(&evt) != osOK) {
      continue;
    }

    char name[40];
    FormatEventName(evt.type, evt.value, name, sizeof(name));

    char comm_line[APP_COMM_MSG_LEN];
    snprintf(comm_line, sizeof(comm_line), "%s", name);
    AppComm_Post(comm_line);

    RTC_DateTime_t dt;
    uint8_t have_time = (RTC_GetDateTime(&dt) == HAL_OK);

    AppState_t state;
    AppState_Get(&state);

    char log_line[APP_LOG_MSG_LEN];
    if (have_time) {
      snprintf(log_line, sizeof(log_line),
               "%04u-%02u-%02u %02u:%02u:%02u %s light=%lu tempADC=%lu "
               "batt=%lu humADC=%lu dht=%u/%u%s",
               dt.year, dt.month, dt.day, dt.hour, dt.min, dt.sec, name,
               (unsigned long)state.light_raw, (unsigned long)state.temp_raw,
               (unsigned long)state.battery_raw,
               (unsigned long)state.humidity_adc_raw, state.dht11_temp_int,
               state.dht11_humidity_int, state.dht11_valid ? "" : "(invalid)");
    } else {
      snprintf(log_line, sizeof(log_line),
               "[no-RTC] %s light=%lu tempADC=%lu batt=%lu humADC=%lu "
               "dht=%u/%u%s",
               name, (unsigned long)state.light_raw,
               (unsigned long)state.temp_raw, (unsigned long)state.battery_raw,
               (unsigned long)state.humidity_adc_raw, state.dht11_temp_int,
               state.dht11_humidity_int, state.dht11_valid ? "" : "(invalid)");
    }
    AppLog_Post(log_line);
  }
}
