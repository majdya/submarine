#include "gs_dashboard_api.h"

#include "json_util.h"

namespace ground_station {

std::string GsDashboardApi::relay(const std::string& command) {
  std::string response = client_.sendRequest(command);
  if (response.empty()) {
    json::ObjectBuilder b;
    b.strField("status", "error").strField("data", "Not connected to Central Computer: " + client_.errorMessage());
    return b.build();
  }
  return response;
}

std::string GsDashboardApi::listSubmarines() { return relay("LIST_SUBMARINES"); }

std::string GsDashboardApi::getLogs(const std::string& serial, const std::string& startYmd,
                                     const std::string& endYmd) {
  return relay("GET_LOGS," + startYmd + "," + endYmd + "," + serial);
}

std::string GsDashboardApi::getEvents(const std::string& serial, const std::string& startYmd,
                                       const std::string& endYmd) {
  return relay("GET_EVENTS," + startYmd + "," + endYmd + "," + serial);
}

std::string GsDashboardApi::summaryReport() { return relay("SUMMARY_REPORT"); }

}  // namespace ground_station
