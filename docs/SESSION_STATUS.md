# Project Status / Session Handoff

Running checklist of what's done and what's next, so a new session can pick
up without re-reading everything. Update this file at the end of each
session rather than starting a new one.

_Last updated: 2026-09-05_

## Git state

- Repo: single repo at the top of `submarine/` (not nested per-project repos).
- Branches:
  - `main` — merged, stable baseline.
  - `spec-alignment-modes-comm-flash-log` — LNC firmware spec-alignment work,
    commit `e43b014` (test tool + tracking doc). Not merged to main yet.
  - `central-computer-oop-app` (branched off the above) — the new C++ fleet
    app, commit `1c6ab66` (app + web dashboard). **Currently checked out.**
    Not merged to main yet.
- Neither feature branch has been pushed to `origin` or merged. Decide when
  ready: merge order, PR vs direct merge, whether to squash.

## 1. LNC Firmware (STM32 Nucleo-L476RG, spec SW-FD-LNC-001)

- [x] Operating modes, mode/object RGB + alarm wiring
- [x] Keep-alive, flash config, log retention, real TLV comms
- [x] Host-compiled date-arithmetic verification (4018 dates checked, zero bugs)
- [x] `tools/lnc_test_tool.py` — PC-side Python test client (mirrors `comm_frame.c` exactly)
- [x] `TEST_TRACKING.md` — checklist of hardware test results

### Confirmed on real hardware (12 items)
- [x] object_detect_toggle
- [x] silence_button
- [x] log_file_creation
- [x] rtc_battery_backed
- [x] mode_reporting
- [x] keepalive_cadence
- [x] comm_frame_realworld
- [x] command_set_time
- [x] command_get_time
- [x] command_set_limits
- [x] command_get_data
- [x] command_get_events

### Still needs testing on hardware
- [ ] mode_transitions
- [ ] rgb_mode_color
- [ ] rgb_alarm_interaction
- [ ] alarm_buzzer
- [ ] flash_config_persistence
- [ ] log_retention_7day

### Blocked / watch items
- [ ] iwdg_watchdog — **blocked**, waiting on explicit go-ahead before re-enabling
- [ ] possible_silence_double_fire — **watch**, unconfirmed; needs one deliberate
      isolated button press to tell debounce issue vs. double-press by the tester
- [ ] sensor_reading_anomaly — **watch**
- [ ] debug_binary_uart_shared — known issue, documented, not fixed

_Full detail: `submarine-final-project/TEST_TRACKING.md`_

## 2. Central Computer + Fleet Management App (C++17)

- [x] Single cross-platform CMake app (Linux + Windows), kept modular per-module
- [x] Real UART to the LNC via `SerialTransport` (not mocked), `ITransport`
      abstraction + `LoopbackTransport` for host-side testing
- [x] Standard C++ STL throughout (not the course's container library)
- [x] Full OOP hierarchy: `Submarine` / `ResearchSubmarine` / `CombatSubmarine`,
      `Mission`, `Message`, `Fleet`, `Menu` — all 10 spec menu operations
- [x] Reuses the firmware's own `tlv.c` / `comm_frame.c` unmodified (zero
      protocol drift between firmware and PC app)
- [x] Test suites: `test_protocol_bridge`, `test_comm_link`,
      `test_central_computer`, `test_fleet` — all passing, `-Wall -Wextra
      -Wconversion` zero warnings
- [x] Manual smoke test of the full console menu (all 10 operations) — clean

## 3. Web Dashboard (added this session)

- [x] Embedded HTTP server, raw sockets, POSIX/Win32 behind one interface
      (`http_server.h/.cpp`) — no external library, no keep-alive, form-encoded
      bodies only (no JSON parser needed)
- [x] Hand-written JSON writer (`json_util.h`)
- [x] `DashboardApi` — thread-safe wrapper around `Menu`/`Fleet`
      (`dashboard_api.h/.cpp`)
- [x] Single-page dashboard UI embedded as a C++ string (`dashboard_html.h/.cpp`)
- [x] Fully interactive: add submarine, assign/update/end mission, associate
      combat submarines, send messages — all from the browser
- [x] Live per-submarine LNC snapshot (`LiveSnapshot` on `CentralComputer`,
      populated from `onUnsolicited()`) shown on the dashboard, polled every 2s
- [x] Console menu behavior is **unchanged** — still the graded deliverable;
      dashboard is additive, runs in the same process against the same `Fleet`
- [x] Concurrency: one coarse-grained `std::mutex` (`g_fleetMutex`) shared
      between console handlers and `DashboardApi` — documented tradeoff
      (simplicity over throughput; fine for a local single-operator tool)
- [x] New test: `test_dashboard` — real `HttpServer`/`DashboardApi` driven by
      raw HTTP requests (add/duplicate-reject, mission assign/reject-double/end,
      404 handling)
- [x] All 5 test suites pass, zero warnings
- [x] End-to-end smoke test: console + `curl` hitting the API concurrently,
      confirmed shared live state both directions
