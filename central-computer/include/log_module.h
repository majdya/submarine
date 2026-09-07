#ifndef CENTRAL_COMPUTER_LOG_MODULE_H
#define CENTRAL_COMPUTER_LOG_MODULE_H

#include <mutex>
#include <string>

#include "protocol_bridge.h"

namespace submarine {

// Central Computer's Log Module - spec section 3.3: "Prints logs and
// persists them to files." Mirrors the LNC firmware's own Log module
// (date-named files, 7-day rolling retention) using the exact same
// Howard-Hinnant day-count approach that was host-compiled and swept
// against 4018 calendar dates during the LNC firmware's own verification
// pass - see YmdToDayNumber in the .cpp for the port.
//
// Every KEEPALIVE and EVENT frame the LNC sends arrives here via
// CentralComputer's CommLink callback; this module only formats and
// writes, it does not interpret the data - that is DataCollection's job
// (data_collection.h).
class LogModule {
 public:
  // logDir: directory the date-named files are written into (created if
  // missing). today: YYYYMMDD, used for retention - passed in rather than
  // read from the system clock so tests can control it deterministically.
  explicit LogModule(std::string logDir);

  // Formats and writes one line for an incoming keep-alive or event
  // message, and prints the same line to stdout. now_ymd/now_hms let the
  // caller supply the reception timestamp (normally "now" on the PC) since
  // not all fields (e.g. TAG_STATUS-only ACKs) carry their own.
  void recordKeepAlive(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms);
  void recordEvent(const proto::Message& msg, uint32_t nowYmd, const std::string& nowHms);

  // Deletes any date-named log file older than 7 days relative to
  // todayYmd. Safe to call as often as desired (e.g. once per day, or once
  // at startup) - a no-op if nothing has aged out.
  void enforceRetention(uint32_t todayYmd);

  std::string logDir() const { return logDir_; }

 private:
  void writeLine(uint32_t ymd, const std::string& line);
  std::string formatMode(const proto::Message& msg) const;

  std::string logDir_;
  std::mutex mutex_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_LOG_MODULE_H
