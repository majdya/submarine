#ifndef CENTRAL_COMPUTER_MISSION_H
#define CENTRAL_COMPUTER_MISSION_H

#include <string>
#include <vector>

namespace submarine {

// Per spec section "OOP Part": a combat submarine's current mission
// carries "the description of its current mission, the name of the
// commander, and the number of combat personnel on board for the
// mission"; a research submarine's carries "the names of the researchers
// on board and the current research topic". Rather than a class hierarchy
// mirroring Submarine's (which would force RTTI/casting every time a
// mission is touched, for no real benefit at this scale), Mission is one
// concrete class holding every field either type uses - Menu only asks
// for, and displays, the subset relevant to a given submarine's type
// (see Menu::promptMissionDetails), determined by a dynamic_cast on the
// Submarine.
class Mission {
 public:
  Mission() = default;
  explicit Mission(std::string description) : description_(std::move(description)) {}

  const std::string& description() const { return description_; }
  void setDescription(std::string d) { description_ = std::move(d); }

  // --- Combat-submarine fields ---
  const std::string& commanderName() const { return commanderName_; }
  void setCommanderName(std::string n) { commanderName_ = std::move(n); }

  int personnelCount() const { return personnelCount_; }
  void setPersonnelCount(int n) { personnelCount_ = n; }

  // --- Research-submarine fields ---
  const std::vector<std::string>& researcherNames() const { return researcherNames_; }
  void setResearcherNames(std::vector<std::string> names) { researcherNames_ = std::move(names); }

  const std::string& researchTopic() const { return researchTopic_; }
  void setResearchTopic(std::string t) { researchTopic_ = std::move(t); }

 private:
  std::string description_;
  std::string commanderName_;
  int personnelCount_ = 0;
  std::vector<std::string> researcherNames_;
  std::string researchTopic_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_MISSION_H
