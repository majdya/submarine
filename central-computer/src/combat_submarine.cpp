#include "combat_submarine.h"

#include <algorithm>
#include <ostream>

namespace submarine {

void CombatSubmarine::addParticipatingSubmarine(const std::string& serial) {
  if (serial == serialNumber()) return;  // a submarine doesn't participate with itself
  auto& v = participatingSubmarineSerials_;
  if (std::find(v.begin(), v.end(), serial) == v.end()) {
    v.push_back(serial);
  }
}

void CombatSubmarine::printDetails(std::ostream& os) const {
  Submarine::printDetails(os);
  if (const auto& m = currentMission()) {
    os << "  Commander: " << m->commanderName() << "  Personnel: " << m->personnelCount() << "\n";
  }
  os << "  Participating submarines (" << participatingSubmarineSerials_.size() << "): ";
  for (const auto& s : participatingSubmarineSerials_) os << s << "; ";
  os << "\n";
  os << "  Messages received: " << receivedMessages_.size() << "\n";
}

}  // namespace submarine
