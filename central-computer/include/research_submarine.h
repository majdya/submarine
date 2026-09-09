#ifndef CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H
#define CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H

#include <memory>

#include "submarine.h"

namespace submarine {

// "A research submarine is used for research missions. For each research
// submarine, the system must store the names of the researchers on board
// and the current research topic." These live on the *mission* (see
// mission.h's class comment for why), so ResearchSubmarine itself adds no
// new state beyond Submarine - only the type name and how its mission
// details are printed.
//
// `centralComputer` is optional, same as the base class: a research
// submarine can be wired to real LNC hardware, same as a combat submarine
// (see submarine.h's class comment on this deliberate spec deviation), or
// left on the default loopback transport when it isn't.
class ResearchSubmarine : public Submarine {
 public:
  ResearchSubmarine(std::string serialNumber, std::string name,
                     std::unique_ptr<CentralComputer> centralComputer = nullptr)
      : Submarine(std::move(serialNumber), std::move(name), std::move(centralComputer)) {}

  std::string typeName() const override { return "Research"; }
  void printDetails(std::ostream& os) const override;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H
