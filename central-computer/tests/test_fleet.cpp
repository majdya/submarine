#include <filesystem>

#include "combat_submarine.h"
#include "fleet.h"
#include "research_submarine.h"
#include "test_harness.h"

namespace fs = std::filesystem;
using namespace submarine;

static std::unique_ptr<CentralComputer> makeDisconnectedCC(const std::string& tag) {
  fs::path base = fs::temp_directory_path() / ("fleet_test_" + tag);
  return std::make_unique<CentralComputer>((base / "logs").string(), (base / "data").string());
}

static void test_add_and_find_and_duplicate_serial_rejected() {
  Fleet fleet;
  auto* s1 = fleet.addSubmarine(std::make_unique<ResearchSubmarine>("SN-1", "Nautilus"));
  CHECK(s1 != nullptr);
  CHECK(fleet.findBySerial("SN-1") == s1);
  CHECK(fleet.findBySerial("nope") == nullptr);

  // Duplicate serial number must be rejected (findBySerial/messaging
  // assume serials are unique).
  auto* dup = fleet.addSubmarine(std::make_unique<ResearchSubmarine>("SN-1", "Impostor"));
  CHECK(dup == nullptr);
  CHECK(fleet.submarines().size() == 1);
}

static void test_mission_assign_update_end_is_exclusive() {
  ResearchSubmarine sub("SN-2", "Explorer");
  CHECK(!sub.isAssignedToMission());

  Mission m("Deep trench survey");
  m.setResearchTopic("Hydrothermal vents");
  m.setResearcherNames({"Dr. Ada", "Dr. Grace"});
  CHECK(sub.assignMission(m));
  CHECK(sub.isAssignedToMission());

  // Can't double-assign while already on a mission.
  CHECK(!sub.assignMission(Mission("Second mission")));

  // Update in place via the mutable accessor (what Menu's "update mission
  // details" operation uses).
  sub.currentMissionMutable()->setResearchTopic("Updated topic");
  CHECK(sub.currentMission()->researchTopic() == "Updated topic");

  CHECK(sub.endMission());
  CHECK(!sub.isAssignedToMission());
  CHECK(sub.missionHistory().size() == 1);
  CHECK(sub.missionHistory()[0].researchTopic() == "Updated topic");

  // Ending again with nothing assigned fails cleanly.
  CHECK(!sub.endMission());
}

static void test_combat_participation_and_messaging() {
  Fleet fleet;
  auto* a = fleet.addSubmarine(
      std::make_unique<CombatSubmarine>("C-1", "Alpha", makeDisconnectedCC("a")));
  auto* b = fleet.addSubmarine(
      std::make_unique<CombatSubmarine>("C-2", "Bravo", makeDisconnectedCC("b")));
  auto* aCombat = fleet.findCombatBySerial("C-1");
  auto* bCombat = fleet.findCombatBySerial("C-2");
  CHECK(aCombat == a);
  CHECK(bCombat == b);

  Mission joint("Blockade run");
  joint.setCommanderName("Cmdr. Reyes");
  joint.setPersonnelCount(42);
  CHECK(aCombat->assignMission(joint));

  aCombat->addParticipatingSubmarine("C-2");
  aCombat->addParticipatingSubmarine("C-1");  // self - must be ignored
  aCombat->addParticipatingSubmarine("C-2");  // duplicate - must be ignored
  CHECK(aCombat->participatingSubmarineSerials().size() == 1);
  CHECK(aCombat->participatingSubmarineSerials()[0] == "C-2");

  // Operation 8/9: a message sent from Bravo to Alpha, stored with a
  // resolvable reference to the sender.
  bCombat->receiveMessage(Message{"placeholder-not-used-here", "ignored"});  // sanity: API works both ways
  aCombat->receiveMessage(Message{bCombat->serialNumber(), "Rendezvous at grid 7"});
  CHECK(aCombat->receivedMessages().size() == 1);
  CHECK(aCombat->receivedMessages()[0].senderSerial() == "C-2");
  CHECK(aCombat->receivedMessages()[0].content() == "Rendezvous at grid 7");

  auto* sender = fleet.findBySerial(aCombat->receivedMessages()[0].senderSerial());
  CHECK(sender != nullptr);
  CHECK(sender->name() == "Bravo");
}

static void test_research_and_combat_type_names_and_dynamic_cast() {
  Fleet fleet;
  fleet.addSubmarine(std::make_unique<ResearchSubmarine>("R-1", "Beagle"));
  fleet.addSubmarine(std::make_unique<CombatSubmarine>("C-3", "Charlie", makeDisconnectedCC("c")));

  CHECK(fleet.findBySerial("R-1")->typeName() == "Research");
  CHECK(fleet.findBySerial("C-3")->typeName() == "Combat");
  CHECK(fleet.findCombatBySerial("R-1") == nullptr);  // wrong type, not a crash
  CHECK(fleet.findCombatBySerial("C-3") != nullptr);
}

int main() {
  test_add_and_find_and_duplicate_serial_rejected();
  test_mission_assign_update_end_is_exclusive();
  test_combat_participation_and_messaging();
  test_research_and_combat_type_names_and_dynamic_cast();
  TEST_MAIN_EXIT();
}
