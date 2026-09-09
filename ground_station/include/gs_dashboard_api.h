#ifndef GROUND_STATION_GS_DASHBOARD_API_H
#define GROUND_STATION_GS_DASHBOARD_API_H

#include <string>

#include "tcp_client.h"

namespace ground_station {

// Backs the read-only web dashboard's HTTP routes (see main.cpp) - a thin
// relay, not a second implementation of anything. Every method just turns
// an HTTP request into the exact same wire command the console CLI itself
// sends (see ground_station_cli.cpp / tcp_client.h's protocol comment),
// and the Central Computer's TcpApi already replies with well-formed
// {"status":...,"data":...} JSON (see central-computer/src/tcp_api.cpp) -
// so a successful call's result is returned to the browser completely
// unmodified. This class only adds a JSON error shape for the one new
// failure mode an HTTP caller can hit that the CLI's own error handling
// doesn't need to format as JSON (a dropped/never-made connection).
//
// spec §4: "The ground station can request from the submarine's central
// computer data stored over a period of time... both the data stored in
// a log and data of the events." This dashboard is a second, browser-based
// way to make those exact same read-only requests - not a new capability.
class GsDashboardApi {
 public:
  explicit GsDashboardApi(TcpClient& client) : client_(client) {}

  std::string listSubmarines();
  std::string getLogs(const std::string& serial, const std::string& startYmd, const std::string& endYmd);
  std::string getEvents(const std::string& serial, const std::string& startYmd, const std::string& endYmd);
  std::string summaryReport();

 private:
  std::string relay(const std::string& command);
  TcpClient& client_;
};

}  // namespace ground_station

#endif  // GROUND_STATION_GS_DASHBOARD_API_H
