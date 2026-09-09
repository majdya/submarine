#ifndef CENTRAL_COMPUTER_SUBMARINE_H
#define CENTRAL_COMPUTER_SUBMARINE_H

#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "central_computer.h"
#include "mission.h"

namespace submarine {

// Abstract base of the fleet's OOP hierarchy (spec "OOP Part"): "Each
// submarine has a serial number and a name. In addition, for every
// submarine the system must indicate whether it is currently assigned to
// a mission or available for a new mission." ResearchSubmarine and
// CombatSubmarine (see their own headers) are the two concrete types.
//
// [Deviation from the spec's literal wording, done deliberately at the
// project owner's request: the spec's OOP Part says the central computer
// "belongs to each combat submarine". This project instead gives every
// submarine - research included - its own CentralComputer, so a research
// submarine can also be wired to real LNC hardware (or, same as most
// CombatSubmarines in a demo fleet, left on the default loopback
// transport). CombatSubmarine's own centralComputer() accessors (see
// combat_submarine.h) are unchanged and just forward to this base-class
// storage, so none of its existing callers needed to change.]
class Submarine {
 public:
  // `centralComputer` may be omitted (or explicitly nullptr): a
  // CentralComputer is auto-created on a default LoopbackTransport under
  // "logs/<serial>" / "data/<serial>" so every submarine always has one,
  // whether or not it names real hardware.
  Submarine(std::string serialNumber, std::string name,
            std::unique_ptr<CentralComputer> centralComputer = nullptr)
      : serialNumber_(std::move(serialNumber)),
        name_(std::move(name)),
        centralComputer_(centralComputer
                              ? std::move(centralComputer)
                              : std::make_unique<CentralComputer>("logs/" + serialNumber_,
                                                                   "data/" + serialNumber_)) {}
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

  // Every submarine - research or combat - has its own CentralComputer
  // (see the class comment above for why this is broader than the spec's
  // literal wording).
  CentralComputer& centralComputer() { return *centralComputer_; }
  const CentralComputer& centralComputer() const { return *centralComputer_; }

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
  std::unique_ptr<CentralComputer> centralComputer_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_SUBMARINE_H
