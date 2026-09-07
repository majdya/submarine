#ifndef CENTRAL_COMPUTER_MANAGEMENT_COMMAND_H
#define CENTRAL_COMPUTER_MANAGEMENT_COMMAND_H

#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "comm_link.h"

namespace submarine {

struct TimeStamp {
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

// Spec section 3.2, Management Command Module: "Implements Management
// commands to be sent to the LNC end unit via the Communication module.
// Handles command processing." Covers the 8 SET_LIMITS-family commands
// from LNC spec section 2.5 (consolidated, same as the firmware side, into
// one generic per-parameter command rather than 8 near-duplicate ones),
// SET_TIME/GET_TIME, and the two retrieval instructions (GET_DATA /
// GET_EVENTS) - grouped here rather than in a separate module since the
// spec itself describes them as "instructions the Communication module
// receives from the Central Computer", i.e. commands this module issues.
class ManagementCommand {
 public:
  explicit ManagementCommand(CommLink& link) : link_(link) {}

  // param: PARAM_TEMP/PARAM_HUMIDITY/PARAM_LIGHT/PARAM_BATTERY (comm_tags.h).
  // normalMax/warningMax only apply to PARAM_TEMP (see comm_tags.h) - pass
  // std::nullopt for the other three parameters.
  bool setLimits(uint8_t param, std::optional<int32_t> normalMin, std::optional<int32_t> normalMax,
                 std::optional<int32_t> warningMin, std::optional<int32_t> warningMax,
                 int timeoutMs = 2000);

  bool setTime(const TimeStamp& ts, int timeoutMs = 2000);
  std::optional<TimeStamp> getTime(int timeoutMs = 2000);

  // One callback per returned raw log line (TAG_LOG_LINE), streamed as they
  // arrive rather than buffered - see CommLink::sendAndStream.
  bool getData(uint32_t startYmd, uint32_t endYmd,
               const std::function<void(const std::string&)>& onLine, int overallTimeoutMs = 5000);
  bool getEvents(uint32_t startYmd, uint32_t endYmd,
                 const std::function<void(const std::string&)>& onLine, int overallTimeoutMs = 5000);

 private:
  bool getRange(uint8_t cmdType, uint32_t startYmd, uint32_t endYmd,
                const std::function<void(const std::string&)>& onLine, int overallTimeoutMs);
  CommLink& link_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_MANAGEMENT_COMMAND_H
