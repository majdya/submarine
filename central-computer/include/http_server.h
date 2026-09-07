#ifndef CENTRAL_COMPUTER_HTTP_SERVER_H
#define CENTRAL_COMPUTER_HTTP_SERVER_H

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace submarine {

// A minimal, dependency-free embedded HTTP server for the fleet dashboard
// - deliberately hand-rolled (raw sockets, POSIX/Win32 behind one
// interface, same pattern as transport/serial_transport.h) rather than
// pulling in a library, since this project's build has no network access
// to fetch one. It is not a general-purpose web server: single request
// per connection (no keep-alive), GET query strings and POST bodies are
// both simple key=value pairs (application/x-www-form-urlencoded) so the
// dashboard never needs a JSON *parser* - only json_util.h's JSON
// *writer* for responses. That is enough for a local single-user
// dashboard talking to its own page's JS.
struct HttpRequest {
  std::string method;
  std::string path;                        // without query string
  std::map<std::string, std::string> query;  // parsed "?a=1&b=2"
  std::map<std::string, std::string> form;   // parsed POST body (same encoding as query)
  std::map<std::string, std::string> pathParams;  // from ":name" segments in the route pattern

  std::string param(const std::string& key, const std::string& def = "") const {
    auto it = pathParams.find(key);
    if (it != pathParams.end()) return it->second;
    auto it2 = form.find(key);
    if (it2 != form.end()) return it2->second;
    auto it3 = query.find(key);
    if (it3 != query.end()) return it3->second;
    return def;
  }
};

struct HttpResponse {
  int status = 200;
  std::string contentType = "text/plain";
  std::string body;

  static HttpResponse json(const std::string& body, int status = 200) {
    return {status, "application/json", body};
  }
  static HttpResponse html(const std::string& body, int status = 200) {
    return {status, "text/html; charset=utf-8", body};
  }
  static HttpResponse notFound() { return {404, "text/plain", "not found"}; }
};

using HttpHandler = std::function<HttpResponse(const HttpRequest&)>;

class HttpServer {
 public:
  explicit HttpServer(int port);
  ~HttpServer();

  HttpServer(const HttpServer&) = delete;
  HttpServer& operator=(const HttpServer&) = delete;

  // Route patterns are literal path segments plus ":name" placeholders,
  // e.g. "/api/submarines/:serial/mission". Registration order doesn't
  // matter - the first pattern (for the right method) whose segment count
  // and literal segments match wins.
  void get(const std::string& pattern, HttpHandler handler);
  void post(const std::string& pattern, HttpHandler handler);

  // Binds and starts accepting connections on a background thread.
  // Returns false if the port couldn't be bound.
  bool start();
  void stop();

  int port() const { return port_; }

 private:
  struct Route {
    std::string method;
    std::vector<std::string> segments;
    HttpHandler handler;
  };

  void acceptLoop();
  void handleConnection(int clientSocket);
  HttpResponse dispatch(HttpRequest& req);
  bool matchRoute(const Route& route, HttpRequest& req) const;

  int port_;
  std::vector<Route> routes_;
  std::atomic<bool> running_{false};
  std::thread acceptThread_;
#if defined(_WIN32)
  std::uintptr_t listenSocket_ = 0;  // SOCKET, kept as an integer type to avoid winsock2.h in this header
#else
  int listenSocket_ = -1;
#endif
};

}  // namespace submarine

#endif  // CENTRAL_COMPUTER_HTTP_SERVER_H
