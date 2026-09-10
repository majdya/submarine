# STM32 Firmware: LNC End Unit (submarine-final-project)

This directory contains the STM32 Nucleo-L476RG firmware for the Submarine Monitoring System's Local Node Controller (LNC) end unit per spec SW-FD-LNC-001.

**Architecture:**
- **OS:** FreeRTOS (CMSIS-RTOS v2 interface)
- **Compiler:** ARM GCC (STM32CubeIDE built-in)
- **Build system:** CMake (with STM32CubeIDE as primary IDE for peripheral configuration)
- **Protocol:** TLV (Tag-Length-Value) over UART2, shared code reused by the Central Computer PC app
- **Modules:** Nine FreeRTOS tasks per spec §2 (Monitor, Object Detection, Event, Log, Communication, Configuration, Init, Keep-Alive, Watchdog)

---

## Quick Start

### 1. CubeMX Peripheral Configuration

Before writing application code, configure all peripherals via STM32CubeIDE's CubeMX editor:

1. Open `Core/` → right-click on `PROJECT_NAME.ioc` → Open with CubeMX
2. Follow **all steps** in [`docs/CUBEMX_Setup.md`](../docs/CUBEMX_Setup.md) (consolidated peripheral checklist)
3. Generate code (Project → Generate Code)
4. Verify it compiles with zero application logic added

**Key configuration items:**
- Clock: HSI→PLL→80MHz
- UART2: 115200 baud (to Central Computer over VCP)
- SPI1: SD card reader
- TIM3: PWM for buzzer + RGB LED
- ADC1/ADC2: Environmental sensors
- I2C3: DS1307 RTC (or internal RTC — decision needed, see CUBEMX_Setup.md §7)
- FreeRTOS: CMSIS_V2 interface, 8–15 KB heap

### 2. Wiring Shields

Connect the sensor shields following [`docs/WIRING_GUIDE.md`](../docs/WIRING_GUIDE.md). The stack layout is:
- **Top:** 9-in-1 Sensor Shield (DHT, LDR, LM35, buttons, RGB, buzzer)
- **Middle:** Arduino Data Logger Shield (SD card, RTC)
- **Base:** STM32 Nucleo-L476RG

Pin conflicts are documented in [`docs/hardware.md`](../docs/hardware.md).

### 3. Build (CMake in STM32CubeIDE)

**Option A: Via STM32CubeIDE (recommended for development)**

1. File → New → STM32 Project
2. Select board **NUCLEO-L476RG**
3. Create with desired project name
4. CubeMX configures; generate code
5. Build → Clean; Build → Build Project
6. Flashing: right-click project → Run As → STM32 C/C++ Application

**Option B: Via CMake (command line)**

```bash
cd submarine-final-project
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/stm32cubemx/gcc_arm_toolchain.cmake
cmake --build build -j
# Binary: build/Debug/submarine-final-project.elf
```

> **Windows note:** The STM32 GCC ARM toolchain path must be in CMake's search path or specified explicitly. Verify `cmake/stm32cubemx/gcc_arm_toolchain.cmake` has the correct `GCC_TOOLCHAIN_PREFIX` for your installation.

### 4. Flash to Device

**Via STM32CubeIDE:**
1. Connect Nucleo board via USB
2. Right-click project → Debug As → STM32 C/C++ Application

**Via command line (ST-LINK):**
```bash
st-flash write build/Debug/submarine-final-project.bin 0x08000000
```

**Via CubeProgrammer (GUI):**
1. Open CubeProgrammer
2. Connect → Click → Load binary at `0x08000000` → Download

---

## Testing & Verification

### Hardware Tests

A complete test checklist is in [`TEST_TRACKING.md`](TEST_TRACKING.md). Verify the following on real hardware:

**Confirmed (12 items):**
- ✅ object_detect_toggle
- ✅ silence_button
- ✅ log_file_creation
- ✅ rtc_battery_backed
- ✅ mode_reporting
- ✅ keepalive_cadence
- ✅ comm_frame_realworld
- ✅ command_set_time
- ✅ command_get_time
- ✅ command_set_limits
- ✅ command_get_data
- ✅ command_get_events

**Still needs testing:**
- [ ] mode_transitions
- [ ] rgb_mode_color
- [ ] rgb_alarm_interaction
- [ ] alarm_buzzer
- [ ] flash_config_persistence
- [ ] log_retention_7day

**Blocked/watch items:**
- [ ] iwdg_watchdog (waiting for explicit enable)
- [ ] possible_silence_double_fire (unconfirmed debounce issue)
- [ ] sensor_reading_anomaly (unresolved)

See [`TEST_TRACKING.md`](TEST_TRACKING.md) for full detail and test procedures.

### Testing with Python Tool

The firmware can be tested directly (bypassing the Central Computer app) using the Python test tool:

