#include "http_server.h"

#include <cstring>
#include <sstream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace submarine {

namespace {

std::string urlDecode(const std::string& in) {
  std::string out;
  out.reserve(in.size());
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '%' && i + 2 < in.size()) {
      int value = 0;
      std::istringstream hex(in.substr(i + 1, 2));
      if (hex >> std::hex >> value) {
        out += static_cast<char>(value);
        i += 2;
      } else {
        out += in[i];
      }
    } else if (in[i] == '+') {
      out += ' ';
    } else {
      out += in[i];
    }
  }
  return out;
}

std::map<std::string, std::string> parseKeyValues(const std::string& s) {
  std::map<std::string, std::string> result;
  std::istringstream ss(s);
  std::string pair;
  while (std::getline(ss, pair, '&')) {
    if (pair.empty()) continue;
    auto eq = pair.find('=');
    if (eq == std::string::npos) {
      result[urlDecode(pair)] = "";
    } else {
      result[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
    }
  }
  return result;
}

std::vector<std::string> splitPath(const std::string& path) {
  std::vector<std::string> segments;
  std::istringstream ss(path);
  std::string seg;
  while (std::getline(ss, seg, '/')) {
    if (!seg.empty()) segments.push_back(seg);
  }
  return segments;
}

}  // namespace

HttpServer::HttpServer(int port) : port_(port) {}

HttpServer::~HttpServer() { stop(); }

void HttpServer::get(const std::string& pattern, HttpHandler handler) {
  routes_.push_back({"GET", splitPath(pattern), std::move(handler)});
}

void HttpServer::post(const std::string& pattern, HttpHandler handler) {
  routes_.push_back({"POST", splitPath(pattern), std::move(handler)});
}

bool HttpServer::matchRoute(const Route& route, HttpRequest& req) const {
  if (route.method != req.method) return false;
  auto reqSegments = splitPath(req.path);
  if (reqSegments.size() != route.segments.size()) return false;

  std::map<std::string, std::string> params;
  for (size_t i = 0; i < route.segments.size(); i++) {
    const std::string& pat = route.segments[i];
    if (!pat.empty() && pat[0] == ':') {
      params[pat.substr(1)] = reqSegments[i];
    } else if (pat != reqSegments[i]) {
      return false;
    }
  }
  req.pathParams = std::move(params);
  return true;
}

HttpResponse HttpServer::dispatch(HttpRequest& req) {
  for (const auto& route : routes_) {
    if (matchRoute(route, req)) return route.handler(req);
  }
  return HttpResponse::notFound();
}

#if defined(_WIN32)

namespace {
struct WinsockInit {
  WinsockInit() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
  }
  ~WinsockInit() { WSACleanup(); }
};
}  // namespace

bool HttpServer::start() {
  static WinsockInit winsockInit;  // once per process, harmless if constructed more than once

  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sock == INVALID_SOCKET) return false;

  BOOL reuse = TRUE;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost only - this is a local dev dashboard
  addr.sin_port = htons(static_cast<u_short>(port_));

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    closesocket(sock);
    return false;
  }
  if (listen(sock, 16) != 0) {
    closesocket(sock);
    return false;
  }

  listenSocket_ = static_cast<std::uintptr_t>(sock);
  running_ = true;
  acceptThread_ = std::thread(&HttpServer::acceptLoop, this);
  return true;
}

void HttpServer::stop() {
  if (!running_) return;
  running_ = false;
  closesocket(static_cast<SOCKET>(listenSocket_));
  if (acceptThread_.joinable()) acceptThread_.join();
}

void HttpServer::acceptLoop() {
  while (running_) {
    SOCKET client = accept(static_cast<SOCKET>(listenSocket_), nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      if (!running_) break;
      continue;
    }
    handleConnection(static_cast<int>(client));
  }
}

