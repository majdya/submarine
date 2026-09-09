# Quick Start: Testing the Submarine System

## What's Ready Now

✅ **Three-Binary System Complete**
- Central Computer (console + dashboard + TCP server)
- Ground Station (read-only TCP client)
- LNC Firmware (STM32 + FreeRTOS)

## 1. Build on Windows (5 minutes)

### Option A: Using CMake (if installed)
```powershell
cd C:\Users\M0Y\Dev\Embedded\submarine
rm -r build -Force  # Clean old build
cmake -B build
cmake --build build -j
```

### Option B: Using Visual Studio
Open `central-computer\build\central_computer.vcxproj` in Visual Studio, add `ground_station` project, build all.

### Option C: Manual MSVC (if CMake not available)
Run `ground_station\build.bat` with Visual Studio environment activated.

**Expected result**: 
- `build\Debug\central_computer.exe` (1.4 MB)
- `build\Debug\ground_station.exe` (≈350 KB)

## 2. Run the Integration Test (10 minutes)

**Open 3 terminals in PowerShell or Command Prompt:**

### Terminal 1: Start Central Computer
```powershell
PS> .\build\Debug\central_computer.exe
Submarine Fleet Management System
Web dashboard: http://localhost:8080
Ground Station server: localhost:9000
=== Submarine Fleet Management System ===
 1. Add a new submarine to the fleet
 2. Display all submarines in the fleet
 ...
Choice: 
```

### Terminal 2: Open Web Dashboard
```powershell
# Just open browser
http://localhost:8080

# You should see a dark-themed dashboard with an empty fleet
```

### Terminal 3: Start Ground Station
```powershell
PS> .\build\Debug\ground_station.exe
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

## 3. Test Scenario (5 minutes)

**In Terminal 1 (Central Computer)**, add a submarine:
```
Choice: 1
Type ('research' or 'combat'): combat
Serial number: C-9
Name: Cutlass
Serial port for this submarine's LNC (blank if no hardware): 
[Press ENTER - no hardware for now]
```

Expected output:
```
-> C-9's central computer created without hardware (loopback placeholder).
Added Combat submarine C-9.
```

**In Browser (Terminal 2)**, refresh http://localhost:8080
- You should see C-9 listed
- Status shows "Unassigned" and "Disconnected"

**Back in Terminal 1**, assign a mission:
```
Choice: 4
Serial number: C-9
Mission description: Patrol the Strait
Commander name: Cmdr Reyes
Personnel count: 12
```

Expected output:
```
Mission assigned.
```

**Refresh Browser again**
- C-9 now shows "Assigned: true"
- Mission details visible

**In Terminal 3 (Ground Station)**, query the fleet:
```
Choice: 1
```

Expected output:
```
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
    }
  ]
}
```

**Try Summary Report**:
```
Choice: 4
```

Expected output shows fleet stats including mission count.

## 4. Verify Everything Works

✅ **Checklist**:
- [ ] Central Computer console starts without errors
- [ ] Web dashboard loads on http://localhost:8080
- [ ] Ground Station connects to TCP server
- [ ] Can add submarine via console
- [ ] Dashboard updates in real-time (refresh shows new data)
- [ ] Ground Station queries return JSON with correct structure
- [ ] Can assign mission and see it in all three interfaces

## 5. Next Steps

### For Hardware Testing
If you have the STM32 Nucleo connected:

**In Terminal 1**, when adding a submarine, provide the COM port:
```
Serial port for this submarine's LNC: COM8
```

Then:
- Watch dashboard for real-time sensor updates
- Check Terminal 1 for keep-alive messages
- Use Ground Station to query event logs

### For Full Feature Testing
See **INTEGRATION_TEST.md** for:
- Combat message passing between submarines
- Mission update operations
- Mission end and state recovery
- 7-day log retention
- Event filtering by date range

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Port 8080/9000 already in use | Kill existing process: `Get-Process central_computer \| Stop-Process` |
| Ground Station can't connect | Check central_computer.exe is running with "Ground Station server: localhost:9000" message |
| Dashboard not updating | Refresh browser (F5) or check browser console (F12) |
| Build fails with CMake not found | Use Option B (Visual Studio) or Option C (Manual MSVC) |
| Ground Station menu shows garbled input | Make sure UTF-8 is enabled in terminal (Terminal → Default → Character Set → UTF-8) |

## Files to Know

- `central-computer/src/main.cpp` — Console menu, dashboard, TCP server startup
- `ground_station/src/main.cpp` — Ground Station entry point
- `build/Debug/central_computer.exe` — The main app binary
- `build/Debug/ground_station.exe` — The read-only client binary
- `GROUND_STATION_PROTOCOL.md` — Full TCP protocol spec
- `INTEGRATION_TEST.md` — Comprehensive test scenarios
- `SESSION_SUMMARY.md` — Technical summary of changes

## Success Criteria

You've successfully completed the integration test when:
1. Central Computer starts and serves HTTP + TCP
2. Web dashboard displays submarines and accepts missions
3. Ground Station queries return valid JSON
4. All three interfaces show consistent data
5. No crashes or runtime errors after 5+ minutes of operation

---

**Estimated Total Time**: 20 minutes (build + test)  
**Status**: Ready to test — all components built and integrated  
**Questions?**: See INTEGRATION_TEST.md for detailed scenarios or SESSION_SUMMARY.md for architecture details
