#ifndef APP_COMM_TAGS_H
#define APP_COMM_TAGS_H

/* TLV tags used inside comm_frame payloads - a shared vocabulary between
   Task_Event/Task_KeepAlive (producers), app_command.c (consumer +
   response producer), and whatever eventually parses this on the PC
   side. Values are arbitrary but fixed; add new tags at the end rather
   than renumbering existing ones. */

#define TAG_TIMESTAMP_YEAR 0x01u  /* u16 */
#define TAG_TIMESTAMP_MONTH 0x02u /* u8 */
#define TAG_TIMESTAMP_DAY 0x03u   /* u8 */
#define TAG_TIMESTAMP_HOUR 0x04u  /* u8 */
#define TAG_TIMESTAMP_MIN 0x05u   /* u8 */
#define TAG_TIMESTAMP_SEC 0x06u   /* u8 */

#define TAG_MODE 0x07u /* u8, AppMode_t */

#define TAG_LIGHT_RAW 0x08u    /* u32 */
#define TAG_TEMP_ADC_RAW 0x09u /* u32 */
#define TAG_BATTERY_RAW 0x0Au  /* u32 */
#define TAG_DHT_TEMP 0x0Bu     /* u8, degrees C */
#define TAG_DHT_HUMIDITY 0x0Cu /* u8, %RH */
#define TAG_DHT_VALID 0x0Du    /* u8, 0 or 1 */

#define TAG_EVENT_TYPE 0x0Eu  /* u8, AppEventType_t */
#define TAG_EVENT_VALUE 0x0Fu /* u32, meaning depends on TAG_EVENT_TYPE */

/* Management "set limits" command - PARAM selects which of the four
   Monitor parameters this applies to; NORMAL_MAX/WARNING_MAX are only
   present for PARAM_TEMP (the other three are lower-bound-only, per
   app_config.h). */
#define TAG_PARAM 0x10u       /* u8, see the PARAM_* values below */
#define TAG_NORMAL_MIN 0x11u  /* i32 */
#define TAG_NORMAL_MAX 0x12u  /* i32, PARAM_TEMP only */
#define TAG_WARNING_MIN 0x13u /* i32 */
#define TAG_WARNING_MAX 0x14u /* i32, PARAM_TEMP only */

#define PARAM_TEMP 0u
#define PARAM_HUMIDITY 1u
#define PARAM_LIGHT 2u
#define PARAM_BATTERY 3u

#define TAG_STATUS 0x15u /* u8, see STATUS_* values below - MSG_TYPE_ACK */
#define STATUS_OK 0u
#define STATUS_ERROR 1u

/* GET_DATA / GET_EVENTS query range, inclusive, as YYYYMMDD integers
   (e.g. 20260905) - matches task_log.c's date-named log files exactly,
   so no separate date encoding is needed. */
#define TAG_RANGE_START_YMD 0x16u /* u32 */
#define TAG_RANGE_END_YMD 0x17u   /* u32 */

/* One raw log-file line (as already formatted by Task_Event/task_log.c),
   returned verbatim rather than re-parsed into numeric fields - see
   app_command.c's HandleGetData/HandleGetEvents for why. */
#define TAG_LOG_LINE 0x18u /* raw bytes, not NUL-terminated */

#endif /* APP_COMM_TAGS_H */
