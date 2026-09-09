#ifndef GROUND_STATION_CLI_H
#define GROUND_STATION_CLI_H

#include <memory>
#include "tcp_client.h"

namespace ground_station {

// CLI menu for the Ground Station - read-only interface to the Central Computer.
// Allows querying:
//   - List all submarines
//   - Get logs for a date range and submarine
//   - Get events for a date range and submarine
//   - Display a summary report
//
class GroundStationCli {
 public:
  explicit GroundStationCli(std::unique_ptr<TcpClient> client);

  // Runs the interactive CLI menu loop.
  // Returns false on fatal errors (e.g., connection lost).
  bool run();

  // Exposes the underlying TcpClient so main() can also wire up the
  // read-only web dashboard's HTTP routes against the same connection
  // (TcpClient::sendRequest() is mutex-protected for exactly this reason).
  TcpClient& client() { return *client_; }

 private:
  void printMenu() const;
  bool handleListSubmarines();
  bool handleGetLogs();
  bool handleGetEvents();
  bool handleSummaryReport();

  std::unique_ptr<TcpClient> client_;
};

}  // namespace ground_station

#endif  // GROUND_STATION_CLI_H
