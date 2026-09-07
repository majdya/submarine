#include "app_command.h"
#include "app_comm.h"
#include "app_config.h"
#include "comm_frame.h"
#include "comm_tags.h"
#include "log_query.h"
#include "rtc_ds1307.h"
#include "tlv.h"
#include <stddef.h>

/* Management Commands + instructions (SW-FD-LNC-001's Communication
   module, incoming side). Ten-ish commands from the spec collapse into
   five message types here: SET_LIMITS is one generic "set this
   parameter's Normal/Warning bounds" command carrying a TAG_PARAM
   selector rather than eight near-identical hardcoded ones (one per
   parameter x bound) - same functional coverage, less duplicated code. */

static void SendAck(uint8_t status) {
  uint8_t payload[8];
  size_t offset = 0;
  TLV_EncodeU8(payload, sizeof(payload), &offset, TAG_STATUS, status);
  AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_ACK, payload, (uint16_t)offset);
}

static void HandleSetLimits(const uint8_t *payload, uint16_t len) {
  /* Decode every present field first - a command is only ever applied
     as a whole, never partially, so a malformed/incomplete one can't
     leave AppConfig_t half-updated. */
  size_t offset = 0;
  TLV_Field_t field;
  uint8_t have_param = 0, param = 0;
  uint8_t have_normal_min = 0, have_normal_max = 0;
  uint8_t have_warning_min = 0, have_warning_max = 0;
  int32_t normal_min = 0, normal_max = 0, warning_min = 0, warning_max = 0;

  while (TLV_Decode(payload, len, &offset, &field)) {
    switch (field.tag) {
      case TAG_PARAM:
        have_param = TLV_FieldAsU8(&field, &param);
        break;
      case TAG_NORMAL_MIN:
        have_normal_min = TLV_FieldAsI32(&field, &normal_min);
        break;
      case TAG_NORMAL_MAX:
        have_normal_max = TLV_FieldAsI32(&field, &normal_max);
        break;
      case TAG_WARNING_MIN:
        have_warning_min = TLV_FieldAsI32(&field, &warning_min);
        break;
      case TAG_WARNING_MAX:
        have_warning_max = TLV_FieldAsI32(&field, &warning_max);
        break;
      default:
        break; /* unknown tag - ignore, keeps this forward-compatible */
    }
  }

  if (!have_param || !have_normal_min || !have_warning_min) {
    SendAck(STATUS_ERROR);
    return;
  }
  if (param == PARAM_TEMP && (!have_normal_max || !have_warning_max)) {
    SendAck(STATUS_ERROR); /* temp needs both ends of both ranges */
    return;
  }

  AppConfig_t cfg;
  AppConfig_Get(&cfg);
  switch (param) {
    case PARAM_TEMP:
      cfg.temp_normal_min = normal_min;
      cfg.temp_normal_max = normal_max;
      cfg.temp_warning_min = warning_min;
      cfg.temp_warning_max = warning_max;
      break;
    case PARAM_HUMIDITY:
      cfg.humidity_normal_lower = (uint32_t)normal_min;
      cfg.humidity_warning_lower = (uint32_t)warning_min;
      break;
    case PARAM_LIGHT:
      cfg.light_normal_lower = (uint32_t)normal_min;
      cfg.light_warning_lower = (uint32_t)warning_min;
      break;
    case PARAM_BATTERY:
      cfg.battery_normal_lower = (uint32_t)normal_min;
      cfg.battery_warning_lower = (uint32_t)warning_min;
      break;
    default:
      SendAck(STATUS_ERROR);
      return;
  }

  AppConfig_Set(&cfg); /* also persists to flash - see app_config.c */
  SendAck(STATUS_OK);
}

