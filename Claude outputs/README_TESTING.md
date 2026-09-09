# Submarine Fleet Management System - Testing & Integration Guide

## 🚀 Quick Navigation

**Just want to test it?** → Start with **[QUICKSTART_TESTING.md](QUICKSTART_TESTING.md)** (20 minutes)

**Need comprehensive testing?** → See **[INTEGRATION_TEST.md](INTEGRATION_TEST.md)** (detailed scenarios)

**Want to understand the protocol?** → Read **[GROUND_STATION_PROTOCOL.md](GROUND_STATION_PROTOCOL.md)**

**Need technical architecture?** → Check **[SESSION_SUMMARY.md](SESSION_SUMMARY.md)**

---

## System Overview

The Submarine Fleet Management System consists of three independent binaries working together:

```
┌─────────────────────────────┐
│   CENTRAL COMPUTER          │
│  ├─ Console Menu (10 ops)   │
│  ├─ Web Dashboard :8080     │
│  └─ TCP Server :9000        │
└────────────┬────────────────┘
             │ TCP
             ▼
┌─────────────────────────────┐
│   GROUND STATION            │
│  ├─ Read-Only CLI           │
│  └─ 4 Query Operations      │
└─────────────────────────────┘

┌─────────────────────────────┐
│   LNC FIRMWARE              │
│  ├─ STM32 Nucleo L476RG     │
│  ├─ FreeRTOS (9 tasks)      │
│  └─ TLV Protocol via UART   │
└─────────────────────────────┘
```

## Getting Started

### 1. Prerequisites
- Windows 10+ (or Linux for dev)
- CMake 3.16+ **OR** Visual Studio **OR** MSVC in PATH
- (Optional) STM32 Nucleo L476RG for hardware testing

### 2. Build (Choose One)

**Using CMake** (recommended):
```powershell
cd submarine
cmake -B build
cmake --build build -j
```

**Using Visual Studio**:
1. Open `central-computer\build\central_computer.vcxproj`
2. Add `ground_station` project to solution
3. Build All

**Using Manual MSVC**:
```powershell
cd ground_station
./build.bat
```

### 3. Test (20 minutes)

Follow **QUICKSTART_TESTING.md**:
1. Start Central Computer (Terminal 1)
2. Open Web Dashboard (Browser)
3. Start Ground Station (Terminal 2)
4. Add submarine, assign mission, verify in all three interfaces

---

## Documentation Map

| Document | Purpose | Read Time |
|----------|---------|-----------|
| **QUICKSTART_TESTING.md** | Get the system running in 20 minutes | 5 min |
| **INTEGRATION_TEST.md** | Comprehensive test scenarios with checklist | 15 min |
| **GROUND_STATION_PROTOCOL.md** | TCP protocol specification | 10 min |
| **SESSION_SUMMARY.md** | Technical architecture and decisions | 10 min |
| **This file** | Navigation guide | 2 min |

---

## Key Features

### Console Menu (10 Operations)
1. Add submarine (research or combat)
2. Display all submarines
3. Search submarine by serial
4. Assign mission
5. Update mission
6. End mission
7. Associate combat submarines
8. Send message between combat subs
9. Display received messages
10. Exit

### Web Dashboard (Real-Time)
- List all submarines with live state
- Add submarine form
- Assign/update mission forms
- Display current operations
- 2-second polling for live updates

### Ground Station Queries (Read-Only)
1. **LIST_SUBMARINES** - Get all submarines with status
2. **GET_LOGS** - Query logs by submarine and date range
3. **GET_EVENTS** - Query events by submarine and date range
4. **SUMMARY_REPORT** - Fleet-wide summary statistics

---

## Testing Checklist

### Basic Functionality
- [ ] Central Computer starts without errors
- [ ] Web dashboard loads on http://localhost:8080
- [ ] Ground Station connects to port 9000
- [ ] Can add submarine via console
- [ ] Dashboard updates in real-time
- [ ] Ground Station queries return JSON

### OOP & Menu Operations
- [ ] Add combat and research submarines
- [ ] Assign missions with type-specific fields
- [ ] Update mission details
- [ ] End mission and verify availability
- [ ] Associate combat submarines
- [ ] Send and receive messages

### Data Consistency
- [ ] Console, dashboard, and Ground Station show same data
- [ ] Missions persist across operations
- [ ] Message history correct
- [ ] State transitions valid

### Hardware Integration (Optional)
- [ ] Connect STM32 Nucleo on COM port
- [ ] Central Computer receives keep-alive packets
- [ ] Dashboard shows live sensor readings
- [ ] Ground Station queries return real event logs
- [ ] Firmware mode changes visible in both interfaces

