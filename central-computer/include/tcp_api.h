#ifndef CENTRAL_COMPUTER_TCP_API_H
#define CENTRAL_COMPUTER_TCP_API_H

#include <string>
#include <mutex>
#include "tcp_server.h"
#include "menu.h"

namespace submarine {

// API handlers for Ground Station TCP queries.
// All operations are read-only. Logs/events are read from each Combat
// submarine's own CentralComputer::dataStore() - only Combat submarines
// have a CentralComputer/hardware link (see combat_submarine.h); a
// Research submarine has no measurement/event history to query.
class TcpApi {
 public:
  TcpApi(Menu& menu, std::mutex& fleetMutex);

  std::string listSubmarines();
  std::string getLogs(uint32_t startYmd, uint32_t endYmd, const std::string& serial);
  std::string getEvents(uint32_t startYmd, uint32_t endYmd, const std::string& serial);
  std::string summaryReport();

 private:
  Menu& menu_;
  std::mutex& fleetMutex_;
};

}  // namespace submarine

#endif
