# Project Status / Session Handoff

Running checklist of what's done and what's next, so a new session can pick
up without re-reading everything. Update this file at the end of each
session rather than starting a new one.

_Last updated: 2026-09-09_

- [x] **Central Computer dashboard UI overhaul, 2026-09-09** (project
      owner's explicit request: "UI can be improved" -> layout/visual
      polish, then a follow-up: per-submarine actions must live inside
      that submarine's own card, not a separate form where you type the
      serial by hand):
      - Full visual redesign: sticky top bar with a live-refresh
        indicator, a 4-tile fleet summary strip (total/combat/research/
        connected), refined typography scale, card shadows/depth, a
        key-value grid for mission details, and a dedicated "live
        telemetry" strip per submarine instead of plain inline text.
      - Submarine type (Add form) and sensor Enabled state (Set limits)
        are small segmented toggles instead of dropdowns (this was
        already done 2026-09-08 for the *old* layout - carried over and
        restyled to match the new design).
      - **Structural change**: removed the old always-visible sidebar
        forms for mission/combat/limits, which required typing a target
        serial number by hand into a shared global form. Each submarine
        card now has its own collapsible "Actions" section with tabs
        (Mission / Associate &amp; message [Combat only] / Sensor
        limits), pre-scoped to that exact submarine - no serial entry
        needed. Only "Add submarine" (which has no target submarine)
        remains a separate control, now a modal dialog opened from a
        toolbar button instead of a sidebar card.
      - A per-card actions panel's open/closed state and active tab
        survive the dashboard's 2-second auto-refresh (previously the
        whole panel would have been wiped and rebuilt every cycle).
      - Fixed a real bug caught while testing this: a per-card action's
        "Saved"/"Error: ..." status message was being written to the DOM
        and then immediately wiped out by an unconditional `refresh()`
        call right after - the user would never actually see it. Fixed
        with a short (900ms) debounced `refreshSoon()` used after a
        successful per-card action, so the confirmation is visible for a
        beat before the fleet list re-renders; the regular 2s auto-poll
        is unaffected.
      - Verified with a real headless-browser (Playwright) test driving
        the actual built `central_computer` binary end-to-end: opened
        the dashboard, added a Combat and a Research submarine via the
        new modal, opened a submarine's Actions panel, assigned a
        mission through the per-card Mission tab (no serial typed
        manually), confirmed the "Saved" status was visible, confirmed
        the assigned mission appeared on the card after the next
        refresh, confirmed the actions panel stayed open across that
        refresh, and exercised the Associate/message and Sensor-limits
        tabs (including the Enabled toggle) - all against live HTTP
        responses from the real dashboard API, not mocked.
      - Purely a `central-computer/web/dashboard.html` change (static
        file, read from disk on every request) - no C++, build, or
        protocol changes; no rebuild needed, only a restart of
        `central_computer.exe` (or a browser refresh if it was already
        running from the source directory for live editing).

