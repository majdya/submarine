#ifndef CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H
#define CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H

#include "submarine.h"

namespace submarine {

// "A research submarine is used for research missions. For each research
// submarine, the system must store the names of the researchers on board
// and the current research topic." These live on the *mission* (see
// mission.h's class comment for why), so ResearchSubmarine itself adds no
// new state beyond Submarine - only the type name and how its mission
// details are printed.
class ResearchSubmarine : public Submarine {
 public:
  ResearchSubmarine(std::string serialNumber, std::string name)
      : Submarine(std::move(serialNumber), std::move(name)) {}

  std::string typeName() const override { return "Research"; }
  void printDetails(std::ostream& os) const override;
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_RESEARCH_SUBMARINE_H