void HttpServer::handleConnection(int clientSocketInt) {
  SOCKET clientSocket = static_cast<SOCKET>(clientSocketInt);
  std::string request;
  char buf[4096];
  // Read until we have the header terminator; then, if Content-Length says
  // there's a body, keep reading until we have that many bytes past it.
  size_t headerEnd = std::string::npos;
  while (headerEnd == std::string::npos) {
    int n = recv(clientSocket, buf, sizeof(buf), 0);
    if (n <= 0) { closesocket(clientSocket); return; }
    request.append(buf, static_cast<size_t>(n));
    headerEnd = request.find("\r\n\r\n");
  }

  size_t contentLength = 0;
  {
    std::istringstream headerStream(request.substr(0, headerEnd));
    std::string line;
    while (std::getline(headerStream, line)) {
      if (line.rfind("Content-Length:", 0) == 0 || line.rfind("content-length:", 0) == 0) {
        contentLength = static_cast<size_t>(std::stoul(line.substr(line.find(':') + 1)));
      }
    }
  }
  size_t bodyStart = headerEnd + 4;
  while (request.size() - bodyStart < contentLength) {
    int n = recv(clientSocket, buf, sizeof(buf), 0);
    if (n <= 0) break;
    request.append(buf, static_cast<size_t>(n));
  }

  HttpRequest req;
  {
    std::istringstream lineStream(request.substr(0, request.find("\r\n")));
    std::string fullPath;
    lineStream >> req.method >> fullPath;
    auto qpos = fullPath.find('?');
    if (qpos == std::string::npos) {
      req.path = fullPath;
    } else {
      req.path = fullPath.substr(0, qpos);
      req.query = parseKeyValues(fullPath.substr(qpos + 1));
    }
  }
  if (contentLength > 0) {
    req.form = parseKeyValues(request.substr(bodyStart, contentLength));
  }

  HttpResponse resp = dispatch(req);
  std::ostringstream out;
  out << "HTTP/1.1 " << resp.status << " OK\r\n"
      << "Content-Type: " << resp.contentType << "\r\n"
      << "Content-Length: " << resp.body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << resp.body;
  std::string outStr = out.str();
  send(clientSocket, outStr.data(), static_cast<int>(outStr.size()), 0);
  closesocket(clientSocket);
}

#else  // POSIX

bool HttpServer::start() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return false;

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  // localhost only - local dev dashboard
  addr.sin_port = htons(static_cast<uint16_t>(port_));

  if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    return false;
  }
  if (listen(sock, 16) != 0) {
    ::close(sock);
    return false;
  }

  listenSocket_ = sock;
  running_ = true;
  acceptThread_ = std::thread(&HttpServer::acceptLoop, this);
  return true;
}

void HttpServer::stop() {
  if (!running_) return;
  running_ = false;
  ::shutdown(listenSocket_, SHUT_RDWR);
  ::close(listenSocket_);
  if (acceptThread_.joinable()) acceptThread_.join();
}

void HttpServer::acceptLoop() {
  while (running_) {
    int client = accept(listenSocket_, nullptr, nullptr);
    if (client < 0) {
      if (!running_) break;
      continue;
    }
    handleConnection(client);
  }
}

void HttpServer::handleConnection(int clientSocket) {
  std::string request;
  char buf[4096];
  size_t headerEnd = std::string::npos;
  while (headerEnd == std::string::npos) {
    ssize_t n = recv(clientSocket, buf, sizeof(buf), 0);
    if (n <= 0) { ::close(clientSocket); return; }
    request.append(buf, static_cast<size_t>(n));
    headerEnd = request.find("\r\n\r\n");
  }

  size_t contentLength = 0;
  {
    std::istringstream headerStream(request.substr(0, headerEnd));
    std::string line;
    while (std::getline(headerStream, line)) {
      if (line.rfind("Content-Length:", 0) == 0 || line.rfind("content-length:", 0) == 0) {
        contentLength = static_cast<size_t>(std::stoul(line.substr(line.find(':') + 1)));
      }
    }
  }
  size_t bodyStart = headerEnd + 4;
  while (request.size() - bodyStart < contentLength) {
    ssize_t n = recv(clientSocket, buf, sizeof(buf), 0);
    if (n <= 0) break;
    request.append(buf, static_cast<size_t>(n));
  }

  HttpRequest req;
  {
    std::istringstream lineStream(request.substr(0, request.find("\r\n")));
    std::string fullPath;
    lineStream >> req.method >> fullPath;
    auto qpos = fullPath.find('?');
    if (qpos == std::string::npos) {
      req.path = fullPath;
    } else {
      req.path = fullPath.substr(0, qpos);
      req.query = parseKeyValues(fullPath.substr(qpos + 1));
    }
  }
  if (contentLength > 0) {
    req.form = parseKeyValues(request.substr(bodyStart, contentLength));
  }

  HttpResponse resp = dispatch(req);
  std::ostringstream out;
  out << "HTTP/1.1 " << resp.status << " OK\r\n"
      << "Content-Type: " << resp.contentType << "\r\n"
      << "Content-Length: " << resp.body.size() << "\r\n"
      << "Connection: close\r\n\r\n"
      << resp.body;
  std::string outStr = out.str();
  send(clientSocket, outStr.data(), outStr.size(), 0);
  ::close(clientSocket);
}

#endif

}  // namespace submarine
