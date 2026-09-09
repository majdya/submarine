#ifndef CENTRAL_COMPUTER_MENU_H
#define CENTRAL_COMPUTER_MENU_H

#include <vector>

#include "fleet.h"

namespace submarine {

// Pure logic behind the spec's 10 numbered menu operations - no console
// I/O here (that's main.cpp's job), so these can be exercised directly
// from tests. Every method takes serial numbers rather than pointers for
// the same reason CombatSubmarine stores participants/senders as serials
// (see message.h): a caller only ever has an identifier typed by a human,
// and Fleet is the single place that resolves those to live objects.
class Menu {
 public:
  explicit Menu(Fleet& fleet) : fleet_(fleet) {}

  // Operation 1: "Add a new submarine to the fleet - choose the submarine
  // type and enter the relevant details." Two overloads stand in for
  // "choose the type"; returns nullptr if the serial number is already
  // taken (see Fleet::addSubmarine).
  Submarine* addResearchSubmarine(std::string serial, std::string name,
                                   std::unique_ptr<CentralComputer> centralComputer = nullptr);
  Submarine* addCombatSubmarine(std::string serial, std::string name,
                                 std::unique_ptr<CentralComputer> centralComputer = nullptr);

  // Operation 2: every submarine currently in the fleet, in insertion order.
  const std::vector<std::unique_ptr<Submarine>>& allSubmarines() const { return fleet_.submarines(); }

  // Operation 3.
  Submarine* search(const std::string& serial) { return fleet_.findBySerial(serial); }

  // Operation 4: fails if the serial doesn't exist or is already assigned.
  bool assignMission(const std::string& serial, Mission mission);

  // Operation 5: returns a pointer to the live Mission for the caller to
  // edit fields on directly (e.g. `menu.missionToEdit(s)->setResearchTopic(...)`),
  // or nullptr if the serial doesn't exist or has no current mission.
  Mission* missionToEdit(const std::string& serial);

  // Operation 6.
  bool endMission(const std::string& serial);

  // Operation 7: both serials must name existing CombatSubmarines.
  bool addParticipating(const std::string& combatSerial, const std::string& otherCombatSerial);

  // Operation 8: fromSerial and toSerial must both be CombatSubmarines that
  // are associated with the same mission (one appears in the other's
  // participating-submarines list - see addParticipating; the association
  // this checks is deliberately symmetric even though addParticipating
  // itself only updates the caller's own list, since two submarines
  // "participating in the same mission" is inherently a mutual fact).
  bool sendMessage(const std::string& fromSerial, const std::string& toSerial, std::string content);

  // Operation 9: nullptr if the serial doesn't exist or isn't a CombatSubmarine.
  const std::vector<Message>* messagesFor(const std::string& serial);

  Fleet& fleet() { return fleet_; }

 private:
  Fleet& fleet_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_MENU_H
