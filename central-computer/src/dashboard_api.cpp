#include "dashboard_api.h"

#include <cstdlib>

#include "combat_submarine.h"
#include "json_util.h"
#include "research_submarine.h"
#include "transport/serial_transport.h"

namespace submarine {

namespace {

std::string missionJson(const Mission& m) {
  json::ObjectBuilder b;
  b.strField("description", m.description())
      .strField("commanderName", m.commanderName())
      .numField("personnelCount", m.personnelCount())
      .strField("researchTopic", m.researchTopic());
  std::vector<std::string> researchers;
  for (const auto& n : m.researcherNames()) researchers.push_back(json::str(n));
  b.field("researcherNames", json::array(researchers));
  return b.build();
}

std::string snapshotJson(const LiveSnapshot& s) {
  json::ObjectBuilder b;
  b.boolField("hasData", s.hasData)
      .strField("receivedAtHms", s.receivedAtHms)
      .numField("mode", static_cast<int>(s.mode))
      .numField("lightRaw", s.lightRaw)
      .numField("tempAdcRaw", s.tempAdcRaw)
      .numField("batteryRaw", s.batteryRaw)
      .numField("dhtTemp", static_cast<int>(s.dhtTemp))
      .numField("dhtHumidity", static_cast<int>(s.dhtHumidity))
      .boolField("dhtValid", s.dhtValid != 0)
      .strField("lastEventDescription", s.lastEventDescription)
      .strField("lastEventAtHms", s.lastEventAtHms);
  return b.build();
}

std::string submarineJson(const Submarine& sub) {
  json::ObjectBuilder b;
  b.strField("serial", sub.serialNumber())
      .strField("name", sub.name())
      .strField("type", sub.typeName())
      .boolField("assigned", sub.isAssignedToMission())
      .numField("missionHistoryCount", static_cast<int>(sub.missionHistory().size()));

  if (const auto& m = sub.currentMission()) {
    b.field("mission", missionJson(*m));
  } else {
    b.field("mission", "null");
  }

  if (const auto* combat = dynamic_cast<const CombatSubmarine*>(&sub)) {
    std::vector<std::string> participating;
    for (const auto& s : combat->participatingSubmarineSerials()) participating.push_back(json::str(s));
    b.field("participatingSerials", json::array(participating));

    std::vector<std::string> messages;
    for (const auto& msg : combat->receivedMessages()) {
      json::ObjectBuilder mb;
      mb.strField("from", msg.senderSerial()).strField("content", msg.content());
      messages.push_back(mb.build());
    }
    b.field("messages", json::array(messages));
    b.boolField("connected", combat->centralComputer().isConnected());
    b.field("liveSnapshot", snapshotJson(combat->centralComputer().latestSnapshot()));
  } else {
    b.field("participatingSerials", "[]");
    b.field("messages", "[]");
    b.boolField("connected", false);
    b.field("liveSnapshot", "null");
  }
  return b.build();
}

Mission buildMission(const std::string& description, const std::string& commanderName,
                      const std::string& personnelCountStr, const std::string& researchTopic,
                      const std::string& researcherNamesCsv) {
  Mission m(description);
  m.setCommanderName(commanderName);
  if (!personnelCountStr.empty()) {
    m.setPersonnelCount(std::atoi(personnelCountStr.c_str()));
  }
  m.setResearchTopic(researchTopic);

  std::vector<std::string> names;
  std::string current;
  for (char c : researcherNamesCsv) {
    if (c == ',') {
      if (!current.empty()) names.push_back(current);
      current.clear();
    } else {
      current += c;
    }
  }
  if (!current.empty()) names.push_back(current);
  // trim leading spaces left over from ", " separated input
  for (auto& n : names) {
    size_t start = n.find_first_not_of(' ');
    n = (start == std::string::npos) ? "" : n.substr(start);
  }
  m.setResearcherNames(names);
  return m;
}

std::string okJson() { return R"({"ok":true})"; }
std::string errorJson(const std::string& message) {
  json::ObjectBuilder b;
  b.boolField("ok", false).strField("error", message);
  return b.build();
}

}  // namespace

std::string DashboardApi::stateJson() {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  std::vector<std::string> items;
  for (const auto& sub : menu_.fleet().submarines()) {
    items.push_back(submarineJson(*sub));
  }
  return "{\"submarines\":" + json::array(items) + "}";
}

std::string DashboardApi::addSubmarine(const std::string& type, const std::string& serial,
                                        const std::string& name, const std::string& port) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  Submarine* created = nullptr;
  if (type == "combat") {
    std::unique_ptr<CentralComputer> cc;
    std::string logDir = "logs/" + serial;
    std::string dataDir = "data/" + serial;
    if (port.empty()) {
      cc = std::make_unique<CentralComputer>(logDir, dataDir);
    } else {
      cc = std::make_unique<CentralComputer>(logDir, dataDir, std::make_unique<SerialTransport>(port, 115200));
      cc->connect();  // best-effort - state will just show connected:false if it fails
    }
    created = menu_.addCombatSubmarine(serial, name, std::move(cc));
  } else {
    created = menu_.addResearchSubmarine(serial, name);
  }
  if (!created) return errorJson("serial number already in use");
  return okJson();
}

std::string DashboardApi::assignMission(const std::string& serial, const std::string& description,
                                         const std::string& commanderName,
                                         const std::string& personnelCountStr,
                                         const std::string& researchTopic,
                                         const std::string& researcherNamesCsv) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  if (!menu_.search(serial)) return errorJson("no such submarine");
  Mission m = buildMission(description, commanderName, personnelCountStr, researchTopic, researcherNamesCsv);
  if (!menu_.assignMission(serial, std::move(m))) return errorJson("already assigned to a mission");
  return okJson();
}

std::string DashboardApi::updateMission(const std::string& serial, const std::string& description,
                                         const std::string& commanderName,
                                         const std::string& personnelCountStr,
                                         const std::string& researchTopic,
                                         const std::string& researcherNamesCsv) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  Mission* m = menu_.missionToEdit(serial);
  if (!m) return errorJson("no current mission to update");
  *m = buildMission(description, commanderName, personnelCountStr, researchTopic, researcherNamesCsv);
  return okJson();
}

std::string DashboardApi::endMission(const std::string& serial) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  if (!menu_.endMission(serial)) return errorJson("no such submarine, or none assigned");
  return okJson();
}

std::string DashboardApi::addParticipating(const std::string& serial, const std::string& other) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  if (!menu_.addParticipating(serial, other)) return errorJson("both must be existing combat submarines");
  return okJson();
}

std::string DashboardApi::sendMessage(const std::string& from, const std::string& to,
                                       const std::string& content) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  if (!menu_.sendMessage(from, to, content)) {
    return errorJson("both must be combat submarines associated with the same mission");
  }
  return okJson();
}

}  // namespace submarine
