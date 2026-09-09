#include "tcp_server.h"
#include <thread>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#endif

namespace submarine {

#if defined(_WIN32)
struct WsaInit {
  WsaInit() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
  }
};
static WsaInit g_wsaInit;
#endif

TcpServer::TcpServer(int port) : port_(port) {}

TcpServer::~TcpServer() {
  stop();
}

void TcpServer::on(const std::string& command, Handler handler) {
  handlers_[command] = handler;
}

bool TcpServer::start() {
  if (running_) {
    return true;
  }

  struct sockaddr_in addr {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(static_cast<uint16_t>(port_));

#if defined(_WIN32)
  listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket_ == 0) {
    errorMessage_ = "socket() failed";
    return false;
  }
  
  int opt = 1;
  if (setsockopt((SOCKET)listenSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
    errorMessage_ = "setsockopt() failed";
    closesocket((SOCKET)listenSocket_);
    listenSocket_ = 0;
    return false;
  }
  
  if (bind((SOCKET)listenSocket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
    errorMessage_ = "bind() failed";
    closesocket((SOCKET)listenSocket_);
    listenSocket_ = 0;
    return false;
  }
  if (listen((SOCKET)listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
    errorMessage_ = "listen() failed";
    closesocket((SOCKET)listenSocket_);
    listenSocket_ = 0;
    return false;
  }
#else
  listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket_ < 0) {
    errorMessage_ = "socket() failed";
    return false;
  }
  int opt = 1;
  if (setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) < 0) {
    errorMessage_ = "setsockopt() failed";
    close(listenSocket_);
    listenSocket_ = -1;
    return false;
  }
  if (bind(listenSocket_, (sockaddr*)&addr, sizeof(addr)) < 0) {
    errorMessage_ = "bind() failed";
    close(listenSocket_);
    listenSocket_ = -1;
    return false;
  }
  if (listen(listenSocket_, SOMAXCONN) < 0) {
    errorMessage_ = "listen() failed";
    close(listenSocket_);
    listenSocket_ = -1;
    return false;
  }
#endif

  running_ = true;
  acceptThread_ = new std::thread(&TcpServer::acceptLoop, this);
  errorMessage_.clear();
  return true;
}

void TcpServer::stop() {
  if (!running_) {
    return;
  }
  running_ = false;

#if defined(_WIN32)
  if (listenSocket_ != 0) {
    closesocket((SOCKET)listenSocket_);
    listenSocket_ = 0;
  }
#else
  if (listenSocket_ >= 0) {
    close(listenSocket_);
    listenSocket_ = -1;
  }
#endif

  if (acceptThread_) {
    std::thread* t = static_cast<std::thread*>(acceptThread_);
    if (t->joinable()) t->join();
    delete t;
    acceptThread_ = nullptr;
  }
}

void TcpServer::acceptLoop() {
  while (running_) {
    struct sockaddr_in clientAddr {};
    socklen_t addrLen = sizeof(clientAddr);
    
#if defined(_WIN32)
    SOCKET clientSocket = accept((SOCKET)listenSocket_, (sockaddr*)&clientAddr, &addrLen);
    if (clientSocket == INVALID_SOCKET) {
      if (running_) continue;
      break;
    }
    handleConnection((int)clientSocket);
    closesocket(clientSocket);
#else
    int clientSocket = accept(listenSocket_, (sockaddr*)&clientAddr, &addrLen);
    if (clientSocket < 0) {
      if (running_) continue;
      break;
    }
    handleConnection(clientSocket);
    close(clientSocket);
#endif
  }
}

// Handles one accepted connection for as long as the client keeps it open,
// processing one command per line (Ground Station's TcpClient connects once
// and reuses that socket for every menu choice - see ground_station/src/tcp_client.cpp -
// so the server must not close the connection after a single reply).
void TcpServer::handleConnection(int clientSocket) {
  while (running_) {
    char buffer[4096];
#if defined(_WIN32)
    int bytesRead = recv((SOCKET)clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead == SOCKET_ERROR || bytesRead == 0) {
      return;  // client closed or error - end this connection
    }
#else
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
      return;
    }
#endif

    buffer[bytesRead] = '\0';
    std::string requestStr(buffer);

    std::stringstream ss(requestStr);
    std::string line;
    std::getline(ss, line);

    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty() && line.back() == '\n') line.pop_back();

    std::vector<std::string> parts;
    std::stringstream partsSS(line);
    std::string part;
    while (std::getline(partsSS, part, ',')) {
      parts.push_back(part);
    }

    if (parts.empty()) {
      continue;
    }

    std::string command = parts[0];
    std::vector<std::string> params(parts.begin() + 1, parts.end());

    TcpRequest req{command, params};
    TcpResponse resp = TcpResponse::error("Unknown command: " + command);

    auto it = handlers_.find(command);
    if (it != handlers_.end()) {
      resp = it->second(req);
    }

    std::string response = resp.body + "\n.\n";
#if defined(_WIN32)
    send((SOCKET)clientSocket, response.c_str(), (int)response.size(), 0);
#else
    ::send(clientSocket, response.c_str(), response.size(), 0);
#endif
  }
}

}
