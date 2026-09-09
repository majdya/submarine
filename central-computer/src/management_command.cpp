#include "management_command.h"

namespace submarine {

bool ManagementCommand::setLimits(uint8_t param, std::optional<int32_t> normalMin,
                                   std::optional<int32_t> normalMax,
                                   std::optional<int32_t> warningMin,
                                   std::optional<int32_t> warningMax,
                                   std::optional<bool> enabled, int timeoutMs) {
  proto::MessageBuilder b;
  b.u8(TAG_PARAM, param);
  if (normalMin) b.i32(TAG_NORMAL_MIN, *normalMin);
  if (normalMax) b.i32(TAG_NORMAL_MAX, *normalMax);
  if (warningMin) b.i32(TAG_WARNING_MIN, *warningMin);
  if (warningMax) b.i32(TAG_WARNING_MAX, *warningMax);
  if (enabled) b.u8(TAG_ENABLED, *enabled ? 1 : 0);

  auto reply = link_.sendAndWait(b.encode(MSG_TYPE_CMD_SET_LIMITS), timeoutMs);
  if (!reply || reply->type != MSG_TYPE_ACK) return false;
  auto status = reply->find(TAG_STATUS);
  return status && status->asU8() == STATUS_OK;
}

bool ManagementCommand::setTime(const TimeStamp& ts, int timeoutMs) {
  proto::MessageBuilder b;
  b.u16(TAG_TIMESTAMP_YEAR, ts.year)
      .u8(TAG_TIMESTAMP_MONTH, ts.month)
      .u8(TAG_TIMESTAMP_DAY, ts.day)
      .u8(TAG_TIMESTAMP_HOUR, ts.hour)
      .u8(TAG_TIMESTAMP_MIN, ts.minute)
      .u8(TAG_TIMESTAMP_SEC, ts.second);

  auto reply = link_.sendAndWait(b.encode(MSG_TYPE_CMD_SET_TIME), timeoutMs);
  if (!reply || reply->type != MSG_TYPE_ACK) return false;
  auto status = reply->find(TAG_STATUS);
  return status && status->asU8() == STATUS_OK;
}

std::optional<TimeStamp> ManagementCommand::getTime(int timeoutMs) {
  proto::MessageBuilder b;
  auto reply = link_.sendAndWait(b.encode(MSG_TYPE_CMD_GET_TIME), timeoutMs);
  if (!reply || reply->type != MSG_TYPE_DATA_REPORT) return std::nullopt;

  TimeStamp ts;
  auto year = reply->find(TAG_TIMESTAMP_YEAR);
  auto month = reply->find(TAG_TIMESTAMP_MONTH);
  auto day = reply->find(TAG_TIMESTAMP_DAY);
  auto hour = reply->find(TAG_TIMESTAMP_HOUR);
  auto minute = reply->find(TAG_TIMESTAMP_MIN);
  auto second = reply->find(TAG_TIMESTAMP_SEC);
  if (!year || !month || !day || !hour || !minute || !second) return std::nullopt;

  ts.year = year->asU16().value_or(0);
  ts.month = month->asU8().value_or(0);
  ts.day = day->asU8().value_or(0);
  ts.hour = hour->asU8().value_or(0);
  ts.minute = minute->asU8().value_or(0);
  ts.second = second->asU8().value_or(0);
  return ts;
}

bool ManagementCommand::getRange(uint8_t cmdType, uint32_t startYmd, uint32_t endYmd,
                                  const std::function<void(const std::string&)>& onLine,
                                  int overallTimeoutMs) {
  proto::MessageBuilder b;
  b.u32(TAG_RANGE_START_YMD, startYmd).u32(TAG_RANGE_END_YMD, endYmd);
  return link_.sendAndStream(
      b.encode(cmdType),
      [&](const proto::Message& msg) {
        if (auto line = msg.find(TAG_LOG_LINE)) onLine(line->asString());
      },
      /*perFrameTimeoutMs=*/500, overallTimeoutMs);
}

bool ManagementCommand::getData(uint32_t startYmd, uint32_t endYmd,
                                 const std::function<void(const std::string&)>& onLine,
                                 int overallTimeoutMs) {
  return getRange(MSG_TYPE_CMD_GET_DATA, startYmd, endYmd, onLine, overallTimeoutMs);
}

bool ManagementCommand::getEvents(uint32_t startYmd, uint32_t endYmd,
                                   const std::function<void(const std::string&)>& onLine,
                                   int overallTimeoutMs) {
  return getRange(MSG_TYPE_CMD_GET_EVENTS, startYmd, endYmd, onLine, overallTimeoutMs);
}

}  // namespace submarine
