// Exercises the dashboard's HTTP layer end-to-end: starts a real
// HttpServer wired to a real DashboardApi/Menu/Fleet (no mocking - this is
// the same kind of "against the real thing" testing the rest of this
// project uses), issues raw HTTP requests at it exactly like a browser's
// fetch() would, and checks the JSON responses. This is deliberately a
// small, direct socket client rather than a second HTTP library, mirroring
// the transport tests' own "hand-write the minimum, no downloaded deps"
// approach.
#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>

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

#include "dashboard_api.h"
#include "fleet.h"
#include "http_server.h"
#include "menu.h"
#include "test_harness.h"

namespace fs = std::filesystem;
using namespace submarine;

namespace {

#if defined(_WIN32)
struct WinsockInitTest {
  WinsockInitTest() {
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
  }
};
#endif

// Sends one raw HTTP/1.1 request to 127.0.0.1:port and returns the
// response's status line, headers and body as one string. Blocks briefly
// waiting for the connection; the server side always closes after one
// response (see http_server.cpp - "Connection: close"), so a plain
// recv-until-EOF loop is enough.
std::string rawHttpRequest(int port, const std::string& method, const std::string& path,
                            const std::string& body = "") {
#if defined(_WIN32)
  static WinsockInitTest winsockInit;
  SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
  int sock = socket(AF_INET, SOCK_STREAM, 0);
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    return "";
  }

  std::ostringstream req;
  req << method << " " << path << " HTTP/1.1\r\n"
      << "Host: localhost\r\n";
  if (!body.empty()) {
    req << "Content-Type: application/x-www-form-urlencoded\r\n"
        << "Content-Length: " << body.size() << "\r\n";
  }
  req << "\r\n" << body;
  std::string reqStr = req.str();

#if defined(_WIN32)
  send(sock, reqStr.data(), static_cast<int>(reqStr.size()), 0);
#else
  send(sock, reqStr.data(), reqStr.size(), 0);
#endif

  std::string response;
  char buf[4096];
  while (true) {
#if defined(_WIN32)
    int n = recv(sock, buf, sizeof(buf), 0);
#else
    ssize_t n = recv(sock, buf, sizeof(buf), 0);
#endif
    if (n <= 0) break;
    response.append(buf, static_cast<size_t>(n));
  }

#if defined(_WIN32)
  closesocket(sock);
#else
  ::close(sock);
#endif
  return response;
}

std::string bodyOf(const std::string& httpResponse) {
  auto pos = httpResponse.find("\r\n\r\n");
  return pos == std::string::npos ? "" : httpResponse.substr(pos + 4);
}

bool statusIs200(const std::string& httpResponse) {
  return httpResponse.rfind("HTTP/1.1 200", 0) == 0;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

static void test_dashboard_http_layer() {
  Fleet fleet;
  Menu menu(fleet);
  std::mutex fleetMutex;
  DashboardApi api(menu, fleetMutex);

  // Port chosen away from the app's own default (8080) so this test can
  // run even while a real instance happens to be up on the same machine.
  const int kTestPort = 18080;
  HttpServer server(kTestPort);
  server.get("/api/state", [&api](const HttpRequest&) { return HttpResponse::json(api.stateJson()); });
  server.post("/api/submarines", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.addSubmarine(req.param("type"), req.param("serial"),
                                                req.param("name"), req.param("port")));
  });
  server.post("/api/submarines/:serial/mission", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.assignMission(req.param("serial"), req.param("description"),
                                                 req.param("commanderName"), req.param("personnelCount"),
                                                 "", ""));
  });
  server.post("/api/submarines/:serial/mission/end", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.endMission(req.param("serial")));
  });

  CHECK(server.start());
  // Give the accept thread a moment to actually be listening.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Empty fleet.
  auto resp = rawHttpRequest(kTestPort, "GET", "/api/state");
  CHECK(statusIs200(resp));
  CHECK(contains(bodyOf(resp), R"("submarines":[])"));

  // Add a combat submarine with no hardware attached.
  resp = rawHttpRequest(kTestPort, "POST", "/api/submarines", "type=combat&serial=C-9&name=Cutlass&port=");
  CHECK(statusIs200(resp));
  CHECK(contains(bodyOf(resp), R"("ok":true)"));

  // Duplicate serial must fail cleanly with ok:false, not crash the server.
  resp = rawHttpRequest(kTestPort, "POST", "/api/submarines", "type=combat&serial=C-9&name=Dupe&port=");
  CHECK(contains(bodyOf(resp), R"("ok":false)"));

  // State now shows the one submarine, unassigned and disconnected.
  resp = rawHttpRequest(kTestPort, "GET", "/api/state");
  std::string stateBody = bodyOf(resp);
  CHECK(contains(stateBody, R"("serial":"C-9")"));
  CHECK(contains(stateBody, R"("type":"Combat")"));
  CHECK(contains(stateBody, R"("assigned":false)"));
  CHECK(contains(stateBody, R"("connected":false)"));

  // Assign a mission via the path-param route (":serial" extraction).
  resp = rawHttpRequest(kTestPort, "POST", "/api/submarines/C-9/mission",
                        "description=Patrol+the+strait&commanderName=Cmdr+Reyes&personnelCount=12");
  CHECK(contains(bodyOf(resp), R"("ok":true)"));

  resp = rawHttpRequest(kTestPort, "GET", "/api/state");
  stateBody = bodyOf(resp);
  CHECK(contains(stateBody, R"("assigned":true)"));
  CHECK(contains(stateBody, "Patrol the strait"));
  CHECK(contains(stateBody, "Cmdr Reyes"));

  // Assigning again while already assigned must fail.
  resp = rawHttpRequest(kTestPort, "POST", "/api/submarines/C-9/mission",
                        "description=Second&commanderName=X&personnelCount=1");
  CHECK(contains(bodyOf(resp), R"("ok":false)"));

  // End the mission, then confirm it is available again.
  resp = rawHttpRequest(kTestPort, "POST", "/api/submarines/C-9/mission/end", "");
  CHECK(contains(bodyOf(resp), R"("ok":true)"));
  resp = rawHttpRequest(kTestPort, "GET", "/api/state");
  CHECK(contains(bodyOf(resp), R"("assigned":false)"));
  CHECK(contains(bodyOf(resp), R"("missionHistoryCount":1)"));

  // Unknown route -> 404, server still healthy afterwards.
  resp = rawHttpRequest(kTestPort, "GET", "/nope");
  CHECK(resp.rfind("HTTP/1.1 404", 0) == 0);
  resp = rawHttpRequest(kTestPort, "GET", "/api/state");
  CHECK(statusIs200(resp));

  server.stop();
}

int main() {
  test_dashboard_http_layer();
  TEST_MAIN_EXIT();
}
