# Submarine Monitoring System

Final project implementation of **SW-FD-LNC-001** ("Submarine Monitoring
System — Local Node Controller (LNC) End Unit", see [`final project.pdf`](final%20project.pdf)),
plus its "OOP Part" (a Submarine Fleet Management System). Three separate
binaries talk to each other exactly as the spec's system diagram describes:

```
        UART (real hardware)         Ethernet (TCP, localhost)
LNC end unit  <----------------->  Central Computer  <----------------->  Ground Station
(STM32 firmware)                  (C++17 console app                    (C++17 read-only
                                    + web dashboard                       TCP client)
                                    + TCP server)
```

All messages in the system are TLV (Tag-Length-Value)-encoded, per spec
§1.2. The LNC firmware's own TLV/frame source (`tlv.c`, `comm_frame.c`) is
compiled unmodified into the Central Computer, so there is exactly one
implementation of that protocol in the whole system — see
[`central-computer/README.md`](central-computer/README.md#protocol-reuse).

## Repository layout

| Path | What it is |
| --- | --- |
| [`submarine-final-project/`](submarine-final-project/) | STM32 Nucleo-L476RG firmware (FreeRTOS, CMake build) — the LNC end unit. Nine tasks per spec §2 (Monitor, Object Detection, Event, Log, Communication, Configuration, Init, Keep-Alive, Watchdog). |
| [`central-computer/`](central-computer/) | C++17 console app (spec §3 + the OOP Part's 10-operation fleet menu) with an embedded web dashboard and a read-only TCP server for the Ground Station. |
| [`ground_station/`](ground_station/) | C++17 TCP client (spec §4) — queries the Central Computer for submarine lists, logs, events, and summary reports, via a console menu and its own read-only web dashboard. |
| [`docs/`](docs/) | Planning (`PROJECT_PLAN.md`), living status (`SESSION_STATUS.md`), and firmware setup (`CUBEMX_CONFIG.md`, `WIRING_GUIDE.md`, `CUBEMX_CHECKLIST.md`). |
| [`hardware.md`](hardware.md) | Pin allocation and conflict-resolution history for the sensor shield + SD/RTC logger shield stack. |
| [`Claude outputs/`](Claude%20outputs/) | `GROUND_STATION_PROTOCOL.md` (the TCP wire protocol spec), plus testing/integration notes. |
| [`build-and-start.md`](build-and-start.md) | Quick build/run guide for the two PC-side binaries. |
| `final project.pdf` | The assignment's Software Functional Definition (SW-FD-LNC-001) — the authoritative spec. |

## Quick start

**PC side (Central Computer + Ground Station):** see
[`build-and-start.md`](build-and-start.md) — configure/build/test/run in
one page, plus a troubleshooting table for the common MSVC/CMake issues
hit on Windows.

**Firmware (STM32):** see [`docs/CUBEMX_CONFIG.md`](docs/CUBEMX_CONFIG.md)
(CubeMX peripheral setup) and [`docs/WIRING_GUIDE.md`](docs/WIRING_GUIDE.md)
(shield wiring), then build/flash via STM32CubeIDE or the project's own
CMake + `CMakePresets.json` in `submarine-final-project/`.

## Current status

`docs/SESSION_STATUS.md` is the living checklist of what's done, what's
pending, and what's still open (remaining hardware tests, submission
packaging, grading-rubric unknowns, etc.) — check it before assuming
anything below is fully finished. As of the last update: firmware
implements all nine spec modules with real TLV comms over UART; the
Central Computer implements spec §3 plus all 10 OOP-part menu operations,
a web dashboard, and the Ground Station's TCP server; the Management
Command module's Set Limits capability (spec §2.5/§3.2) is reachable from
both the console (extra menu item, beyond the 10 spec operations) and the
dashboard, in addition to Set/Get Time and the two retrieval instructions.
Three deliberate extensions beyond the spec's literal wording, all done at
the project owner's explicit request: every submarine (research included)
can now have a `CentralComputer`/LNC hardware link, not just combat ones;
each of the 4 environmental sensors (temp/humidity/light/battery) can be
individually enabled/disabled per submarine at the firmware level via the
same Set Limits interface; and the Ground Station also has its own
read-only web dashboard (port 8081) alongside its console menu, matching
the Central Computer's dashboard (port 8080) — a UI is not part of the
spec's Ground Station requirements at all, only the read-only queries
themselves.

## OOP Part — Submarine Fleet Management System

Per spec, the Central Computer also manages a fleet: research and combat
submarines, missions, and inter-submarine messaging during a mission —
implemented as `Submarine` → `ResearchSubmarine`/`CombatSubmarine`. The
spec says the central computer "should be treated as an object that
belongs to each combat submarine"; this project deliberately extends that
(project owner's explicit choice) so every submarine, research included,
owns its own `CentralComputer` and can be wired to real LNC hardware —
`ResearchSubmarine` just doesn't get the Combat-only participation/
messaging operations (7-9), which stay spec-literal. See
`central-computer/include/menu.h` for the 10 menu operations and
`central-computer/README.md` for the full class design.
