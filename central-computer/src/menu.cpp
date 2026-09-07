#include "menu.h"

#include <algorithm>

#include "research_submarine.h"

namespace submarine {

Submarine* Menu::addResearchSubmarine(std::string serial, std::string name) {
  return fleet_.addSubmarine(std::make_unique<ResearchSubmarine>(std::move(serial), std::move(name)));
}

Submarine* Menu::addCombatSubmarine(std::string serial, std::string name,
                                     std::unique_ptr<CentralComputer> centralComputer) {
  return fleet_.addSubmarine(
      std::make_unique<CombatSubmarine>(std::move(serial), std::move(name), std::move(centralComputer)));
}

bool Menu::assignMission(const std::string& serial, Mission mission) {
  auto* sub = fleet_.findBySerial(serial);
  if (!sub) return false;
  return sub->assignMission(std::move(mission));
}

Mission* Menu::missionToEdit(const std::string& serial) {
  auto* sub = fleet_.findBySerial(serial);
  if (!sub) return nullptr;
  return sub->currentMissionMutable();
}

bool Menu::endMission(const std::string& serial) {
  auto* sub = fleet_.findBySerial(serial);
  if (!sub) return false;
  return sub->endMission();
}

bool Menu::addParticipating(const std::string& combatSerial, const std::string& otherCombatSerial) {
  auto* a = fleet_.findCombatBySerial(combatSerial);
  auto* b = fleet_.findCombatBySerial(otherCombatSerial);
  if (!a || !b) return false;
  a->addParticipatingSubmarine(otherCombatSerial);
  return true;
}

bool Menu::sendMessage(const std::string& fromSerial, const std::string& toSerial, std::string content) {
  auto* from = fleet_.findCombatBySerial(fromSerial);
  auto* to = fleet_.findCombatBySerial(toSerial);
  if (!from || !to || from == to) return false;

  const auto& fromList = from->participatingSubmarineSerials();
  const auto& toList = to->participatingSubmarineSerials();
  bool associated = std::find(fromList.begin(), fromList.end(), toSerial) != fromList.end() ||
                     std::find(toList.begin(), toList.end(), fromSerial) != toList.end();
  if (!associated) return false;

  to->receiveMessage(Message(fromSerial, std::move(content)));
  return true;
}

const std::vector<Message>* Menu::messagesFor(const std::string& serial) {
  auto* combat = fleet_.findCombatBySerial(serial);
  if (!combat) return nullptr;
  return &combat->receivedMessages();
}

}  // namespace submarine