- [x] **Fixed: typing in a per-card action field got wiped by the 2s
      auto-refresh, 2026-09-09** (project owner's explicit bug report:
      "it seems when i try to type and fill the card for a submarine,
      for action for example card refreshes on incoming message"):
      - Root cause: the dashboard's `refresh()` fully rebuilds every
        submarine card's HTML from the API response every 2 seconds
        (`el.innerHTML = ...`). The previous "preserve state across
        refresh" logic only remembered a card's open/closed
        actions-panel state and active tab - it never captured or
        restored the actual values typed into that card's form fields,
        since the fields themselves were torn down and recreated fresh
        on every tick regardless of whether the user was mid-edit.
      - Fix: `refresh()` now identifies whichever `.sub-card` currently
        contains the focused element (`document.activeElement`) and
        treats it as "pinned" - every other card's HTML is rebuilt as
        before, but the pinned card's actual live DOM node is moved
        into the rebuilt fleet list untouched, so nothing inside it
        (typed text, cursor position, selection) is ever destroyed.
      - This needed care: simply moving a DOM node (even back into the
        same document) fires a `blur` event on any focused descendant
        first, and focus isn't restored automatically afterward. The
        fix explicitly remembers which field had focus and its
        selection range before the move, then calls `.focus()` and
        `.setSelectionRange()` on it again right after - so the caret
        position is preserved too, not just the text.
      - The "pin by current focus" approach (rather than "pin by
        actions-open") was a deliberate choice: it means a card the
        user has simply left open (not currently typing in) still
        refreshes normally - so a just-assigned mission's details still
        appear on the card right after a successful save, exactly as
        before. Only the specific card - and specific instant - where
        the user is actually mid-keystroke is ever exempted from the
        refresh cycle.
      - Verified with Playwright against the real running binary:
        typed into a per-card Mission-description field and held focus
        there through three full 2-second auto-refresh ticks (6+
        seconds), confirming the typed text and the actions-open state
        both survived untouched on every tick; then confirmed the
        existing "Saved" status flow and "mission appears on the card
        after refresh" behavior still both work correctly; then
        confirmed an unrelated new submarine's card still renders and
        updates normally while another card is pinned.
      - Purely a `central-computer/web/dashboard.html` change - no
        rebuild needed, only a restart of `central_computer.exe` (or a
        browser refresh if already running from the source directory).


- [x] **Console UX change 2026-09-08, by operator request:** every fixed-set
      console choice is now a numbered menu instead of a typed word -
      "Type ('research' or 'combat')" -> "1. Research / 2. Combat",
      "Parameter (temp/humidity/light/battery)" -> a 1-4 menu, and every
      yes/no prompt (e.g. "Change limits too?") is specifically 1=Yes/2=No.
      New `promptChoice()`/`promptYesNo()` helpers in `main.cpp` back all
      of them, looping until a valid number is entered. `main()`'s own
      1-11 menu and Ground Station's 1-5 menu were already numbered - no
      change needed there. Verified: rebuild, all 5 test suites pass, and
      a scripted stdin run through Add-submarine and Set-limits confirms
      every prompt now shows its numbered options.

- [x] **Ground Station web dashboard, added 2026-09-08** (project owner's
      explicit choice, via "do we comply with... web ui must be in the
      Computer and Ground?" -> "Yes, add a Ground Station web dashboard"):
      Ground Station now has a second, independent read-only web UI,
      alongside its existing console CLI, exactly mirroring how Central
      Computer already has both a console menu and a web dashboard.
      - New `HttpServer` on port 8081 (Central Computer's is 8080), started
        in `ground_station/src/main.cpp` next to the existing
        `GroundStationCli`, both sharing one `TcpClient` connection to the
        Central Computer's read-only TCP server (port 9000). `TcpClient`
        gained an internal mutex so the console thread and the HTTP
        server's background thread can safely share one socket.
      - `ground_station/include+src/http_server.*` and `json_util.h` are
        the same hand-rolled, dependency-free HTTP server/JSON writer
        already used by Central Computer, copied over verbatim with the
        namespace renamed `submarine` -> `ground_station` (no third-party
        libraries pulled in, consistent with the rest of the project).
      - New `GsDashboardApi` (`gs_dashboard_api.h/.cpp`) is a thin relay:
        it calls the exact same read-only TCP protocol
        (`LIST_SUBMARINES` / `GET_LOGS,...` / `GET_EVENTS,...` /
        `SUMMARY_REPORT`) the console CLI already used, and forwards the
        Central Computer's JSON response back to the browser unchanged -
        Ground Station gains no new capability, just a second way to see
        the same read-only data (spec §4).
      - Routes: `GET /` (dashboard page), `GET /api/submarines`,
        `GET /api/logs?serial=&start=&end=`, `GET /api/events?serial=&start=&end=`,
        `GET /api/summary`.
      - `ground_station/web/dashboard.html` (new): dark-themed page styled
        to match `central-computer/web/dashboard.html`, clearly labeled
        READ-ONLY, with a live fleet list, a summary panel, and log/event
        query forms - no add/mutate forms, since Ground Station cannot
        change fleet state per spec.
      - `ground_station/CMakeLists.txt` updated to compile the new sources
        and copy `web/` next to the built executable after every build
        (identical `POST_BUILD` pattern to Central Computer's own CMake).
      - Verified: full project rebuild is clean (zero warnings/errors),
        all 5 existing test suites (`ctest`) still pass unchanged, and an
        end-to-end smoke test was run: a real `central_computer` was
        started, two submarines (one Combat, one Research) were added via
        its dashboard API, a real `ground_station` was started against it,
        and all 4 new routes plus `GET /` were hit with `curl` and
        returned correct JSON/HTML - confirmed against the exact response
        shapes documented in `central-computer/src/tcp_api.cpp`.
      - There are no dedicated Ground Station unit tests yet (the project
        never had any before this change either) - verification was via
        the live smoke test above rather than a `ctest` entry; a
        `test_dashboard`-style HTTP test for Ground Station (mirroring
        Central Computer's own `tests/test_dashboard.cpp`) would be a
        reasonable follow-up if more time is available.

- [x] **Build fixed 2026-09-08 (MSVC):** `ground_station`'s CMake target
      had no explicit C++ standard set, so on the project owner's real
      Windows/MSVC build it fell back to MSVC's pre-C++17 default and
      failed with `error C2429: language feature 'nested-namespace-
      definition' requires compiler flag '/std:c++17'` compiling
      `gs_dashboard_api.cpp` (which includes `json_util.h`'s
      `namespace ground_station::json { ... }`). Not caught by this
      session's own build verification because the cloud sandbox's g++
      happened to default to a late-enough standard already. Fixed by
      adding explicit `set_target_properties(... CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON)` to both `ground_station_lib` and
      `ground_station` in `ground_station/CMakeLists.txt`, matching what
      `central-computer/CMakeLists.txt` already sets globally near its
      top. Verified: full rebuild clean, all 5 test suites still pass.

- [x] **Bug fixed 2026-09-08:** `LogModule::writeLine()` echoed every log
      line to stdout (`[CentralComputer LOG] ...`) unconditionally - since
      this runs on `CommLink`'s background reader thread and the LNC sends
      a KEEPALIVE every `monitor_period_ms` (5s by default), this printed
      forever and raced with the console menu's own `std::cin`/`std::cout`
      prompts on the main thread, burying them. Found via the project
      owner's own E2E run. Removed the stdout echo - `LogModule` now only
      writes to the log file, which is what a log module should do; a
      live view belongs in the web dashboard (already polls, doesn't
      print) or a future "tail logs" console option, not an
      uncontrollable stream mixed into the interactive prompt. Verified:
      rebuilt, all 5 test suites still pass, and `test_central_computer`
      (which exercises this exact code path) no longer prints the
      `[CentralComputer LOG]` line on stdout.

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
- [x] command_set_limits (firmware decode/apply/persist path, via `tools/lnc_test_tool.py` - **not** via the Central Computer app itself, see §2 below)
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
      `Mission`, `Message`, `Fleet`, `Menu` — all 10 spec menu operations.
      `Submarine` (base class) owns the `CentralComputer` as of 2026-09-08,
      so Research submarines can have LNC hardware too, not just Combat ones
      (deliberate spec extension - see "Pending / open" below)
- [x] Reuses the firmware's own `tlv.c` / `comm_frame.c` unmodified (zero
      protocol drift between firmware and PC app)
- [x] Test suites: `test_protocol_bridge`, `test_comm_link`,
      `test_central_computer`, `test_fleet` — all passing, `-Wall -Wextra
      -Wconversion` zero warnings
- [x] Manual smoke test of the full console menu (all 10 operations) — clean
- [x] Verified 2026-09-07 (host-compiled test driver, real `tlv.c`/`comm_frame.c`/`app_command.c`/`app_config.c` from the firmware, unmodified): SET_LIMITS decode -> validate -> `AppConfig_Set` -> flash persist -> ACK works correctly end-to-end at the protocol/firmware level, for both the temp full-range param and the light/humidity/battery single-lower-bound params, including negative bounds and rejection of malformed commands. `ManagementCommand::setLimits()` (PC side) also verified: its TLV encoding matches the firmware decoder exactly (`test_protocol_bridge`, real wire round-trip, passing).
- [x] **Gap closed 2026-09-08:** `ManagementCommand::setLimits()` is now reachable from both UIs - console menu item 11 (extra, beyond the spec's 10 numbered ops - those stay unrenumbered) and a dashboard "Set sensor limits" form/`POST /api/submarines/:serial/limits` route. Both go through the same `Menu`/`Fleet`/`g_fleetMutex` as everything else. "Get reading on demand" was considered and deliberately *not* added - confirmed against `final project.pdf` that it isn't a spec requirement.

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
- [x] Done 2026-09-08: `ManagementCommand::setLimits()` wired into the console (menu item 11) and the dashboard - see §2.

### Scope/requirements to settle
- [x] **Ground Station**: built as its own separate PC app
      (`ground_station/`), a TCP client to the Central Computer's read-only
      server (port 9000), exactly as the spec's 3-app plan calls for. It
      now has both a console CLI and (2026-09-08) its own read-only web
      dashboard on port 8081 - see the entry above.
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
- [x] Done 2026-09-08: dashboard is now a real static file at
      `central-computer/web/dashboard.html`, read from disk on every request
      (not cached) - edit it and refresh the browser, no rebuild needed.
      `dashboard_html.h/.cpp` (the embedded-string version) removed.
- [x] Done 2026-09-08: **`ResearchSubmarine` now has a `CentralComputer` too**
      - a deliberate, explicit extension beyond the spec's literal "belongs
      to each combat submarine" wording, done at the project owner's request.
      `Submarine` (the base class) now owns the `CentralComputer`, defaulting
      to a loopback placeholder when no real transport is supplied; both the
      console `Add` flow and the dashboard's add-submarine form can now
      optionally wire a serial port to a research submarine, same as combat.
      Participation/messaging (ops 7-9) remain Combat-only - genuinely
      combat-specific, unrelated to hardware. Verified: `test_fleet` (new
      `test_research_submarine_gets_default_central_computer`) and
      `test_dashboard` (new research-submarine HTTP assertions), all 5 test
      suites still passing.
- [x] Done 2026-09-08: **per-submarine enable/disable toggles for the 4
      environmental sensors** (temp/humidity/light/battery), at the real
      firmware level - explicitly *not* object detection or other features
      (project owner's own scoped choice). `AppConfig_t` gained 4 new
      `*_enabled` fields (default enabled=1, so existing behavior is
      unchanged until something explicitly disables a sensor); a new
      optional `TAG_ENABLED` TLV tag rides on the existing
      `MSG_TYPE_CMD_SET_LIMITS` command rather than a new message type -
      it can arrive alone (toggle only), alongside real limits in the same
      command, or be omitted entirely (old-style limits-only commands
      behave exactly as before); `task_monitor.c`'s mode classification
      loop skips a disabled parameter's contribution to `WorstMode()` -
      still sampled and reported, just excluded from alarm status. PC side:
      `ManagementCommand::setLimits()` gained an optional `enabled`
      parameter; console menu item 11 and the dashboard's "Set sensor
      limits / enable-disable" form both expose it.
      Verified: firmware logic via a from-scratch host-side test driver
      (`app_command.c`/`app_config.c`, real and unmodified, compiled
      against stub HAL/RTOS headers - same approach as the 2026-09-07
      SET_LIMITS verification) - 30/30 checks pass, including enabled-only
      commands, combined limits+enabled commands, persistence across a
      simulated reboot, and rejection of a command with neither bounds nor
      TAG_ENABLED. `task_monitor.c`'s classification-skip change compiles
      clean (`-Wall -Wextra`, zero warnings) against the same kind of stub
      headers - no ARM toolchain available for a full build. PC side:
      `test_central_computer` extended with a real MessageBuilder/
      MessageDecoder/TLV round trip of TAG_ENABLED (enabled-only and
      combined-with-limits), all 5 test suites still passing.
- [ ] No fleet persistence yet (submarines/missions/messages don't survive
      an app restart) — only the Central Computer's own logs/data do
- [x] Ground Station (TCP client side) from the original 3-app plan — built
      as its own separate app, with both a console CLI and (2026-09-08) a
      read-only web dashboard on port 8081
