#ifndef CENTRAL_COMPUTER_TCP_SERVER_H
#define CENTRAL_COMPUTER_TCP_SERVER_H

#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>

namespace submarine {

struct TcpRequest {
  std::string command;
  std::vector<std::string> params;
};

struct TcpResponse {
  std::string body;

  static TcpResponse ok(const std::string& jsonData) {
    return TcpResponse{jsonData};
  }

  static TcpResponse error(const std::string& message) {
    std::string json = "{\"status\":\"error\",\"data\":\"" + message + "\"}";
    return TcpResponse{json};
  }
};

// TCP server for Ground Station read-only queries.
// Listens on specified port. Each connection receives one request/response,
// then closes. Handlers registered with on() are called for matching commands.
class TcpServer {
 public:
  explicit TcpServer(int port);
  ~TcpServer();

  TcpServer(const TcpServer&) = delete;
  TcpServer& operator=(const TcpServer&) = delete;

  using Handler = std::function<TcpResponse(const TcpRequest&)>;
  void on(const std::string& command, Handler handler);

  bool start();
  void stop();

  std::string errorMessage() const { return errorMessage_; }

 private:
  void acceptLoop();
  void handleConnection(int clientSocket);

  int port_;
  bool running_ = false;
  std::string errorMessage_;
  std::map<std::string, Handler> handlers_;

#if defined(_WIN32)
  std::uintptr_t listenSocket_ = 0;
#else
  int listenSocket_ = -1;
#endif
  void* acceptThread_ = nullptr;
};

}  // namespace submarine

#endif
