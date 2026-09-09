#ifndef CENTRAL_COMPUTER_COMBAT_SUBMARINE_H
#define CENTRAL_COMPUTER_COMBAT_SUBMARINE_H

#include <memory>
#include <vector>

#include "central_computer.h"
#include "message.h"
#include "submarine.h"

namespace submarine {

// "A combat submarine is used for operational missions... Each submarine
// has a central computer... it should be treated as an object that
// belongs to each combat submarine." Plus: "for each combat submarine, the
// system must also keep track of the other submarines participating with
// it in the current mission" and the messaging requirement ("submarines
// participating in the same mission can communicate with one another and
// send messages... store the message content together with a reference to
// the submarine that sent it").
//
// The CentralComputer itself is now owned by the Submarine base class (see
// submarine.h - a deliberate, acknowledged deviation from the spec's
// literal wording, so a ResearchSubmarine can have one too); this class
// keeps its own centralComputer() accessors purely so existing callers
// (combat->centralComputer().foo()) didn't need to change.
class CombatSubmarine : public Submarine {
 public:
  CombatSubmarine(std::string serialNumber, std::string name,
                   std::unique_ptr<CentralComputer> centralComputer = nullptr)
      : Submarine(std::move(serialNumber), std::move(name), std::move(centralComputer)) {}

  std::string typeName() const override { return "Combat"; }
  void printDetails(std::ostream& os) const override;

  using Submarine::centralComputer;

  // Operation 7: "For combat submarine - associate additional combat
  // submarines with the same mission." Stored by serial number (see
  // message.h's class comment for why references are serials, not
  // pointers, throughout this codebase) - resolve through Fleet::findBySerial
  // for display.
  void addParticipatingSubmarine(const std::string& serial);
  const std::vector<std::string>& participatingSubmarineSerials() const {
    return participatingSubmarineSerials_;
  }

  // Operations 8/9: messaging between combat submarines in the same mission.
  void receiveMessage(Message msg) { receivedMessages_.push_back(std::move(msg)); }
  const std::vector<Message>& receivedMessages() const { return receivedMessages_; }

 private:
  std::vector<std::string> participatingSubmarineSerials_;
  std::vector<Message> receivedMessages_;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_COMBAT_SUBMARINE_H
