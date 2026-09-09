// Ground Station - Read-only TCP client for the Submarine Fleet Management System
//
// Two interfaces onto the same read-only connection to the Central
// Computer's TCP server (spec §4 - "the ground station can request from
// the submarine's central computer data stored over a period of time...
// both the data stored in a log and data of the events"):
//   - a console CLI menu (GroundStationCli) - the original interface;
//   - a small web dashboard (http://localhost:8081 by default) - a
//     second, browser-based way to make the exact same read-only
//     requests, added per the project owner's explicit request that a
//     web UI live on both the Central Computer and the Ground Station,
//     not just the Central Computer.
//
// Both share one TcpClient/one TCP connection (TcpClient::sendRequest()
// is mutex-protected for exactly this reason - see tcp_client.h). All
// operations are read-only - no mutations to the Fleet occur in this
// binary, from either interface.

#include <fstream>
#include <iostream>
#include <sstream>

#include "gs_dashboard_api.h"
#include "ground_station_cli.h"
#include "http_server.h"

using namespace ground_station;

namespace {

// The dashboard is a plain static HTML/CSS/JS file at web/dashboard.html
// (relative to wherever ground_station is launched from), not embedded in
// the binary - same convention, and same reasoning, as central-computer's
// own dashboard (see that project's main.cpp). Read fresh on every GET /
// rather than cached once at startup.
std::string loadDashboardHtml() {
  std::ifstream file("web/dashboard.html", std::ios::binary);
  if (!file) {
    return "<html><body><h1>web/dashboard.html not found</h1>"
           "<p>Run ground_station from the ground_station/ source directory, "
           "or from the build output directory (CMake copies web/ there "
           "after every build).</p></body></html>";
  }
  std::ostringstream contents;
  contents << file.rdbuf();
  return contents.str();
}

void registerDashboardRoutes(HttpServer& server, GsDashboardApi& api) {
  server.get("/", [](const HttpRequest&) { return HttpResponse::html(loadDashboardHtml()); });
  server.get("/api/submarines", [&api](const HttpRequest&) { return HttpResponse::json(api.listSubmarines()); });
  server.get("/api/logs", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.getLogs(req.param("serial"), req.param("start"), req.param("end")));
  });
  server.get("/api/events", [&api](const HttpRequest& req) {
    return HttpResponse::json(api.getEvents(req.param("serial"), req.param("start"), req.param("end")));
  });
  server.get("/api/summary", [&api](const HttpRequest&) { return HttpResponse::json(api.summaryReport()); });
}

}  // namespace

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

  // Connect now (idempotent - GroundStationCli::run() also calls this, and
  // TcpClient::connect() returns true immediately if already connected) so
  // the dashboard has a live connection to relay through as soon as it
  // starts, not just once the operator reaches the CLI's own prompt.
  cli.client().connect();

  GsDashboardApi dashboardApi(cli.client());
  const int kDashboardPort = 8081;  // Central Computer's own dashboard uses 8080
  HttpServer server(kDashboardPort);
  registerDashboardRoutes(server, dashboardApi);
  if (server.start()) {
    std::cout << "Web dashboard: http://localhost:" << kDashboardPort << " (read-only)\n";
  } else {
    std::cout << "Web dashboard could not start on port " << kDashboardPort
              << " (already in use?) - continuing with console only.\n";
  }

  bool ok = cli.run();

  server.stop();
  return ok ? 0 : 1;
}
