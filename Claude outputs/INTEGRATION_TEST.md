# End-to-End Integration Test Guide

This document walks through testing the complete submarine system with real hardware integration.

## Prerequisites

✅ **Completed Setup**:
- Central Computer console app (main.cpp) with Fleet/Menu OOP system
- Web dashboard (port 8080) displaying live submarine state
- TCP server (port 9000) for read-only Ground Station queries
- Ground Station CLI client with 4 query operations
- FreeRTOS firmware with all 9 tasks (Init, Monitor, Config, Event, Log, Comm, Command, Keep-Alive, Watchdog)
- TLV protocol (Tag-Length-Value) unmodified in both firmware and PC app

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    CENTRAL COMPUTER (Windows)                   │
│                                                                   │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Console Menu (Options 1-10)                              │   │
│  │ - Add submarine → creates CombatSubmarine + LNC hw conn  │   │
│  │ - Assign mission, messages, combat grouping             │   │
│  │ - Shares g_fleetMutex with dashboard & TCP server       │   │
│  └──────────────────────────────────────────────────────────┘   │
│                              │                                    │
│                    g_fleetMutex (coarse-grained)                 │
│                    shared Fleet object                           │
│                              │                                    │
│  ┌──────────────┐        ┌──────────────┐      ┌─────────────┐  │
│  │ HTTP Server  │        │  TCP Server  │      │ Serial Port │  │
│  │ :8080        │        │  :9000       │      │ (hardware)  │  │
│  │ (Dashboard)  │        │ (G.S. Query) │      │ → LNC FW    │  │
│  └──────────────┘        └──────────────┘      └─────────────┘  │
│         │                       │                      ▲          │
└─────────┼───────────────────────┼──────────────────────┼──────────┘
          │                       │                      │
    (browser on                   │              (UART 115200 bps)
    localhost:8080)               │
                                  │
                    ┌─────────────┘
                    │
              (TCP localhost:9000)
                    │
                    ▼
          ┌──────────────────┐
          │ GROUND STATION   │
          │ (CLI client)     │
          │ • List subs      │
          │ • Get logs       │
          │ • Get events     │
          │ • Summary report │
          └──────────────────┘
```

## Test Scenario: Full Fleet Mission

### Setup Phase

**Terminal 1 - Central Computer**:
```powershell
PS C:\Users\M0Y\Dev\Embedded\submarine\central-computer> .\build\Debug\central_computer.exe
Submarine Fleet Management System
Web dashboard: http://localhost:8080
Ground Station server: localhost:9000
```

The app is now running with:
- Console menu ready for input
- HTTP dashboard listening on port 8080
- TCP server listening on port 9000

### Test 1: Add Submarines (Console)

**Input** (Terminal 1):
```
Choice: 1
Type ('research' or 'combat'): combat
Serial number: C-9
Name: Cutlass
Serial port for this submarine's LNC (e.g. COM8 or /dev/ttyACM0), blank if no hardware connected: 
[Press ENTER - no hardware for initial test]
```

**Expected Output**:
```
-> C-9's central computer created without hardware (loopback placeholder).
Added Combat submarine C-9.
```

Repeat to add:
- C-11 (Combat) - Swordfish
- R-1 (Research) - Seeker

### Test 2: Web Dashboard Verification

**Open browser**:
```
http://localhost:8080
```

**Expected**: Dark-themed dashboard showing:
- All 3 submarines listed
- "Unassigned" status for each
- "Disconnected" status (no hardware)
- Forms to add/assign missions

### Test 3: Assign Missions (Console)

**Input** (Terminal 1):
```
Choice: 4
Serial number: C-9
Mission description: Patrol the Strait
Commander name: Cmdr Reyes
Personnel count: 12
```

**Expected**:
```
Mission assigned.
```

Repeat for C-11 and R-1 with different details.

### Test 4: Dashboard Update

**Refresh browser** (http://localhost:8080):
- Submarines now show "Assigned: true"
- Mission descriptions visible
- Personnel/researcher info displayed

### Test 5: Ground Station Query (Terminal 2)

**Start Ground Station**:
```powershell
PS C:\Users\M0Y\Dev\Embedded\submarine> .\build\Debug\ground_station.exe
Ground Station
Connecting to Central Computer at localhost:9000
Ground Station - Connected to Central Computer
All operations are read-only.

=== Ground Station Menu (Read-Only) ===
 1. List all submarines
 2. Get logs for a date range and submarine
 3. Get events for a date range and submarine
 4. Display summary report
 5. Exit
Choice: 
```

**Test 5a - List Submarines**:
```
Choice: 1
--- Submarine List ---
{
  "status": "ok",
  "data": [
    {
      "serial": "C-9",
      "type": "Combat",
      "name": "Cutlass",
      "missionAssigned": true,
      "missionDescription": "Patrol the Strait",
      "connected": false
    },
    ...
  ]
}
```

**Test 5b - Summary Report**:
```
Choice: 4
--- Summary Report ---
{
  "status": "ok",
  "data": {
    "totalSubmarines": 3,
    "combat": 2,
    "research": 1,
    "activeMissions": 2,
    ...
  }
}
```

### Test 6: Combat Messaging

**Back in Terminal 1 Console**:
```
Choice: 7
Combat submarine serial: C-9
Other combat submarine serial to associate: C-11
```

**Expected**:
```
Associated.
```

**Then send message**:
```
Choice: 8
From (combat submarine serial): C-9
To (combat submarine serial): C-11
Message: Proceeding to waypoint Alpha-1
```

**Expected**:
```
Message sent.
```

**Verify**:
```
Choice: 9
Combat submarine serial: C-11
  From C-9: Proceeding to waypoint Alpha-1
