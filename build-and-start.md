# Build & Start — Submarine Monitoring System

Quick guide for building and running the host-PC side of the project
(Central Computer + Ground Station). For the STM32 firmware see
[`docs/CUBEMX_CONFIG.md`](docs/CUBEMX_CONFIG.md) and
[`docs/WIRING_GUIDE.md`](docs/WIRING_GUIDE.md).

_Last verified: 2026-09-07, Windows + Visual Studio generator._

## Prerequisites

- C++17 compiler (MSVC, MinGW-w64, or GCC)
- CMake >= 3.16
- No external C++ libraries are needed (HTTP server, JSON writer, and the
  test harness are hand-rolled).

## 1. Configure

Run from the repo root (`submarine/`):

```powershell
cmake -S . -B build
```

This configures both `central-computer` and `ground_station`.
Expected output ends with:

```
-- Build files have been written to: C:/.../submarine/build
```

> **If configuration fails with `error C1090: PDB API call failed, error code '3'`:**
> MSVC cannot write the debug `.pdb`. On a fresh machine this is usually
> Windows Defender (or another AV) locking the file during compilation. Fix:
>
> ```powershell
> Add-MpPreference -ExclusionPath "C:\Users\M0Y\Dev\Embedded\submarine\build"
> Remove-Item -Recurse -Force .\build
> cmake -S . -B build
> ```
>
> If it still fails, try the Ninja generator instead (different PDB pipeline):
> `cmake -S . -B build -G Ninja`.

## 2. Build

```powershell
cmake --build build -j --config Debug
```

Binaries land in (Visual Studio generator):

```
build\central-computer\Debug\central_computer.exe
build\ground_station\Debug\ground_station.exe
build\central-computer\Debug\test_*.exe
```

With the Ninja generator the executables are directly under `build\` instead
(CMake defaults to `RelWithDebInfo` for single-config generators).

## 3. Test

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Expect **5/5 passing**: `test_central_computer`, `test_comm_link`,
`test_dashboard`, `test_fleet`, `test_protocol_bridge`.

Notes:

- `-C Debug` is required with the multi-config (Visual Studio) generator —
  without it options can bite you.
- If `ctest` reports "no tests found", the root `CMakeLists.txt` is missing
  `enable_testing()` (it must be in the top-level file, not just a
  subdirectory). It is present in this repo.

## 4. Run

### Central Computer (console menu + web dashboard + TCP server)

```powershell
.\build\central-computer\Debug\central_computer.exe
```

Always run the exe from `build\central-computer\Debug\` — a stale copy
elsewhere can look like "the dashboard isn't there".

The dashboard page itself is a real file, `central-computer\web\dashboard.html`,
read from disk (not baked into the exe). CMake copies it next to the built
exe automatically after every build, so running from `build\central-computer\Debug\`
(as above) just works. For live editing without rebuilding, run the exe from
the `central-computer\` source directory instead - it then reads the real
source file directly, and a browser refresh picks up any edit immediately.

- **Console** — the OOP part's 10-operation fleet management menu
  (add submarines, start missions, query data/logs, ...), plus an 11th
  extra item beyond the spec's 10: Set a submarine's sensor limits and/or
  enable/disable a sensor (spec §2.5/§3.2's Management Command, sent live
  over UART to a connected submarine's LNC - research submarines can have
  hardware too now, not just combat ones, a deliberate extension beyond
  the spec).
- **Web dashboard** — open <http://localhost:8080>
- **TCP server** — listens on `localhost:9000` (read-only, for the Ground Station)

### Ground Station (read-only TCP client)

In a second terminal:

```powershell
.\build\ground_station\Debug\ground_station.exe
```

Connects to `localhost:9000` by default and can list submarines, fetch
logs/events, and print a fleet summary.

- **Console** — the read-only 5-item menu above.
- **Web dashboard** — open <http://localhost:8081> in a browser. A second,
  independent read-only web UI (mirrors the Central Computer's own
  dashboard, but with no add/mutate forms - Ground Station cannot change
  fleet state, only view the same log/event/summary data the console menu
  can). Shares the same TCP connection to the Central Computer as the
  console menu; either interface can be used at the same time as the
  other. Same rule as the Central Computer's dashboard applies: CMake
  copies `ground_station\web\dashboard.html` next to the built exe, so run
  it from `build\ground_station\Debug\` for the page to be found.

## 5. Talking to the real LNC firmware (STM32)

Only needed when hardware is connected. Serial port defaults differ per OS
(`COM8` on Windows, `/dev/ttyACM0` on Linux). Python tooling:

```powershell
pip install pyserial
python3 submarine-final-project\tools\lnc_test_tool.py --port COM5 listen
```

Other subcommands: `get-time`, `set-time`, `set-limits`, `get-data`,
`get-events`.

Note: `set-limits`/`get-time`/etc. here talk to the firmware directly and
bypass the Central Computer entirely - useful for isolating a firmware-only
issue. The Central Computer app itself now also exposes Set Limits (console
option 11, or the dashboard's "Set sensor limits / enable-disable" card)
against whichever submarine you've connected with a real serial port -
research submarines can have hardware too now, not just combat ones -
that's the path that exercises the full stack end-to-end, not just the
firmware. Option 11 also carries a per-sensor enable/disable toggle for
the 4 environmental sensors (temp/humidity/light/battery) - a deliberate
extension beyond the spec.

## Troubleshooting

| Symptom | Likely cause / fix |
| --- | --- |
| `C1090: PDB API call failed` | AV locking `.pdb` — add Defender exclusion, wipe `build/`, reconfigure (see above). |
| `ctest` finds no tests | `enable_testing()` missing from root `CMakeLists.txt`, or wrong `--test-dir`. |
| Tests "not found" / `NOT_AVAILABLE` | Running ctest without `-C Debug` under the VS generator. |
| Dashboard won't load | Run the exe from `build\central-computer\Debug\`, not a stale copy. |

## See also

- [`README.md`](README.md) — project overview, repo layout, architecture
- [`central-computer/README.md`](central-computer/README.md) — module & design docs
- [`docs/PROJECT_PLAN.md`](docs/PROJECT_PLAN.md) — original phased plan
- [`docs/SESSION_STATUS.md`](docs/SESSION_STATUS.md) — what's done / what's next