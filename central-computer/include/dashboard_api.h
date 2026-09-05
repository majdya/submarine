#ifndef CENTRAL_COMPUTER_DASHBOARD_API_H
#define CENTRAL_COMPUTER_DASHBOARD_API_H

#include <mutex>
#include <string>

#include "fleet.h"
#include "menu.h"

namespace submarine {

// The web dashboard's fully-interactive backend: every method here does
// one complete operation (read or write) against the shared Fleet under
// fleetMutex_ and returns a ready-to-send JSON body - callers (the
// HttpServer route handlers in main.cpp) never touch Fleet/Menu directly.
//
// Concurrency note: main.cpp's console menu handlers use the *same*
// Menu instance and are expected to take the same mutex (passed in here
// by reference) around each Menu call they make, so the console and the
// browser can safely be used at the same time. This is a coarse,
// whole-fleet lock rather than fine-grained per-submarine locking - more
// than adequate for a local single-operator dashboard, and far simpler to
// reason about correctly than finer-grained locking would be.
class DashboardApi {
 public:
  DashboardApi(Menu& menu, std::mutex& fleetMutex) : menu_(menu), fleetMutex_(fleetMutex) {}

  // GET /api/state - the whole fleet plus each combat submarine's live
  // LNC snapshot (see CentralComputer::latestSnapshot).
  std::string stateJson();

  // Each of these performs one full operation and returns a JSON body:
  // {"ok":true} or {"ok":false,"error":"..."}. type is "research" or
  // "combat"; port is optional (empty = no real hardware for this
  // submarine's central computer).
  std::string addSubmarine(const std::string& type, const std::string& serial,
                            const std::string& name, const std::string& port);

  // description/commanderName/personnelCountStr apply to combat
  // submarines; description/researchTopic/researcherNamesCsv to research
  // submarines - callers only need to send the fields relevant to the
  // submarine's actual type, but sending all of them is harmless (Mission
  // holds every field regardless of type - see mission.h).
  std::string assignMission(const std::string& serial, const std::string& description,
                             const std::string& commanderName, const std::string& personnelCountStr,
                             const std::string& researchTopic, const std::string& researcherNamesCsv);
  std::string updateMission(const std::string& serial, const std::string& description,
                             const std::string& commanderName, const std::string& personnelCountStr,
                             const std::string& researchTopic, const std::string& researcherNamesCsv);
  std::string endMission(const std::string& serial);
  std::string addParticipating(const std::string& serial, const std::string& other);
  std::string sendMessage(const std::string& from, const std::string& to, const std::string& content);

 private:
  Menu& menu_;
  std::mutex& fleetMutex_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_DASHBOARD_API_H
