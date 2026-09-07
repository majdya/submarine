#include "task_keepalive.h"
#include "app_comm.h"
#include "app_state.h"
#include "cmsis_os2.h"
#include "comm_frame.h"
#include "comm_tags.h"
#include "rtc_ds1307.h"
#include "tlv.h"

/* SW-FD-LNC-001's Keep-Alive module: sends a message to the Central
   Computer every 6 seconds, carrying the current timestamp, the latest
   measured data, and the current Operating Mode - as its own task (not
   folded into Task_Comm's periodic branch, which used to only carry a
   bare uptime tick) so its content and period are independent of
   whatever else is queued for sending. Posted at APP_COMM_PRIO_HIGH per
   the spec's outbound priority ordering (Keep-Alive highest). */
#define KEEPALIVE_PERIOD_MS 6000

void Task_KeepAlive(void *argument) {
  (void)argument;

  for (;;) {
    AppState_t state;
    AppState_Get(&state);

    RTC_DateTime_t dt = {0};
    uint8_t have_time = (RTC_GetDateTime(&dt) == HAL_OK);

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
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_MODE,
                 (uint8_t)state.mode);
    TLV_EncodeU32(payload, sizeof(payload), &offset, TAG_LIGHT_RAW,
                  state.light_raw);
    TLV_EncodeU32(payload, sizeof(payload), &offset, TAG_TEMP_ADC_RAW,
                  state.temp_raw);
    TLV_EncodeU32(payload, sizeof(payload), &offset, TAG_BATTERY_RAW,
                  state.battery_raw);
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_DHT_TEMP,
                 state.dht11_temp_int);
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_DHT_HUMIDITY,
                 state.dht11_humidity_int);
    TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_DHT_VALID,
                 state.dht11_valid);

    AppComm_Post(APP_COMM_PRIO_HIGH, MSG_TYPE_KEEPALIVE, payload,
                 (uint16_t)offset);

    osDelay(KEEPALIVE_PERIOD_MS);
  }
}
