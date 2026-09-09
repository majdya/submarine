#include "submarine.h"

#include <ostream>

namespace submarine {

bool Submarine::assignMission(Mission mission) {
  if (currentMission_.has_value()) return false;
  currentMission_ = std::move(mission);
  return true;
}

bool Submarine::endMission() {
  if (!currentMission_.has_value()) return false;
  missionHistory_.push_back(std::move(*currentMission_));
  currentMission_.reset();
  return true;
}

void Submarine::printDetails(std::ostream& os) const {
  os << "Serial: " << serialNumber_ << "  Name: " << name_
     << "  Type: " << typeName()
     << "  Status: " << (isAssignedToMission() ? "ASSIGNED" : "AVAILABLE") << "\n";
  if (currentMission_) {
    os << "  Current mission: " << currentMission_->description() << "\n";
  }
  os << "  Past missions: " << missionHistory_.size() << "\n";
  os << "  Central computer: " << (centralComputer_->isConnected() ? "CONNECTED" : "not connected") << "\n";
}

}  // namespace submarine