---

## Troubleshooting

### Build Issues

**CMake not found**:
- Install CMake from https://cmake.org
- Or use Visual Studio build option

**Compilation errors**:
- Check C++17 compiler support (MSVC 2017+)
- Verify no old build artifacts: `rm -r build`

### Runtime Issues

**Port already in use**:
```powershell
# Find and kill existing process
Get-Process central_computer | Stop-Process
```

**Ground Station can't connect**:
- Check Central Computer is running
- Verify "Ground Station server: localhost:9000" message appears
- Check firewall allows localhost:9000

**Dashboard not updating**:
- Refresh browser (F5)
- Check browser console (F12) for errors
- Verify central_computer.exe still running

**Serial port errors** (hardware testing):
- Check Device Manager for correct COM port
- Verify no other application using port
- Use blank entry for no-hardware mode

---

## File Structure

```
submarine/
├── README_TESTING.md              ← You are here
├── QUICKSTART_TESTING.md          ← Start here for quick test
├── INTEGRATION_TEST.md            ← Comprehensive test guide
├── GROUND_STATION_PROTOCOL.md     ← Protocol specification
├── SESSION_SUMMARY.md             ← Technical summary
│
├── central-computer/
│   ├── src/main.cpp               ← Console + Dashboard + TCP server
│   ├── src/tcp_server.cpp         ← HTTP/TCP server implementation
│   ├── src/tcp_api.cpp            ← Ground Station API handlers
│   ├── include/...                ← Fleet, Submarine, Menu headers
│   ├── CMakeLists.txt             ← C++ build config
│   └── build/                     ← Compiled binaries (Debug/)
│
├── ground_station/
│   ├── src/main.cpp               ← Ground Station CLI entry point
│   ├── src/tcp_client.cpp         ← TCP client implementation
│   ├── src/ground_station_cli.cpp ← Interactive menu
│   ├── include/...                ← TCP client headers
│   ├── CMakeLists.txt             ← Ground Station build config
│   └── build.bat                  ← Manual MSVC build script
│
├── submarine-final-project/       ← LNC Firmware (STM32 + FreeRTOS)
│   └── Core/Src/app/
│       ├── tlv.c                  ← TLV protocol (shared with PC)
│       └── comm_frame.c           ← Communication frame handling
│
├── CMakeLists.txt                 ← Root build (includes both binaries)
└── docs/                          ← Flowcharts and component diagrams
```

---

## Communication Paths

### 1. Console Menu ↔ Fleet
- Synchronous (blocking prompts)
- g_fleetMutex serializes access
- All 10 operations lock entire Fleet

### 2. Web Dashboard ↔ Fleet
- HTTP GET on port 8080
- 2-second polling from browser
- JSON response includes live sensor state

### 3. Ground Station ↔ Central Computer
- TCP on port 9000
- Text commands with JSON responses
- Read-only (no Fleet mutations)

### 4. Central Computer ↔ LNC Firmware
- UART 115200 bps (selectable COM port)
- TLV (Tag-Length-Value) protocol
- Bidirectional (commands and keep-alives)

---

## Next Steps

### Immediate
1. Follow **QUICKSTART_TESTING.md** (20 minutes)
2. Verify system starts and all three interfaces work
3. Review **INTEGRATION_TEST.md** for deeper testing

### For Hardware Testing
1. Connect STM32 Nucleo to COM port
2. Provide port name when adding submarine (e.g., COM8)
3. Watch dashboard for real-time sensor updates
4. Query events/logs via Ground Station

### For Submission
1. Complete all testing from INTEGRATION_TEST.md
2. Document any issues or workarounds
3. Prepare demo video or script
4. Write final build/run instructions
5. Commit submission version to GitHub

---

## Support Resources

- **INTEGRATION_TEST.md**: Detailed test scenarios with expected outputs
- **SESSION_SUMMARY.md**: Architecture diagrams and design decisions
- **GROUND_STATION_PROTOCOL.md**: Complete protocol reference
- **central-computer/include/**: Source code comments and headers
- **CMakeLists.txt**: Build configuration documentation

---

## Status

✅ **Complete & Ready for Testing**

- Ground Station TCP client fully implemented
- Central Computer with console, dashboard, and TCP server
- Root CMakeLists.txt builds both binaries
- Three-binary separation complete
- All documentation provided
- Git committed to main branch

**Your turn**: Build on Windows and run QUICKSTART_TESTING.md!

---

**Last Updated**: September 7, 2026  
**Git Commit**: 1450363 (Ground Station TCP client)  
**Status**: Ready for testing and hardware validation
