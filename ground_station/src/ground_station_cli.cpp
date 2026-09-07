#include "ground_station_cli.h"

#include <iostream>
#include <limits>
#include <sstream>

namespace ground_station {

GroundStationCli::GroundStationCli(std::unique_ptr<TcpClient> client)
    : client_(std::move(client)) {}

bool GroundStationCli::run() {
  if (!client_->connect()) {
    std::cerr << "Could not connect to Central Computer: " << client_->errorMessage() << "\n";
    return false;
  }

  std::cout << "Ground Station - Connected to Central Computer\n";
  std::cout << "All operations are read-only.\n";

  bool running = true;
  while (running) {
    printMenu();

    std::string choice;
    std::getline(std::cin, choice);

    switch (choice[0]) {
      case '1': running = handleListSubmarines(); break;
      case '2': running = handleGetLogs(); break;
      case '3': running = handleGetEvents(); break;
      case '4': running = handleSummaryReport(); break;
      case '5': running = false; break;
      default:
        std::cout << "  Please choose 1-5.\n";
        break;
    }
  }

  client_->disconnect();
  std::cout << "Goodbye.\n";
  return true;
}

void GroundStationCli::printMenu() const {
  std::cout << "\n=== Ground Station Menu (Read-Only) ===\n"
            << " 1. List all submarines\n"
            << " 2. Get logs for a date range and submarine\n"
            << " 3. Get events for a date range and submarine\n"
            << " 4. Display summary report\n"
            << " 5. Exit\n"
            << "Choice: ";
}

bool GroundStationCli::handleListSubmarines() {
  std::string response = client_->sendRequest("LIST_SUBMARINES");
  if (response.empty()) {
    std::cerr << "Error: " << client_->errorMessage() << "\n";
    return false;
  }
  std::cout << "\n--- Submarine List ---\n" << response << "\n";
  return true;
}

bool GroundStationCli::handleGetLogs() {
  std::cout << "  Submarine serial: ";
  std::string serial;
  std::getline(std::cin, serial);

  std::cout << "  Start date (YYYYMMDD): ";
  std::string startStr;
  std::getline(std::cin, startStr);

  std::cout << "  End date (YYYYMMDD): ";
  std::string endStr;
  std::getline(std::cin, endStr);

  std::string command = "GET_LOGS," + startStr + "," + endStr + "," + serial;
  std::string response = client_->sendRequest(command);
  if (response.empty()) {
    std::cerr << "Error: " << client_->errorMessage() << "\n";
    return false;
  }
  std::cout << "\n--- Logs ---\n" << response << "\n";
  return true;
}

bool GroundStationCli::handleGetEvents() {
  std::cout << "  Submarine serial: ";
  std::string serial;
  std::getline(std::cin, serial);

  std::cout << "  Start date (YYYYMMDD): ";
  std::string startStr;
  std::getline(std::cin, startStr);

  std::cout << "  End date (YYYYMMDD): ";
  std::string endStr;
  std::getline(std::cin, endStr);

  std::string command = "GET_EVENTS," + startStr + "," + endStr + "," + serial;
  std::string response = client_->sendRequest(command);
  if (response.empty()) {
    std::cerr << "Error: " << client_->errorMessage() << "\n";
    return false;
  }
  std::cout << "\n--- Events ---\n" << response << "\n";
  return true;
}

bool GroundStationCli::handleSummaryReport() {
  std::string response = client_->sendRequest("SUMMARY_REPORT");
  if (response.empty()) {
    std::cerr << "Error: " << client_->errorMessage() << "\n";
    return false;
  }
  std::cout << "\n--- Summary Report ---\n" << response << "\n";
  return true;
}

}  // namespace ground_station