static void HandleSetTime(const uint8_t *payload, uint16_t len) {
  size_t offset = 0;
  TLV_Field_t field;
  RTC_DateTime_t dt = {0};
  uint8_t have_year = 0, have_month = 0, have_day = 0;
  uint8_t have_hour = 0, have_min = 0, have_sec = 0;
  uint8_t v;

  while (TLV_Decode(payload, len, &offset, &field)) {
    switch (field.tag) {
      case TAG_TIMESTAMP_YEAR:
        have_year = TLV_FieldAsU16(&field, &dt.year);
        break;
      case TAG_TIMESTAMP_MONTH:
        if ((have_month = TLV_FieldAsU8(&field, &v))) {
          dt.month = v;
        }
        break;
      case TAG_TIMESTAMP_DAY:
        if ((have_day = TLV_FieldAsU8(&field, &v))) {
          dt.day = v;
        }
        break;
      case TAG_TIMESTAMP_HOUR:
        if ((have_hour = TLV_FieldAsU8(&field, &v))) {
          dt.hour = v;
        }
        break;
      case TAG_TIMESTAMP_MIN:
        if ((have_min = TLV_FieldAsU8(&field, &v))) {
          dt.min = v;
        }
        break;
      case TAG_TIMESTAMP_SEC:
        if ((have_sec = TLV_FieldAsU8(&field, &v))) {
          dt.sec = v;
        }
        break;
      default:
        break;
    }
  }

  if (!(have_year && have_month && have_day && have_hour && have_min &&
        have_sec)) {
    SendAck(STATUS_ERROR);
    return;
  }

  SendAck(RTC_SetDateTime(&dt) == HAL_OK ? STATUS_OK : STATUS_ERROR);
}

static void HandleGetTime(void) {
  RTC_DateTime_t dt;
  uint8_t payload[32];
  size_t offset = 0;

  if (RTC_GetDateTime(&dt) == HAL_OK) {
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
  /* offset stays 0 (an empty DATA_REPORT) if the RTC read failed - still
     a well-formed reply, just "no data", rather than silence. */
  AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_DATA_REPORT, payload,
               (uint16_t)offset);
  /* End-of-results marker, so GET_TIME's reply shape matches
     GET_DATA/GET_EVENTS (one or more DATA_REPORT frames, terminated by
     an empty one) instead of needing a special case on the receiver. */
  AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_DATA_REPORT, NULL, 0);
}

/* GET_DATA and GET_EVENTS both land here - see log_query.h's comment and
   task_log.c's HandleLogQuery for why they currently return identical
   content (there is no separate continuous measurement log yet, only
   Task_Event's daily event lines). */
static void HandleGetDataOrEvents(const uint8_t *payload, uint16_t len) {
  size_t offset = 0;
  TLV_Field_t field;
  uint8_t have_start = 0, have_end = 0;
  uint32_t start_ymd = 0, end_ymd = 0;

  while (TLV_Decode(payload, len, &offset, &field)) {
    switch (field.tag) {
      case TAG_RANGE_START_YMD:
        have_start = TLV_FieldAsU32(&field, &start_ymd);
        break;
      case TAG_RANGE_END_YMD:
        have_end = TLV_FieldAsU32(&field, &end_ymd);
        break;
      default:
        break;
    }
  }

  if (!have_start || !have_end || start_ymd > end_ymd) {
    /* Malformed range - reply with 0 results rather than staying
       silent, matching HandleLogQuery's own "nothing matched" shape. */
    AppComm_Post(APP_COMM_PRIO_LOW, MSG_TYPE_DATA_REPORT, NULL, 0);
    return;
  }

  LogQueryRequest_t req = {.start_ymd = start_ymd, .end_ymd = end_ymd};
  LogQuery_Request(&req);
}

void AppCommand_Handle(uint8_t msg_type, const uint8_t *payload,
                        uint16_t len) {
  switch (msg_type) {
    case MSG_TYPE_CMD_SET_LIMITS:
      HandleSetLimits(payload, len);
      break;
    case MSG_TYPE_CMD_SET_TIME:
      HandleSetTime(payload, len);
      break;
    case MSG_TYPE_CMD_GET_TIME:
      (void)payload;
      (void)len;
      HandleGetTime();
      break;
    case MSG_TYPE_CMD_GET_DATA:
    case MSG_TYPE_CMD_GET_EVENTS:
      HandleGetDataOrEvents(payload, len);
      break;
    default:
      break; /* unknown, or one of our own outgoing-only types looped
                back somehow - ignore rather than guess */
  }
}