```

### Test 7: End Mission and Verify State

**Console**:
```
Choice: 6
Serial number: C-9
```

**Expected**:
```
Mission ended - C-9 is now available.
```

**Ground Station (Test again)**:
```
Choice: 1
```

**Expected**: C-9 now shows `"missionAssigned": false`

## Hardware Integration (With Real LNC)

When you have the STM32 hardware connected:

### Setup

**Terminal 1 - Central Computer** (same as before, but respond with serial port):
```
Choice: 1
Type ('research' or 'combat'): combat
Serial number: C-9
Name: Cutlass
Serial port for this submarine's LNC (e.g. COM8 or /dev/ttyACM0), blank if no hardware connected: COM8
-> Connected to COM8.
```

### Monitor Live Data

**Dashboard** (http://localhost:8080):
- Real-time sensor readings appear in the submarine card
- Last keep-alive timestamp updates every 2 seconds
- Mode, depth, pressure, temperature visible as firmware sends KEEPALIVE packets

**Ground Station** (Terminal 2):
```
Choice: 3
Submarine serial: C-9
Start date (YYYYMMDD): 20260905
End date (YYYYMMDD): 20260907
--- Events ---
{
  "status": "ok",
  "data": [
    {
      "timestamp": "20260906T095432",
      "type": "MODE_CHANGE",
      "details": "Changed from STANDBY to PATROL",
      "submarine": "C-9"
    },
    ...
  ]
}
```

## Checklist

### System Architecture
- [ ] Console app starts without errors
- [ ] Web dashboard loads on http://localhost:8080
- [ ] TCP server reports listening on port 9000
- [ ] No port conflicts (both 8080 and 9000 available)

### OOP (Fleet/Menu/Submarine)
- [ ] Can add combat submarine (CombatSubmarine created)
- [ ] Can add research submarine (ResearchSubmarine created)
- [ ] Serial number uniqueness enforced
- [ ] Mission assignment works
- [ ] Mission update works
- [ ] Mission end works

### Concurrency & Thread Safety
- [ ] Console operations block briefly while dashboard polls
- [ ] Dashboard responsive (2-second poll interval)
- [ ] No race conditions or crashes under concurrent access
- [ ] Mutex lock/unlock visible in logs (or no crashes confirms correctness)

### Web Dashboard
- [ ] HTML renders correctly (dark theme, grid background)
- [ ] Submarines display with live state
- [ ] Add submarine form works
- [ ] Assign mission form updates state
- [ ] End mission button works
- [ ] Real-time polling updates display

### TCP Ground Station
- [ ] Connects to Central Computer on port 9000
- [ ] LIST_SUBMARINES returns JSON array
- [ ] GET_LOGS returns events for date range
- [ ] GET_EVENTS returns events for date range
- [ ] SUMMARY_REPORT returns fleet summary
- [ ] All responses have "status" and "data" fields
- [ ] Error cases return proper error messages

### Combat Messaging
- [ ] Can associate two combat submarines
- [ ] Can send message between associated subs
- [ ] Message appears in receiver's message list
- [ ] Dashboard shows message count/details

### Hardware Integration (Optional)
- [ ] Central Computer connects to LNC on serial port
- [ ] Dashboard shows live sensor readings (mode, depth, pressure, temp)
- [ ] Keep-alive packets arrive every 2-5 seconds
- [ ] Ground Station queries return real event/log data
- [ ] Mode changes trigger events visible in queries

## Troubleshooting

### Port Already in Use
```
Web dashboard could not start on port 8080
Ground Station server could not start on port 9000
```
→ Kill existing central_computer.exe or web service using port 8080/9000

### Ground Station Can't Connect
```
Could not connect to Central Computer: connect() failed
```
→ Verify central_computer.exe is running with TCP server
→ Check firewall allows localhost:9000

### Serial Port Issues
```
Could not open COM8 (The system cannot find the specified file)
```
→ Verify LNC hardware is plugged in
→ Check Device Manager for correct COM port
→ Use blank entry (no hardware) for initial testing

### Dashboard Not Updating
→ Refresh browser (F5)
→ Check browser console for errors (F12 → Console tab)
→ Verify central_computer.exe is still running

## Next Steps

1. **Test with real hardware**: Connect STM32 Nucleo on COM8
2. **Verify protocol**: Check firmware serial output matches TLV format
3. **Long-running test**: Leave system running, monitor stability
4. **Performance**: Measure dashboard update latency under load
5. **Integration with Project Spec**: Cross-check all 10 menu options work
6. **Documentation**: Record test results and any issues
7. **Submission**: Prepare demo video and write-up

## Files to Commit After Testing

```
central-computer/src/main.cpp       # Console + dashboard + TCP server
central-computer/src/tcp_server.cpp # HTTP/TCP server implementation  
central-computer/src/tcp_api.cpp    # Ground Station read-only API
ground_station/src/main.cpp         # Ground Station entry point
ground_station/src/tcp_client.cpp   # TCP client implementation
ground_station/src/ground_station_cli.cpp  # Interactive menu
CMakeLists.txt                      # Root build config with ground_station
GROUND_STATION_PROTOCOL.md          # This protocol spec
INTEGRATION_TEST.md                 # This test guide
```

All changes ready to push to `main` branch after successful testing.
