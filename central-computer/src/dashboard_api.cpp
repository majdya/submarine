#include "dashboard_api.h"

#include <cstdlib>
#include <optional>

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

  // Every submarine - research or combat - now has its own CentralComputer
  // (see submarine.h's class comment on this deliberate spec deviation).
  b.boolField("connected", sub.centralComputer().isConnected());
  b.field("liveSnapshot", snapshotJson(sub.centralComputer().latestSnapshot()));

  // Participating submarines / messages remain Combat-only OOP features
  // (spec operations 7-9), unrelated to hardware.
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
  } else {
    b.field("participatingSerials", "[]");
    b.field("messages", "[]");
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
  // Every submarine can optionally be wired to real LNC hardware now,
  // research included (see submarine.h's class comment on this deliberate
  // spec deviation) - an empty `port` just means no hardware for this one.
  std::unique_ptr<CentralComputer> cc;
  std::string logDir = "logs/" + serial;
  std::string dataDir = "data/" + serial;
  if (port.empty()) {
    cc = std::make_unique<CentralComputer>(logDir, dataDir);
  } else {
    cc = std::make_unique<CentralComputer>(logDir, dataDir, std::make_unique<SerialTransport>(port, 115200));
    cc->connect();  // best-effort - state will just show connected:false if it fails
  }

  Submarine* created = nullptr;
  if (type == "Combat" || type == "combat") {
    created = menu_.addCombatSubmarine(serial, name, std::move(cc));
  } else {
    created = menu_.addResearchSubmarine(serial, name, std::move(cc));
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

namespace {
std::optional<int32_t> parseOptionalInt(const std::string& s) {
  if (s.empty()) return std::nullopt;
  try {
    return static_cast<int32_t>(std::stol(s));
  } catch (...) {
    return std::nullopt;
  }
}
}  // namespace

std::string DashboardApi::setLimits(const std::string& serial, const std::string& param,
                                     const std::string& normalMinStr, const std::string& normalMaxStr,
                                     const std::string& warningMinStr, const std::string& warningMaxStr,
                                     const std::string& enabledStr) {
  std::lock_guard<std::mutex> lock(fleetMutex_);
  auto* sub = menu_.search(serial);
  if (!sub) return errorJson("no submarine with that serial");

  uint8_t paramCode;
  bool isTemp = (param == "temp");
  if (isTemp) paramCode = PARAM_TEMP;
  else if (param == "humidity") paramCode = PARAM_HUMIDITY;
  else if (param == "light") paramCode = PARAM_LIGHT;
  else if (param == "battery") paramCode = PARAM_BATTERY;
  else return errorJson("param must be temp, humidity, light, or battery");

  auto normalMin = parseOptionalInt(normalMinStr);
  auto normalMax = parseOptionalInt(normalMaxStr);
  auto warningMin = parseOptionalInt(warningMinStr);
  auto warningMax = parseOptionalInt(warningMaxStr);
  bool anyBoundGiven = normalMin || normalMax || warningMin || warningMax;
  if (anyBoundGiven) {
    if (!normalMin || !warningMin) return errorJson("normal and warning bounds are required together");
    if (isTemp && (!normalMax || !warningMax)) {
      return errorJson("temp requires all four bounds (normal min/max, warning min/max)");
    }
  }

  // Deliberate extension beyond the spec, added at the project owner's
  // explicit request: enable/disable this sensor's contribution to the
  // LNC's overall mode, independent of (or alongside) its limits.
  std::optional<bool> enabled;
  if (enabledStr == "enable") enabled = true;
  else if (enabledStr == "disable") enabled = false;
  else if (!enabledStr.empty()) return errorJson("enabled must be 'enable', 'disable', or omitted");

  if (!anyBoundGiven && !enabled) return errorJson("nothing to do - no limits and no enable/disable given");

  bool ok =
      sub->centralComputer().commands().setLimits(paramCode, normalMin, normalMax, warningMin, warningMax, enabled);
  if (!ok) return errorJson("LNC did not acknowledge (check connection) or rejected the command");
  return okJson();
}

}  // namespace submarine
