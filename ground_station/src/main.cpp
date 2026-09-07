// Ground Station - Read-only TCP client for the Submarine Fleet Management System
//
// Connects to the Central Computer's TCP server and provides a CLI menu for:
//   - Listing all submarines
//   - Querying logs for a date range
//   - Querying events for a date range
//   - Displaying a summary report
//
// All operations are read-only - no mutations to the Fleet occur in this binary.
// It communicates with the Central Computer via TCP using a simple
// request/response protocol with JSON payloads.

#include <iostream>
#include "ground_station_cli.h"

using namespace ground_station;

int main(int argc, char* argv[]) {
  std::string host = "localhost";
  int port = 9000;  // Default port for Central Computer's read-only server

  // Simple argument parsing
  if (argc > 1) {
    host = argv[1];
  }
  if (argc > 2) {
    port = std::stoi(argv[2]);
  }

  std::cout << "Ground Station\n";
  std::cout << "Connecting to Central Computer at " << host << ":" << port << "\n";

  auto client = std::make_unique<TcpClient>(host, port);
  GroundStationCli cli(std::move(client));

  if (!cli.run()) {
    return 1;
  }

  return 0;
}
