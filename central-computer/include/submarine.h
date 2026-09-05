#ifndef CENTRAL_COMPUTER_SUBMARINE_H
#define CENTRAL_COMPUTER_SUBMARINE_H

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "mission.h"

namespace submarine {

// Abstract base of the fleet's OOP hierarchy (spec "OOP Part"): "Each
// submarine has a serial number and a name. In addition, for every
// submarine the system must indicate whether it is currently assigned to
// a mission or available for a new mission." ResearchSubmarine and
// CombatSubmarine (see their own headers) are the two concrete types.
class Submarine {
 public:
  Submarine(std::string serialNumber, std::string name)
      : serialNumber_(std::move(serialNumber)), name_(std::move(name)) {}
  virtual ~Submarine() = default;

  Submarine(const Submarine&) = delete;
  Submarine& operator=(const Submarine&) = delete;

  const std::string& serialNumber() const { return serialNumber_; }
  const std::string& name() const { return name_; }

  bool isAssignedToMission() const { return currentMission_.has_value(); }
  const std::optional<Mission>& currentMission() const { return currentMission_; }
  Mission* currentMissionMutable() { return currentMission_ ? &*currentMission_ : nullptr; }
  const std::vector<Mission>& missionHistory() const { return missionHistory_; }

  // Assigns `mission` as current (operation 4, "Assign a mission to a
  // submarine"). Returns false - and leaves state unchanged - if a mission
  // is already assigned, since the spec treats "assigned" vs "available"
  // as mutually exclusive ("it is clear that it is no longer available for
  // another mission").
  bool assignMission(Mission mission);

  // Ends the current mission (operation 6): moves it into missionHistory
  // and marks the submarine available again. Returns false if none was
  // assigned.
  bool endMission();

  // A short, human-readable type label ("Research" / "Combat") used by
  // Menu for display and by operations that need to branch on type
  // without an explicit dynamic_cast at the call site.
  virtual std::string typeName() const = 0;

  // Prints this submarine's full details (operation 2's "including the
  // relevant details of each submarine") - each subclass adds its own
  // type-specific fields on top of the common ones printed here.
  virtual void printDetails(std::ostream& os) const;

 private:
  std::string serialNumber_;
  std::string name_;
  std::optional<Mission> currentMission_;
  std::vector<Mission> missionHistory_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_SUBMARINE_H
