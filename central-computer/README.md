# Central Computer / Submarine Fleet Management (OOP)

Per the final project spec (SW-FD-LNC-001), this is a single, cross-platform C++17
console program covering two tied-together parts:

1. **Central Computer** (spec section 3) — talks to the LNC end unit (the STM32
   firmware in `../submarine-final-project/`) over a real serial/UART link.
   Modules: Communication (`comm_link.h`), Management Command (`management_command.h`),
   Log (`log_module.h`), Data Collection & Analysis (`data_collection.h` + `data_store.h`).
2. **OOP Part — Submarine Fleet Management System** — the spec treats the Central
   Computer as an object owned by each combat submarine, so this extends part 1
   with the fleet/mission/messaging class hierarchy (`Submarine` base class,
   `ResearchSubmarine`/`CombatSubmarine` subclasses, `Mission`, `Message`) plus the
   10-operation console menu (`menu.h`/`main.cpp`).

Built as one program (per project decision) but kept **modular** on purpose — every
module above is its own header/source pair with no hidden dependencies on the
console UI, so any one of them could be pulled into a separate process/binary later
with no change to the others.

## Protocol reuse

`comm_link.h`/`protocol_bridge.h` compile the LNC firmware's own protocol source
(`tlv.c`, `comm_frame.c`, `comm_tags.h`) **unmodified**, straight from
`../submarine-final-project/Core/{Inc,Src}/app/` — see `CMakeLists.txt`'s
`FIRMWARE_DIR` cache variable. There is exactly one implementation of the wire
protocol in the whole system; the PC app and the firmware can never drift apart on
frame/TLV encoding. This is the same protocol already verified live against real
hardware (see `../submarine-final-project/TEST_TRACKING.md` and
`../submarine-final-project/tools/lnc_test_tool.py`).

## Building

Cross-platform (Linux, macOS, Windows via MinGW-w64 or MSVC) — no external
dependencies beyond a C++17 compiler, CMake, and (on Linux) pthreads.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure   # runs the test suite below
./build/central_computer                     # the interactive menu + web dashboard
```

On Windows, the serial port name is e.g. `COM8`; on Linux, `/dev/ttyACM0`.

## Web dashboard

Starting `central_computer` also starts a small embedded HTTP server (default
`http://localhost:8080`) alongside the console menu, printed to the console on
startup. It is a **bonus**, fully-interactive way to use the app from a browser —
the numbered console menu above is unchanged and remains the graded deliverable;
both sides operate on the exact same live `Fleet` at the same time.

From the browser you can do everything the console menu can: add a submarine,
assign/update/end a mission, associate combat submarines with the same mission,
and send messages between them — plus see each connected combat submarine's most
recent live LNC data (mode, sensor readings, last event) without leaving the page.
It also exposes the Management Command module's Set Limits (spec §2.5/§3.2) —
previously only reachable through the console's own extra menu item — from a
form, against a live-connected combat submarine's LNC.
The page polls `GET /api/state` every 2 seconds and posts form-encoded requests to
`/api/submarines`, `/api/submarines/:serial/mission[/update|/end]`,
`/api/submarines/:serial/participate`, `/api/submarines/:serial/limits`, and
`/api/messages` (see `dashboard_api.h` for the exact contract, and `http_server.h`
for the tiny hand-rolled server itself — raw sockets, no library, since this
build has no network access to fetch one).

**It's a plain static webapp, not embedded in the binary.** The page itself
lives at `central-computer/web/dashboard.html` — a real HTML/CSS/JS file, not
a C++ string — and is read from disk on every `GET /` (not cached). Run
`central_computer` from the `central-computer/` source directory (same
convention as the existing `logs/<serial>` and `data/<serial>` relative
paths) and it reads that file directly: edit it, refresh the browser, no
rebuild needed. Run the *built* exe from its own output directory instead
(e.g. `build/central-computer/Debug/`, per `build-and-start.md`) and it
reads a copy of `web/` that CMake places next to the exe automatically
after every build (see the `add_custom_command(... POST_BUILD ...)` in
`CMakeLists.txt`) — that copy is what keeps the documented "run the built
exe" workflow working; edit the source under `central-computer/web/` and
rebuild to refresh it, not the copy itself. Either way, an exe with no
`web/dashboard.html` reachable from its working directory falls back to a
small "not found" page rather than failing to start.

**Concurrency.** The console and the dashboard share one `Fleet` guarded by a
single coarse-grained `std::mutex` (`g_fleetMutex` in `main.cpp`, passed into
`DashboardApi`) — not per-submarine locking. Each console handler holds the lock
for its *entire* body, including its blocking prompts, which is a deliberate
simplicity-over-throughput tradeoff: the dashboard's next poll can briefly wait
if a console operation is mid-prompt, and always catches up on the following
poll. For a local, single-operator tool this is simpler to reason about
correctly than fine-grained locking, at no real cost.

If port 8080 is already in use, the app prints a message and continues with the
console only — the dashboard is additive, never required.

## Design decisions worth knowing about

- **Real hardware, not mocked.** `SerialTransport` opens a real COM port /
  tty and speaks the actual frame protocol. Because there is normally only one
  physical LNC on a bench, only the one submarine you point at real hardware
  gets a connected `CentralComputer`; every other one defaults to a
  disconnected `LoopbackTransport` placeholder (harmless - it just never
  produces or accepts data) rather than a null/optional transport, which keeps
  `CentralComputer` simple to use everywhere.
