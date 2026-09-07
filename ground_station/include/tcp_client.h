#ifndef GROUND_STATION_TCP_CLIENT_H
#define GROUND_STATION_TCP_CLIENT_H

#include <string>
#include <memory>

namespace ground_station {

// TCP client that connects to the Central Computer's read-only server.
// Implements a simple request/response protocol:
//
//   REQUEST: "COMMAND\n"
//   RESPONSE: JSON response body (may span multiple lines) followed by "\n.\n"
//
// Request format:
//   - LIST_SUBMARINES - Get all submarines in the fleet
//   - GET_LOGS,<start_ymd>,<end_ymd>,<submarine_serial> - Get logs for a date range
//   - GET_EVENTS,<start_ymd>,<end_ymd>,<submarine_serial> - Get events for a date range
//   - SUMMARY_REPORT - Get a summary of the entire fleet
//
// Responses are always JSON, with a "status" field ("ok" or "error") and a
// "data" field containing the actual result (or error message if status is "error").
//
class TcpClient {
 public:
  explicit TcpClient(const std::string& host, int port);
  ~TcpClient();

  TcpClient(const TcpClient&) = delete;
  TcpClient& operator=(const TcpClient&) = delete;

  // Attempts to connect to the Central Computer.
  // Returns true if successful, false otherwise.
  bool connect();

  // Checks if currently connected.
  bool isConnected() const;

  // Sends a request and reads the response.
  // Returns the JSON response body as a string, or empty string on error.
  std::string sendRequest(const std::string& command);

  // Disconnects from the server.
  void disconnect();

  std::string errorMessage() const { return errorMessage_; }

 private:
  std::string readUntilTerminator();  // Reads until "\n.\n"

  std::string host_;
  int port_;
  std::string errorMessage_;
#if defined(_WIN32)
  std::uintptr_t socket_ = 0;  // SOCKET, kept as integer to avoid winsock2.h
#else
  int socket_ = -1;
#endif
};

}  // namespace ground_station

#endif  // GROUND_STATION_TCP_CLIENT_H
