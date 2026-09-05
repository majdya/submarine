# Web Dashboard

A hand-rolled HTTP server plus a thin JSON API layer, sharing the same
live `Fleet` as the console via one mutex. Additive - the console menu
is unchanged and remains the graded deliverable.

```mermaid
graph TD
  A[Browser loads localhost:8080] --> B[Serves embedded dashboard_html page]
  B --> C[Page polls GET /api/state every 2s]
  C --> D[DashboardApi::stateJson - lock mutex, read Fleet, build JSON]
  E[Browser submits a form: add/assign/end/message] --> F[POST to /api/... route]
  F --> G[HttpServer matches route incl. :serial path param]
  G --> H[DashboardApi method - lock mutex, mutate Fleet, return ok/error JSON]
  H --> C
  I[Console handler running] -.->|shares g_fleetMutex| H
```

Source: `http_server.h/.cpp`, `dashboard_api.h/.cpp`,
`dashboard_html.h/.cpp`, wired up in `main.cpp`.