- [x] Verified against the real on-device source files (manual g++ compile/link,
      separate from the cmake build)
- [x] `README.md` updated (dashboard section + concurrency tradeoff)
- [x] Committed: `1c6ab66` on `central-computer-oop-app`

### Known gotcha (hit this session)
- CMake on this machine generates **Visual Studio project files**, so the
  built exe lands at `build\Debug\central_computer.exe` (or `build\Release\...`),
  **not** `build\central_computer.exe`. A stale top-level exe from an earlier
  build can be run by mistake and looks like the dashboard "isn't there" —
  always run the one under `build\Debug\` (or whichever config was built),
  and delete stale copies after a big update.

## How to build & run (Windows, current state)

```powershell
cd C:\Users\M0Y\Dev\Embedded\submarine\central-computer
cmake -S . -B build          # only needed after CMakeLists.txt changes / fresh clone
cmake --build build -j
ctest --test-dir build --output-on-failure
.\build\Debug\central_computer.exe      # check actual path with Get-ChildItem if unsure
```

Then open `http://localhost:8080` in a browser for the dashboard, alongside
the console menu in the same terminal.

## What's still needed for a genuinely "done" project

Beyond the immediate pending list above — the bigger-picture gaps between
where things stand and a fully submission-ready project (see
`docs/PROJECT_PLAN.md` Phase 8/9 for the original scope):

### Functional completeness
- [ ] Finish the 6 remaining hardware tests (mode_transitions, rgb_mode_color,
      rgb_alarm_interaction, alarm_buzzer, flash_config_persistence,
      log_retention_7day)
- [ ] Resolve the IWDG watchdog item — currently blocked; a spec-required
      module (§ watchdog) that's disabled/reverted is a real gap for a final
      submission, not just a nice-to-have
- [ ] Confirm possible_silence_double_fire one way or the other (debounce bug
      vs. tester double-press)
- [ ] Run a real 7/8-day log-rotation soak test (or a simulated-clock
      equivalent) — this was never actually exercised, only unit-level date
      arithmetic was verified
- [ ] **End-to-end integration test with real hardware through the actual C++
      app** — everything's been tested either via the Python tool or via
      `LoopbackTransport`/mocked tests; the fleet app has not yet been run
      against the live LNC over a real COM port doing a full scenario (mode
      change → keep-alive → retrieval commands → dashboard reflecting it)

### Scope/requirements to settle
- [ ] **Ground Station**: the original spec/plan called for a separate
      Ground Station PC app (TCP client to the Central Computer). The build
      instead went with one combined app — confirm this is acceptable for
      grading, or add a minimal Ground Station/TCP layer if it's a hard
      requirement
- [ ] **Container library choice**: `PROJECT_PLAN.md` flagged needing to
      confirm whether the OOP Part is required to use the course's generic
      containers (`genvector.h`/`HashMap.h`/etc.) instead of `std::vector`/
      `std::map`. This was decided in favor of STL by explicit choice this
      session, but it was never actually confirmed against a grading rubric —
      worth double-checking before submission
- [ ] No grading rubric was ever located — `PROJECT_PLAN.md` flags this as an
      open item from the start. Worth getting one from the instructor/course
      page so remaining effort targets what's actually graded

### Submission packaging (currently missing entirely)
- [ ] Protocol spec write-up (the TLV/frame format, as a standalone doc —
      right now it only exists as code + comments)
- [ ] Architecture / sequence diagram(s) per module interaction
- [ ] Build & flash instructions for the firmware side (the PC app's build
      instructions exist in `central-computer/README.md`; firmware-side
      build/flash steps for a fresh grader don't yet exist as a doc)
- [ ] A short demo script/rehearsal: walking through Normal→Warning→Error,
      a management-command round trip, and now also the web dashboard, in an
      order a grader can follow live

### Nice-to-haves (not required, but worth flagging)
- [ ] Fleet persistence across app restarts (submarines/missions/messages
      currently don't survive a restart — only the Central Computer's own
      logs/data do)
- [ ] Push feature branches to `origin` and merge to `main` before final
      submission, with a clean, reviewable history

## Pending / open for next session

- [ ] Decide when/whether to merge `spec-alignment-modes-comm-flash-log` and
      `central-computer-oop-app` into `main` (and in what order)
- [ ] Push feature branches to `origin` (currently local-only)
- [ ] Remaining LNC hardware tests (6 items, see above)
- [ ] IWDG watchdog re-enable — needs explicit go-ahead
- [ ] Confirm/rule out possible_silence_double_fire with one isolated press
- [ ] Optional: extract `dashboard_html.cpp`'s embedded HTML into a real
      `dashboard.html` file loaded from disk, if easier editing is wanted
      over the single-binary convenience
- [ ] No fleet persistence yet (submarines/missions/messages don't survive
      an app restart) — only the Central Computer's own logs/data do
- [ ] Ground Station (TCP client side) from the original 3-app plan — not
      built; current design is one combined app instead (by explicit choice)