```bash
pip install pyserial
python3 tools/lnc_test_tool.py --port COM5 listen
```

Subcommands: `get-time`, `set-time`, `set-limits`, `get-data`, `get-events`.

---

## Architecture & Module Design

### Nine FreeRTOS Tasks (per spec §2)

1. **Init Task** — Synchronizes startup with Central Computer time, logs reset cause, signals Event module
2. **Configuration Task** — Manages limit table in internal flash, notifies Event on change
3. **Monitor Task** — Samples environment (DHT, ADC) every 5s, checks against limits, pushes to Log/Event
4. **Object Detection Task** — Detects/cleared transitions, pushes to Event (deferred interface for hardware choice)
5. **Event Task** — Consumes events from queue, drives LED/buzzer, writes SD log, forwards to Communication
6. **Log Task** — Manages SD card with FatFs; implements 7-day rolling retention
7. **Communication Task** — Parses inbound TLV from UART, routes commands to Configuration, drain outbound queue (events, data, keep-alive)
8. **Keep-Alive Task** — 6s timer, sends timestamp + latest measurement to Central Computer
9. **Watchdog Task** — IWDG refresh at safe cadence

### Data Flow

```
Environmental Sensors (DHT, ADC) ──→ Monitor Task ──→ Log Task (SD card)
                                         │
                                         ↓
                                   Event Task ──→ Communication Task ──→ UART to Central Computer
                                         ↑
                                  Configuration changes (Management Commands from Central Computer)
```

### Module Code Structure

```
Core/
  ├─ Inc/ / Src/          ← CubeMX-generated peripherals (main.c, HAL init, IT handlers)
  └─ app/                 ← Application modules (C++ classes with C trampolines)
      ├─ task_*.cpp/.hpp  ← Each of 9 tasks
      ├─ app_config.*     ← Configuration (flash persistence)
      ├─ app_event.*      ← Event queue & LED/buzzer logic
      └─ ...

Drivers_App/
  ├─ dht.c/.h           ← DHT11 single-wire driver
  ├─ adc_sensors.c/.h   ← ADC (battery, light, temperature)
  ├─ sd_fatfs.c/.h      ← SD card with FatFs
  ├─ rgb_led.c/.h       ← RGB LED PWM (3-channel)
  ├─ buzzer.c/.h        ← Buzzer tone generator (PWM frequency control)
  ├─ button.c/.h        ← Button EXTI handlers
  └─ rtc_time.c/.h      ← DS1307 RTC (I2C)

Protocol/
  ├─ tlv.c/.h           ← TLV encode/decode (shared with Central Computer)
  ├─ comm_frame.c/.h    ← Frame structure & CRC
  └─ message_defs.h     ← Message tag definitions
```

---

## Known Issues & Gotchas

| Issue | Status | Notes |
|---|---|---|
| SD card write causes LD2 flicker | Known | SPI1 shares `PA5` with LD2; this is expected and cosmetic. |
| IWDG watchdog disabled | Blocked | Currently off; needs explicit re-enable and testing. |
| Button debounce (double-fire) | Watch | Unconfirmed issue; needs isolated test. |
| Humidity sensor parked | Deferred | ADC conflict; decision pending on whether to use internal or external mux. |
| Debug binary via USB | Known | The firmware build process has a mismatch when debugging over the USB VCP. Workaround: use a separate UART cable or external debugger. |

---

## Development Workflow

1. **Make code changes** in `Core/app/` or `Drivers_App/`
2. **Rebuild** (CMake or STM32CubeIDE build)
3. **Flash** to Nucleo (debug session or command line)
4. **Verify** against [`TEST_TRACKING.md`](TEST_TRACKING.md) checklist or test with `tools/lnc_test_tool.py`

**For CubeMX peripheral changes:**
1. Open `.ioc` file in CubeMX
2. Make changes (follow [`docs/CUBEMX_Setup.md`](../docs/CUBEMX_Setup.md))
3. Generate code
4. Rebuild project
5. Flash and re-test

---

## See Also

- **Spec:** [`docs/datasheets/final project.pdf`](../docs/datasheets/final%20project.pdf)
- **CubeMX setup:** [`docs/CUBEMX_Setup.md`](../docs/CUBEMX_Setup.md)
- **Wiring:** [`docs/WIRING_GUIDE.md`](../docs/WIRING_GUIDE.md)
- **Hardware details:** [`docs/hardware.md`](../docs/hardware.md)
- **Test tracking:** [`TEST_TRACKING.md`](TEST_TRACKING.md)
- **Central Computer:** [`central-computer/README.md`](../central-computer/README.md) (PC-side app, TLV protocol reuse)
- **Live status:** [`docs/SESSION_STATUS.md`](../docs/SESSION_STATUS.md)
