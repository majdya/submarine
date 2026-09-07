// Submarine Fleet Management System - console entry point.
//
// Implements the spec's "OOP Part" menu (10 numbered operations) on top of
// Fleet/Menu, with each CombatSubmarine's CentralComputer able to be wired
// to the real LNC hardware over a serial port (see transport/serial_transport.h)
// using the exact same protocol code the firmware itself was verified
// against (see include/protocol_bridge.h). Only one physical LNC exists on
// a given bench, so in practice only one CombatSubmarine at a time is
// created with a real port; every other one defaults to a disconnected
// placeholder (CentralComputer's default LoopbackTransport).
//
// A local web dashboard (dashboard_api.h/http_server.h) runs alongside the
// console menu in the same process, sharing the same Fleet - it is a
// bonus, fully-interactive way to use the app; the console menu itself is
// unchanged and remains the graded deliverable per the spec. Both sides
// take g_fleetMutex around every Fleet-touching operation, coarse-grained
// (the whole fleet, not per-submarine) on purpose: this is a local,
// single-operator tool, and holding the lock for an entire console
// operation - including its blocking prompts - is simpler to reason about
// correctly than fine-grained locking would be. The practical effect is
// that the dashboard's next poll can briefly wait if a console operation
// is mid-prompt; it always catches up on the following poll.

#include <iostream>
#include <limits>
#include <mutex>
#include <sstream>

#include "dashboard_api.h"
#include "dashboard_html.h"
#include "http_server.h"
#include "menu.h"
#include "transport/serial_transport.h"

using namespace submarine;

namespace {

std::mutex g_fleetMutex;

std::string promptLine(const std::string& label) {
  std::cout << label;
  std::string line;
  std::getline(std::cin, line);
  return line;
}

int promptInt(const std::string& label) {
  while (true) {
    std::string line = promptLine(label);
    try {
      return std::stoi(line);
    } catch (...) {
      std::cout << "  Please enter a whole number.\n";
    }
  }
}

std::vector<std::string> splitCommaList(const std::string& s) {
  std::vector<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    // trim leading/trailing spaces
    size_t start = item.find_first_not_of(' ');
    size_t end = item.find_last_not_of(' ');
    if (start == std::string::npos) continue;
    out.push_back(item.substr(start, end - start + 1));
  }
  return out;
}

std::unique_ptr<CentralComputer> promptCentralComputer(const std::string& serial) {
  std::string logDir = "logs/" + serial;
  std::string dataDir = "data/" + serial;

  std::string port = promptLine(
      "  Serial port for this submarine's LNC (e.g. COM8 or /dev/ttyACM0), "
      "blank if no hardware connected: ");
  if (port.empty()) {
    std::cout << "  -> " << serial << "'s central computer created without hardware (loopback placeholder).\n";
    return std::make_unique<CentralComputer>(logDir, dataDir);
  }

  auto cc = std::make_unique<CentralComputer>(logDir, dataDir,
                                               std::make_unique<SerialTransport>(port, 115200));
  if (!cc->connect()) {
    std::cout << "  -> Could not open " << port << " (" << cc->transport().errorMessage()
              << "). Continuing without hardware for now.\n";
  } else {
    std::cout << "  -> Connected to " << port << ".\n";
  }
  return cc;
}

void printMenu() {
  std::cout << "\n=== Submarine Fleet Management System ===\n"
            << " 1. Add a new submarine to the fleet\n"
            << " 2. Display all submarines in the fleet\n"
            << " 3. Search for and display a submarine by serial number\n"
            << " 4. Assign a mission to a submarine\n"
            << " 5. Update a submarine's mission details\n"
            << " 6. End a submarine's mission\n"
            << " 7. Combat: associate additional combat submarines with the same mission\n"
            << " 8. Send a message from one combat submarine to another\n"
            << " 9. Display the messages received by a submarine\n"
            << "10. Exit the system\n"
            << "Choice: ";
}

// NOTE: each handler below takes g_fleetMutex for its *entire* body
// (including its console prompts) - see the file header comment for why.

void handleAdd(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string type = promptLine("  Type ('research' or 'combat'): ");
  std::string serial = promptLine("  Serial number: ");
  std::string name = promptLine("  Name: ");

  Submarine* created = nullptr;
  if (type == "combat" || type == "Combat") {
    created = menu.addCombatSubmarine(serial, name, promptCentralComputer(serial));
  } else {
    created = menu.addResearchSubmarine(serial, name);
  }

  if (!created) {
    std::cout << "  Could not add submarine - serial number '" << serial << "' is already in use.\n";
  } else {
    std::cout << "  Added " << created->typeName() << " submarine " << serial << ".\n";
  }
}

void handleDisplayAll(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  const auto& all = menu.allSubmarines();
  if (all.empty()) {
    std::cout << "  The fleet is empty.\n";
    return;
  }
  for (const auto& sub : all) {
    sub->printDetails(std::cout);
    std::cout << "---\n";
  }
}

void handleSearch(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Serial number to search for: ");
  auto* sub = menu.search(serial);
  if (!sub) {
    std::cout << "  No submarine with serial '" << serial << "'.\n";
    return;
  }
  sub->printDetails(std::cout);
}

