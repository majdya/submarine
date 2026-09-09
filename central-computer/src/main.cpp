// Submarine Fleet Management System - console entry point.
//
// Implements the spec's "OOP Part" menu (10 numbered operations) on top of
// Fleet/Menu. Every submarine - research or combat, a deliberate extension
// beyond the spec's literal "belongs to each combat submarine" wording -
// has its own CentralComputer, able to be wired to the real LNC hardware
// over a serial port (see transport/serial_transport.h) using the exact
// same protocol code the firmware itself was verified against (see
// include/protocol_bridge.h). Only one physical LNC exists on a given
// bench, so in practice only one submarine at a time is created with a
// real port; every other one defaults to a disconnected placeholder
// (CentralComputer's default LoopbackTransport).
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
//
// A read-only TCP server (tcp_server.h/tcp_api.h) also runs alongside,
// for the separate Ground Station binary (ground_station/) to query the
// same Fleet remotely - see GROUND_STATION_PROTOCOL.md for the wire
// protocol. It shares g_fleetMutex too, and TcpApi reads logs/events
// straight from each Combat submarine's own CentralComputer::dataStore().

#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>

#include "dashboard_api.h"
#include "http_server.h"
#include "tcp_server.h"
#include "tcp_api.h"
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

// Every fixed-set choice in this console goes through here - by operator
// request, none of them are typed words anymore (no "research"/"combat",
// no "y"/"n"): prints a numbered list and loops until the operator picks
// a valid number, returning a 1-based index into `options`.
int promptChoice(const std::string& label, const std::vector<std::string>& options) {
  while (true) {
    std::cout << label << "\n";
    for (size_t i = 0; i < options.size(); i++) {
      std::cout << "    " << (i + 1) << ". " << options[i] << "\n";
    }
    int choice = promptInt("  Choice: ");
    if (choice >= 1 && static_cast<size_t>(choice) <= options.size()) return choice;
    std::cout << "  Please enter a number between 1 and " << options.size() << ".\n";
  }
}

// Yes/no as 1/2 specifically, per operator request - never typed y/n.
bool promptYesNo(const std::string& label) { return promptChoice(label, {"Yes", "No"}) == 1; }

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
            << "11. [Extra, beyond spec's 10 ops] Set a submarine's sensor limits / enable-disable (LNC hardware)\n"
            << "Choice: ";
}

// NOTE: each handler below takes g_fleetMutex for its *entire* body
// (including its console prompts) - see the file header comment for why.

