# Submarine Monitoring System — Build Plan

Source spec: `final project.pdf` (SW-FD-LNC-001). This plan turns that spec into an ordered set of buildable, testable chunks for the STM32 Nucleo-L476RG (Cortex-M4) LNC firmware, the two PC-side tiers, and the separate OOP fleet-management app.

## Locked-in architecture decisions

- **Scheduling (LNC):** FreeRTOS. One task per spec module (Monitor, Object Detection, Event, Log, Communication, Configuration, Init, Keep-Alive, Watchdog), communicating over FreeRTOS queues/mutexes rather than shared globals.
- **Central Computer / Ground Station:** Two separate PC-side C/C++ programs, not simulated inside the firmware. Central Computer ↔ LNC over the Nucleo's USB virtual COM UART. Ground Station ↔ Central Computer over a local TCP socket standing in for Ethernet.
- **Object Detection hardware:** Deferred. Build the module behind a `detected/cleared` interface now (a small vtable/function-pointer struct); wire in the real sensor (ultrasonic, IR, or a button stand-in) once decided, with zero change to Event/Communication logic.
- **On-device storage:** SD card over SPI + FatFs, giving the spec's date-named files and 7-day rotation literally rather than emulating "files" in Flash.
- **Language split:** HAL callbacks, ISRs, and CubeMX-generated code stay in C. Each software module is a C++ class; every task entry point and every ISR/HAL callback that touches a module is a thin `extern "C"` C trampoline function that forwards into the C++ object. No module allocates from the heap after `Init` completes — queues, pools, and buffers are sized and allocated once at startup.

## Repo layout (both existing `c/` and `cpp/` folders stay as course material; new code goes here)

```
submarine/
  firmware/                  <- STM32CubeIDE/CubeMX project for the LNC
    Core/                    <- CubeMX-generated (main.c, IT handlers, HAL config)
    Modules/
      monitor/  object_detection/  event/  log/  communication/
      configuration/  init/  keepalive/  watchdog/
    Drivers_App/              <- thin C wrappers: dht.c, adc_sensors.c, sd_fatfs.c,
                                  rgb_led.c, buzzer.c, button.c, rtc_time.c
    Protocol/                 <- tlv.c/.h, message_defs.h  (shared with central_computer/)
  central_computer/           <- PC app (C/C++)
    src/                      <- uart_link, tcp_server (Ground Station side), mgmt_command,
                                  log, data_store
    Protocol/                 <- copy or symlink of firmware/Protocol
  ground_station/             <- PC app (C/C++), TCP client + simple CLI/report view
  fleet_oop/                  <- OOP Part: Submarine Fleet Management System (separate console app)
  docs/                       <- this plan, protocol spec, sequence diagrams
```

## Phase 1 — Protocol & project skeleton
- Define the TLV message catalogue in one shared header: measurement report, event report (per §2.3.1–2.3.4), keep-alive, the 8 management-command tags (§2.5), the 2 retrieval-instruction tags, and ACK/NACK. Write `tlv_encode`/`tlv_decode` with static buffers only, plus a host-side unit test (no hardware needed) that round-trips every message type.
- CubeMX: create the Nucleo-L476RG project, enable FreeRTOS (CMSIS-RTOS v2), UART2 (ST-LINK VCP), SPI1 (SD card), ADC1 (potentiometer + light sensor channels), one GPIO EXTI (button), 3 GPIO outputs (RGB LED) or a PWM-capable pin set, RTC, IWDG.
- Verify: blink + one FreeRTOS task + a UART "hello" over the VCP, before any spec logic is written.

## Phase 2 — Core infrastructure
- **Configuration module:** internal-Flash read/write of the limit table (temp/humidity/light/battery × Normal/Warning bounds), default values on first boot (empty Flash), notifies Event on change.
- **RTC time utility:** set/get date-time, used by every module that timestamps.
- **Watchdog module:** IWDG refresh task on a safe cadence tied to the longest expected task period.
- **Init module:** requests time sync from Central Computer via Communication, records whether the reset cause register shows a watchdog reset, signals Event, then starts the remaining tasks.

