#include "research_submarine.h"

#include <ostream>

namespace submarine {

void ResearchSubmarine::printDetails(std::ostream& os) const {
  Submarine::printDetails(os);
  if (const auto& m = currentMission()) {
    os << "  Research topic: " << m->researchTopic() << "\n";
    os << "  Researchers (" << m->researcherNames().size() << "): ";
    for (const auto& n : m->researcherNames()) os << n << "; ";
    os << "\n";
  }
}

}  // namespace submarine