void handleAdd(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  int typeChoice = promptChoice("  Submarine type:", {"Research", "Combat"});
  std::string serial = promptLine("  Serial number: ");
  std::string name = promptLine("  Name: ");

  // Every submarine can be wired to real LNC hardware now, research
  // included (see submarine.h's class comment on this deliberate spec
  // deviation) - promptCentralComputer() itself already handles "no
  // hardware for this one" via a blank port.
  Submarine* created = nullptr;
  if (typeChoice == 2) {  // Combat
    created = menu.addCombatSubmarine(serial, name, promptCentralComputer(serial));
  } else {  // Research
    created = menu.addResearchSubmarine(serial, name, promptCentralComputer(serial));
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

// Beyond the spec's 10 numbered OOP-part menu operations: wires the
// Management Command module's setLimits() (spec S2.5/S3.2, required -
// "Set the temperature range...", etc.) into an interface an operator can
// actually use. Firmware-side decode/apply/persist verified 2026-09-07;
// this closes the previously-open gap where nothing called it. Also
// carries the optional per-sensor enable/disable toggle (TAG_ENABLED) - a
// deliberate extension beyond the spec, added at the project owner's
// explicit request, for exactly the 4 environmental sensors here.
void handleSetLimits(Menu& menu) {
  std::lock_guard<std::mutex> lock(g_fleetMutex);
  std::string serial = promptLine("  Submarine serial: ");
  auto* sub = menu.search(serial);
  if (!sub) {
    std::cout << "  No submarine with serial '" << serial << "'.\n";
    return;
  }

  int paramChoice = promptChoice("  Parameter:", {"Temperature", "Humidity", "Light", "Battery"});
  bool isTemp = (paramChoice == 1);
  uint8_t param = 0;
  switch (paramChoice) {
    case 1: param = PARAM_TEMP; break;
    case 2: param = PARAM_HUMIDITY; break;
    case 3: param = PARAM_LIGHT; break;
    case 4: param = PARAM_BATTERY; break;
  }

  std::optional<int32_t> normalMin, normalMax, warningMin, warningMax;
  if (promptYesNo("  Change this parameter's limits too?")) {
    normalMin = promptInt(isTemp ? "  Normal range minimum: " : "  Normal lower boundary: ");
    if (isTemp) normalMax = promptInt("  Normal range maximum: ");
    warningMin = promptInt(isTemp ? "  Warning range minimum: " : "  Warning lower boundary: ");
    if (isTemp) warningMax = promptInt("  Warning range maximum: ");
  }

  int enabledChoice = promptChoice("  Sensor enabled state:", {"Enable", "Disable", "Leave unchanged"});
  std::optional<bool> enabled;
  if (enabledChoice == 1) enabled = true;
  else if (enabledChoice == 2) enabled = false;
  // 3 = leave unchanged -> enabled stays nullopt

  if (!normalMin && !enabled) {
    std::cout << "  Nothing to do - no limits changed and no enable/disable chosen.\n";
    return;
  }

  bool ok =
      sub->centralComputer().commands().setLimits(param, normalMin, normalMax, warningMin, warningMax, enabled);
  if (ok) {
    std::cout << "  Updated and acknowledged by the LNC.\n";
  } else {
    std::cout << "  Failed - no ACK from the LNC (check the connection), or the command was rejected.\n";
  }
}

// The dashboard is a plain static HTML/CSS/JS file at web/dashboard.html
// (relative to wherever central_computer is launched from - same
// convention as the logs/<serial> and data/<serial> directories), not
// embedded in the binary: a normal webapp you can edit and reload
// without recompiling, rather than a C++ string to edit around. Read
// fresh on every GET / rather than cached once at startup, so an edit
// shows up on the next browser refresh.
std::string loadDashboardHtml() {
  std::ifstream file("web/dashboard.html", std::ios::binary);
  if (!file) {
    return "<html><body><h1>web/dashboard.html not found</h1>"
           "<p>Expected at <code>web/dashboard.html</code>, relative to the "
           "directory central_computer was launched from.</p></body></html>";
  }
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

void registerDashboardRoutes(HttpServer& server, DashboardApi& api) {
  server.get("/", [](const HttpRequest&) { return HttpResponse::html(loadDashboardHtml()); });
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
  server.post("/api/submarines/:serial/limits", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.setLimits(req.param("serial"), req.param("param"), req.param("normalMin"),
                                             req.param("normalMax"), req.param("warningMin"),
                                             req.param("warningMax"), req.param("enabled")));
  });
}

void registerTcpServerHandlers(TcpServer& server, TcpApi& api) {
  server.on("LIST_SUBMARINES", [&api](const TcpRequest&) {
    return TcpResponse::ok(api.listSubmarines());
  });

  server.on("GET_LOGS", [&api](const TcpRequest& req) {
    if (req.params.size() < 3) {
      return TcpResponse::error("GET_LOGS requires 3 parameters: start_ymd,end_ymd,serial");
    }
    try {
      uint32_t startYmd = std::stoul(req.params[0]);
      uint32_t endYmd = std::stoul(req.params[1]);
      const std::string& serial = req.params[2];
      return TcpResponse::ok(api.getLogs(startYmd, endYmd, serial));
    } catch (...) {
      return TcpResponse::error("Invalid parameters for GET_LOGS");
    }
  });

  server.on("GET_EVENTS", [&api](const TcpRequest& req) {
    if (req.params.size() < 3) {
      return TcpResponse::error("GET_EVENTS requires 3 parameters: start_ymd,end_ymd,serial");
    }
    try {
      uint32_t startYmd = std::stoul(req.params[0]);
      uint32_t endYmd = std::stoul(req.params[1]);
      const std::string& serial = req.params[2];
      return TcpResponse::ok(api.getEvents(startYmd, endYmd, serial));
    } catch (...) {
      return TcpResponse::error("Invalid parameters for GET_EVENTS");
    }
  });

  server.on("SUMMARY_REPORT", [&api](const TcpRequest&) {
    return TcpResponse::ok(api.summaryReport());
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

  // Start the TCP server for Ground Station connections (read-only).
  // TcpApi reads logs/events straight from each submarine's own
  // CentralComputer::dataStore() - there is no separate/shared data store.
  TcpApi tcpApi(menu, g_fleetMutex);
  const int kTcpServerPort = 9000;
  TcpServer tcpServer(kTcpServerPort);
  registerTcpServerHandlers(tcpServer, tcpApi);
  if (tcpServer.start()) {
    std::cout << "Ground Station server: localhost:" << kTcpServerPort << "\n";
  } else {
    std::cout << "Ground Station server could not start on port " << kTcpServerPort
              << " - Error: " << tcpServer.errorMessage() << "\n";
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
      case 11: handleSetLimits(menu); break;
      default: std::cout << "  Please choose 1-11.\n"; break;
    }
  }

  server.stop();
  tcpServer.stop();
  std::cout << "Goodbye.\n";
  return 0;
}