- **Every submarine has a `CentralComputer` - Research included.** The spec's
  OOP Part literally says the central computer "belongs to each combat
  submarine". This project deliberately goes further, at the project owner's
  explicit request: `Submarine` (the common base class) owns the
  `CentralComputer`, auto-creating a default loopback one when the caller
  doesn't supply a real transport, so a `ResearchSubmarine` can be wired to
  real LNC hardware exactly like a `CombatSubmarine` can. `CombatSubmarine`
  keeps its own `centralComputer()` accessors (now just forwarding to the base
  class) purely so its existing callers didn't need to change. Participating
  submarines and inter-submarine messaging (operations 7-9) remain
  Combat-only, since those are genuinely combat-specific OOP features
  unrelated to hardware.
- **`ITransport` abstraction.** Per the spec's own note ("the physical transport
  is a configuration detail of the communication module, not a structural
  assumption elsewhere"), `CommLink` only ever talks to `ITransport`. This is
  also what makes the whole stack host-testable without hardware: tests use
  `LoopbackTransport` plus a small fake-LNC thread that answers commands the
  way `app_command.c` does on the real firmware.
- **STL containers**, not the course's generic container library — a
  from-scratch decision per this project's own choice, using `std::vector`,
  `std::optional`, `std::map`, etc. throughout.
- **`Mission` is one concrete class**, not a subclass hierarchy mirroring
  `Submarine`'s, holding every field either submarine type uses (description,
  commander/personnel for combat; researcher names/topic for research). The
  menu only asks for and displays the subset relevant to a submarine's actual
  type. See `mission.h`'s class comment for the reasoning.
- **References are serial numbers, not pointers.** `CombatSubmarine` stores
  participating submarines and message senders as serial-number strings,
  resolved through `Fleet::findBySerial` — `Fleet` owns every `Submarine` via
  `unique_ptr`, so a raw pointer could dangle if a submarine were ever removed.
- **"Database" is a flat-file stand-in** (`data_store.h`): no SQL/NoSQL engine
  is bundled (no network access in the build environment to fetch one, and no
  reason to require installing one). Kept behind `DataStore`'s interface so
  swapping in a real database later touches one file, not every caller.
- **Command consolidation**: `setLimits()` takes a `PARAM_*` selector rather
  than exposing 8 near-identical methods, matching the same consolidation
  decision already made on the firmware side.
- **Per-sensor enable/disable is piggybacked on `setLimits()`, not a new
  command** - `setLimits()`'s optional `enabled` parameter encodes
  `TAG_ENABLED` on the same `MSG_TYPE_CMD_SET_LIMITS` message (see
  `comm_tags.h`), rather than inventing a second message type. This is a
  deliberate extension beyond the spec, added at the project owner's
  explicit request, scoped to exactly the 4 environmental sensors (temp,
  humidity, light, battery) - a disabled sensor is still sampled and
  reported, just excluded from the LNC's overall Operating Mode vote (see
  `task_monitor.c`'s `WorstMode()` loop on the firmware side).
- **GET_DATA and GET_EVENTS return identical content** today (both query the
  LNC's date-named log files - there is no separate continuous data stream on
  the firmware side yet). Documented as a known limitation, not silently
  papered over.

## Testing

Every module above the console UI is covered by a dependency-free test
(`tests/test_*.cpp`, using a tiny custom `CHECK`/`TEST_MAIN_EXIT` harness rather
than downloading gtest - see `tests/test_harness.h`), run via `ctest`:

- `test_protocol_bridge` — MessageBuilder/MessageDecoder round-trip against the
  real firmware protocol code, including negative values and multiple
  back-to-back frames in one read.
- `test_comm_link` — CommLink against `LoopbackTransport` plus a fake-LNC
  thread: command/ACK, a multi-frame GET_EVENTS-style stream terminated by an
  empty DATA_REPORT, and an unsolicited KEEPALIVE reaching the right callback
  path (not the reply-waiter path).
- `test_central_computer` — full end-to-end: a fake LNC feeds real KEEPALIVE/
  EVENT frames over a loopback transport and answers a SET_TIME command;
  verifies the Log module actually wrote the lines and DataCollection actually
  persisted and can report on them.
- `test_fleet` — the OOP hierarchy: duplicate-serial rejection, mission
  assign/update/end exclusivity, combat-submarine participation and messaging
  (including sender resolution back through `Fleet`), and `dynamic_cast`-based
  type checks.
- `test_dashboard` — the web dashboard's HTTP layer end-to-end: starts a real
  `HttpServer` wired to a real `DashboardApi`/`Menu`/`Fleet` (no mocking) and
  drives it with raw HTTP requests exactly like a browser's `fetch()` would —
  add/duplicate-reject a submarine, assign/reject-double-assign/end a mission
  via the `:serial` path-param route, and an unknown route returning 404
  without disturbing the server.

All 5 suites pass with `-Wall -Wextra -Wconversion` and zero warnings.

## Menu operations (spec-numbered)

1. Add a new submarine to the fleet (choose Research or Combat)
2. Display all submarines in the fleet
3. Search for and display a submarine by serial number
4. Assign a mission to a submarine
5. Update a submarine's mission details (per its type)
6. End a submarine's mission
7. Combat: associate additional combat submarines with the same mission
8. Send a message from one combat submarine to another
9. Display the messages received by a submarine
10. Exit the system

## Not yet built

- Ground Station (TCP client side) — deferred; `docs/PROJECT_PLAN.md`'s
  original 3-app split was not what this build implements (see the project
  decision above), but a Ground Station-style TCP layer could sit in front of
  `Menu`/`Fleet` later without touching the modules above.
- No persistence of the *fleet* itself (submarines/missions/messages) across
  runs — only the Central Computer's own logs/data survive a restart. Adding
  fleet save/load would be a natural next step using the same `DataStore`
  pattern.
