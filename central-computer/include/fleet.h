#ifndef CENTRAL_COMPUTER_FLEET_H
#define CENTRAL_COMPUTER_FLEET_H

#include <memory>
#include <vector>

#include "combat_submarine.h"
#include "submarine.h"

namespace submarine {

// Owns every Submarine in the system (operations 1-3 in the spec's menu)
// and resolves the serial-number references CombatSubmarine stores for
// participating submarines and message senders (see message.h /
// combat_submarine.h for why those are serials rather than pointers).
class Fleet {
 public:
  // Takes ownership; returns a raw, non-owning pointer for immediate use
  // (e.g. to assign a mission right after adding). Fails (returns nullptr)
  // if the serial number is already in the fleet - serial numbers must be
  // unique for findBySerial/messaging to be unambiguous.
  Submarine* addSubmarine(std::unique_ptr<Submarine> sub);

  const std::vector<std::unique_ptr<Submarine>>& submarines() const { return submarines_; }

  Submarine* findBySerial(const std::string& serial);
  const Submarine* findBySerial(const std::string& serial) const;

  // Convenience for operations that only make sense for combat submarines
  // (participation, messaging) - returns nullptr if the serial doesn't
  // exist or names a ResearchSubmarine.
  CombatSubmarine* findCombatBySerial(const std::string& serial);

 private:
  std::vector<std::unique_ptr<Submarine>> submarines_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_FLEET_H
