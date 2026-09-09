#include "tcp_api.h"
#include "json_util.h"
#include <sstream>

namespace submarine {

TcpApi::TcpApi(Menu& menu, std::mutex& fleetMutex)
    : menu_(menu), fleetMutex_(fleetMutex) {}

std::string TcpApi::listSubmarines() {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  const auto& subs = menu_.allSubmarines();

  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"data\":[";
  bool first = true;
  for (const auto& sub : subs) {
    if (!first) ss << ",";
    first = false;

    ss << "{\"serial\":\"" << json::escape(sub->serialNumber())
       << "\",\"type\":\"" << sub->typeName()
       << "\",\"name\":\"" << json::escape(sub->name())
       << "\",\"missionAssigned\":" << (sub->isAssignedToMission() ? "true" : "false");

    if (sub->isAssignedToMission()) {
      const auto& mission = sub->currentMission();
      ss << ",\"missionDescription\":\"" << json::escape(mission->description()) << "\"";
    } else {
      ss << ",\"missionDescription\":\"\"";
    }

    // Every submarine - research or combat - has its own CentralComputer
    // (see submarine.h's class comment on this deliberate spec deviation).
    bool connected = sub->centralComputer().isConnected();
    ss << ",\"connected\":" << (connected ? "true" : "false") << "}";
  }
  ss << "]}";

  return ss.str();
}

std::string TcpApi::getLogs(uint32_t startYmd, uint32_t endYmd, const std::string& serial) {
  std::lock_guard<std::mutex> lock(fleetMutex_);

  auto* sub = menu_.search(serial);
  if (!sub) {
    return TcpResponse::error("Submarine not found: " + serial).body;
  }

  auto records = sub->centralComputer().dataStore().queryMeasurements(startYmd, endYmd);

  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"data\":[";
  bool first = true;
  for (const auto& rec : records) {
    if (!first) ss << ",";
    first = false;
    ss << "{\"ymd\":" << rec.ymd
       << ",\"hms\":\"" << json::escape(rec.hms) << "\""
       << ",\"mode\":" << static_cast<int>(rec.mode)
       << ",\"lightRaw\":" << rec.lightRaw
       << ",\"tempAdcRaw\":" << rec.tempAdcRaw
       << ",\"batteryRaw\":" << rec.batteryRaw
       << ",\"dhtTemp\":" << static_cast<int>(rec.dhtTemp)
       << ",\"dhtHumidity\":" << static_cast<int>(rec.dhtHumidity)
       << ",\"dhtValid\":" << static_cast<int>(rec.dhtValid)
       << "}";
  }
  ss << "]}";

  return ss.str();
}

std::string TcpApi::getEvents(uint32_t startYmd, uint32_t endYmd, const std::string& serial) {
  std::lock_guard<std::mutex> lock(fleetMutex_);

  auto* sub = menu_.search(serial);
  if (!sub) {
    return TcpResponse::error("Submarine not found: " + serial).body;
  }

  auto records = sub->centralComputer().dataStore().queryEvents(startYmd, endYmd);

  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"data\":[";
  bool first = true;
  for (const auto& rec : records) {
    if (!first) ss << ",";
    first = false;
    ss << "{\"ymd\":" << rec.ymd
       << ",\"hms\":\"" << json::escape(rec.hms) << "\""
       << ",\"eventType\":" << static_cast<int>(rec.eventType)
       << ",\"eventValue\":" << rec.eventValue
       << ",\"mode\":" << static_cast<int>(rec.mode)
       << "}";
  }
  ss << "]}";

  return ss.str();
}

std::string TcpApi::summaryReport() {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  const auto& subs = menu_.allSubmarines();

  int totalSubs = static_cast<int>(subs.size());
  int combatCount = 0, researchCount = 0, activeMissions = 0;

  for (const auto& sub : subs) {
    if (sub->typeName() == "Combat") combatCount++;
    else researchCount++;
    if (sub->isAssignedToMission()) activeMissions++;
  }

  std::stringstream ss;
  ss << "{\"status\":\"ok\",\"data\":"
     << "{\"totalSubmarines\":" << totalSubs
     << ",\"combat\":" << combatCount
     << ",\"research\":" << researchCount
     << ",\"activeMissions\":" << activeMissions
     << ",\"submarines\":[";

  bool first = true;
  for (const auto& sub : subs) {
    if (!first) ss << ",";
    first = false;
    ss << "{\"serial\":\"" << json::escape(sub->serialNumber())
       << "\",\"type\":\"" << sub->typeName()
       << "\",\"missionAssigned\":" << (sub->isAssignedToMission() ? "true" : "false") << "}";
  }

  ss << "]}}";

  return ss.str();
}

}  // namespace submarine