void handleAssignMission(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Serial number: ");
  auto* sub = menu.search(serial);
  if (!sub) {
    std::cout << "  No submarine with serial '" << serial << "'.\n";
    return;
  }
  if (sub->isAssignedToMission()) {
    std::cout << "  " << serial << " is already assigned to a mission - end it first (option 6).\n";
    return;
  }

  Mission mission(promptLine("  Mission description: "));
  if (sub->typeName() == "Research") {
    mission.setResearchTopic(promptLine("  Research topic: "));
    mission.setResearcherNames(splitCommaList(promptLine("  Researcher names (comma-separated): ")));
  } else {
    mission.setCommanderName(promptLine("  Commander name: "));
    mission.setPersonnelCount(promptInt("  Personnel count: "));
  }

  if (menu.assignMission(serial, std::move(mission))) {
    std::cout << "  Mission assigned.\n";
  } else {
    std::cout << "  Could not assign mission.\n";
  }
}

void handleUpdateMission(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Serial number: ");
  auto* sub = menu.search(serial);
  auto* mission = menu.missionToEdit(serial);
  if (!sub || !mission) {
    std::cout << "  " << serial << " has no current mission to update.\n";
    return;
  }

  mission->setDescription(promptLine("  New mission description: "));
  if (sub->typeName() == "Research") {
    mission->setResearchTopic(promptLine("  New research topic: "));
    mission->setResearcherNames(splitCommaList(promptLine("  New researcher names (comma-separated): ")));
  } else {
    mission->setCommanderName(promptLine("  New commander name: "));
    mission->setPersonnelCount(promptInt("  New personnel count: "));
  }
  std::cout << "  Mission details updated.\n";
}

void handleEndMission(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Serial number: ");
  if (menu.endMission(serial)) {
    std::cout << "  Mission ended - " << serial << " is now available.\n";
  } else {
    std::cout << "  Could not end mission (no such submarine, or none assigned).\n";
  }
}

void handleAddParticipating(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Combat submarine serial: ");
  std::string other = promptLine("  Other combat submarine serial to associate: ");
  if (menu.addParticipating(serial, other)) {
    std::cout << "  Associated.\n";
  } else {
    std::cout << "  Could not associate - both serials must name existing combat submarines.\n";
  }
}

void handleSendMessage(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string from = promptLine("  From (combat submarine serial): ");
  std::string to = promptLine("  To (combat submarine serial): ");
  std::string content = promptLine("  Message: ");
  if (menu.sendMessage(from, to, content)) {
    std::cout << "  Message sent.\n";
  } else {
    std::cout << "  Could not send - both must be combat submarines associated with the same mission.\n";
  }
}

void handleDisplayMessages(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Combat submarine serial: ");
  auto* messages = menu.messagesFor(serial);
  if (!messages) {
    std::cout << "  No such combat submarine.\n";
    return;
  }
  if (messages->empty()) {
    std::cout << "  No messages received.\n";
    return;
  }
  for (const auto& m : *messages) {
    std::cout << "  From " << m.senderSerial() << ": " << m.content() << "\n";
  }
}

void registerDashboardRoutes(HttpServer& server, DashboardApi& api) {
  server.get("/", [](const HttpRequest&) { return HttpResponse::html(kDashboardHtml); });
  server.get("/api/state", [&api](const HttpRequest&) { return HttpResponse::json(api.stateJson()); });

  server.post("/api/submarines", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.addSubmarine(req.param("type"), req.param("serial"),
                                                req.param("name"), req.param("port")));
  });
  server.post("/api/submarines/:serial/mission", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.assignMission(req.param("serial"), req.param("description"),
                                                 req.param("commanderName"), req.param("personnelCount"),
                                                 req.param("researchTopic"), req.param("researcherNames")));
  });
  server.post("/api/submarines/:serial/mission/update", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.updateMission(req.param("serial"), req.param("description"),
                                                 req.param("commanderName"), req.param("personnelCount"),
                                                 req.param("researchTopic"), req.param("researcherNames")));
  });
  server.post("/api/submarines/:serial/mission/end", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.endMission(req.param("serial")));
  });
  server.post("/api/submarines/:serial/participate", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.addParticipating(req.param("serial"), req.param("other")));
  });
  server.post("/api/messages", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.sendMessage(req.param("from"), req.param("to"), req.param("content")));
  });
}

}  // namespace

int main() {
  Fleet fleet;
  Menu menu(fleet);
  DashboardApi dashboardApi(menu, g_fleetMutex);

  const int kDashboardPort = 8080;
  HttpServer server(kDashboardPort);
  registerDashboardRoutes(server, dashboardApi);
  if (server.start()) {
    std::cout << "Web dashboard: http://localhost:" << kDashboardPort << "\n";
  } else {
    std::cout << "Web dashboard could not start on port " << kDashboardPort
              << " (already in use?) - continuing with console only.\n";
  }

  std::cout << "Submarine Fleet Management System\n";
  bool running = true;
  while (running) {
    printMenu();
    int choice = promptInt("");
    switch (choice) {
      case 1: handleAdd(menu); break;
      case 2: handleDisplayAll(menu); break;
      case 3: handleSearch(menu); break;
      case 4: handleAssignMission(menu); break;
      case 5: handleUpdateMission(menu); break;
      case 6: handleEndMission(menu); break;
      case 7: handleAddParticipating(menu); break;
      case 8: handleSendMessage(menu); break;
      case 9: handleDisplayMessages(menu); break;
      case 10: running = false; break;
      default: std::cout << "  Please choose 1-10.\n"; break;
    }
  }

  server.stop();
  std::cout << "Goodbye.\n";
  return 0;
}
