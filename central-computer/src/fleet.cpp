#include "fleet.h"

namespace submarine {

Submarine* Fleet::addSubmarine(std::unique_ptr<Submarine> sub) {
  if (findBySerial(sub->serialNumber()) != nullptr) return nullptr;
  submarines_.push_back(std::move(sub));
  return submarines_.back().get();
}

Submarine* Fleet::findBySerial(const std::string& serial) {
  for (auto& s : submarines_) {
    if (s->serialNumber() == serial) return s.get();
  }
  return nullptr;
}

const Submarine* Fleet::findBySerial(const std::string& serial) const {
  for (const auto& s : submarines_) {
    if (s->serialNumber() == serial) return s.get();
  }
  return nullptr;
}

CombatSubmarine* Fleet::findCombatBySerial(const std::string& serial) {
  return dynamic_cast<CombatSubmarine*>(findBySerial(serial));
}

}  // namespace submarine
