#include "tcp_client.h"

#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

namespace ground_station {

TcpClient::TcpClient(const std::string& host, int port)
    : host_(host), port_(port) {}

TcpClient::~TcpClient() {
  disconnect();
}

bool TcpClient::connect() {
  if (isConnected()) {
    return true;
  }

#if defined(_WIN32)
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    errorMessage_ = "WSAStartup failed";
    return false;
  }
#endif

  struct addrinfo hints = {};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  struct addrinfo* result = nullptr;
  std::string portStr = std::to_string(port_);
  if (getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &result) != 0) {
    errorMessage_ = "getaddrinfo failed for " + host_ + ":" + portStr;
#if defined(_WIN32)
    WSACleanup();
#endif
    return false;
  }

#if defined(_WIN32)
  socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (socket_ == 0) {
    errorMessage_ = "socket() failed";
    freeaddrinfo(result);
    WSACleanup();
    return false;
  }
  if (::connect((SOCKET)socket_, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
    errorMessage_ = "connect() failed";
    closesocket((SOCKET)socket_);
    socket_ = 0;
    freeaddrinfo(result);
    WSACleanup();
    return false;
  }
#else
  socket_ = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (socket_ < 0) {
    errorMessage_ = "socket() failed";
    freeaddrinfo(result);
    return false;
  }
  if (::connect(socket_, result->ai_addr, result->ai_addrlen) < 0) {
    errorMessage_ = "connect() failed";
    close(socket_);
    socket_ = -1;
    freeaddrinfo(result);
    return false;
  }
#endif

  freeaddrinfo(result);
  errorMessage_.clear();
  return true;
}

bool TcpClient::isConnected() const {
#if defined(_WIN32)
  return socket_ != 0;
#else
  return socket_ >= 0;
#endif
}

std::string TcpClient::sendRequest(const std::string& command) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!isConnected()) {
    errorMessage_ = "Not connected";
    return "";
  }

  // Send request with newline
  std::string request = command + "\n";
#if defined(_WIN32)
  if (send((SOCKET)socket_, request.c_str(), (int)request.size(), 0) == SOCKET_ERROR) {
    errorMessage_ = "send() failed";
    return "";
  }
#else
  if (::send(socket_, request.c_str(), request.size(), 0) < 0) {
    errorMessage_ = "send() failed";
    return "";
  }
#endif

  // Read response until "\n.\n"
  return readUntilTerminator();
}

std::string TcpClient::readUntilTerminator() {
  std::string response;
  char buffer[4096];
  const std::string terminator = "\n.\n";

  while (true) {
#if defined(_WIN32)
    int bytesRead = recv((SOCKET)socket_, buffer, sizeof(buffer), 0);
    if (bytesRead == SOCKET_ERROR) {
      errorMessage_ = "recv() failed";
      return "";
    }
    if (bytesRead == 0) {
      errorMessage_ = "Connection closed by server";
      return "";
    }
#else
    int bytesRead = ::recv(socket_, buffer, sizeof(buffer), 0);
    if (bytesRead < 0) {
      errorMessage_ = "recv() failed";
      return "";
    }
    if (bytesRead == 0) {
      errorMessage_ = "Connection closed by server";
      return "";
    }
#endif

    response.append(buffer, bytesRead);

    // Check if we have the terminator
    size_t pos = response.find(terminator);
    if (pos != std::string::npos) {
      // Return everything before the terminator
      return response.substr(0, pos);
    }
  }
}

void TcpClient::disconnect() {
  if (!isConnected()) {
    return;
  }
#if defined(_WIN32)
  closesocket((SOCKET)socket_);
  socket_ = 0;
  WSACleanup();
#else
  close(socket_);
  socket_ = -1;
#endif
}

}  // namespace ground_station