## Phase 3 — Sensing & local reaction
- **Monitor module:** FreeRTOS software timer at 5 s; reads temp+humidity (single DHT sensor driver), battery via potentiometer ADC channel, light via ADC/photoresistor channel; compares against Configuration's limits; always pushes to Log; pushes to Event only on a mode change.
- **Object Detection module:** built to the deferred interface above; continuous task loop; pushes detected/cleared transitions to Event.
- **Event module:** consumes one event queue fed by Monitor/ObjectDetection/Configuration/Init; timestamps on arrival; drives the RGB LED and buzzer per the §2.3 transition table; button EXTI callback silences the buzzer; writes every event to the SD card event log; forwards Monitor/ObjectDetection-sourced events to the Communication outbound queue.
- **Log module + SD/FatFs:** date-named files, 7-day rolling retention (delete oldest on the 8th day), one write API shared by Monitor's periodic data and Event's event log.

## Phase 4 — Communication
- UART driver: interrupt or DMA RX into a ring buffer, framed by the TLV layer.
- Transport-independence: define `comm_transport_t` (init/send/recv function pointers); the UART driver is the first implementation, so swapping in Ethernet later touches only one file.
- Communication task: parses inbound TLV, routes management commands to Configuration, routes retrieval instructions to the Log/SD module, and drains a priority-ordered outbound queue (keep-alive > event > data report) — three FreeRTOS queues of different priority polled in fixed order, or one queue with a priority field, either works.
- **Keep-Alive module:** 6 s timer, builds timestamp + latest measurement + current mode, enqueues at highest priority.

## Phase 5 — Central Computer (PC app)
- Reuses the `Protocol/` TLV code unmodified.
- LNC-facing Communication module over the same VCP UART; a Management Command module to issue the 8 config commands + RTC set/get; a Log module writing received data/events to files or a small database; a Data Collection & Analysis module producing simple breakdown reports; a TCP server exposing log/event retrieval to the Ground Station.

## Phase 6 — Ground Station (PC app)
- TCP client + minimal CLI: request log data or events for a date/time range from the Central Computer, print/save the response. Since the spec calls for handling several submarine *types* even though this document characterizes one, keep the request/response format tagged with a submarine-type field from the start rather than retrofitting it later.

## Phase 7 — OOP Part: Submarine Fleet Management System (separate console app, no MCU)
- Class design: abstract `Submarine` (serial number, name, mission-assigned flag) → `ResearchSubmarine` (researcher names, research topic) and `CombatSubmarine` (mission description, commander, personnel count, list of co-participating combat submarines, list of past missions, and a `CentralComputer` member by composition — the same class/concept from Phase 5, or a lightweight stand-in if reusing the PC app class directly is impractical).
- `Mission` and `Message` (content + sender reference) as their own classes.
- Menu-driven `main` implementing the 10 listed operations exactly as numbered in the spec.
- Reuse the course-provided generic containers (`genvector.h`, `HashMap.h`, `genqueue.h`, etc. from `c/`) for the fleet list, per-submarine message lists, etc., rather than `std::vector`/`std::map` — check whether your instructor expects the taught libraries specifically here (the "must use the library of data structures" instruction was on the unrelated chat-project PDF, but it's worth confirming rather than assuming it's not intended here).

## Phase 8 — Integration & test checkpoints
Bring up in this order, each with its own quick manual or scripted check: GPIO/LED → UART echo → FreeRTOS tasks running concurrently → ADC readings sane → DHT readings sane → SD card mount + file write/read → object-detection stub firing → full mode-transition scenario (force Warning/Error via Configuration limits and confirm LED/alarm/button/Central-Computer message) → keep-alive cadence over a few minutes → retrieval commands return correct ranged data → pull the watchdog feed and confirm reset+recovery → 8-day SD soak test (or simulated clock) for log rotation.

## Phase 9 — Docs & submission packaging
Protocol spec write-up, one architecture/sequence diagram per module interaction (Figures 2–4 already give the shape), build/flash instructions, and a short demo script walking a grader through Normal→Warning→Error and a management command round-trip.

## Open items to resolve before/while building
- No grading rubric was found anywhere in the folder — worth checking with the instructor/course page so Phase 8/9 target the right things.
- Exact GPIO pin assignments still need mapping against `nucleol476rg pinout.pdf` for: RGB LED (3 pins or 1 PWM+demux), buzzer, button, DHT data line, SPI SD card (CS/MOSI/MISO/SCK), ADC channels for potentiometer and light sensor.
- DHT11 vs DHT22 — affects timing constants in the driver (the datasheet in `c/` should settle this).
- Object-detection hardware still undecided (see Phase 3).
- FreeRTOS heap size and per-task stack sizes need picking once module code size is known; start conservative and profile with `uxTaskGetStackHighWaterMark`.
