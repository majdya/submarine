# LNC Firmware Test Tracking

**Project:** Submarine Monitoring System — Local Node Controller (LNC) Firmware
**Spec ref:** SW-FD-LNC-001 (`final project.pdf`)
**Branch:** `spec-alignment-modes-comm-flash-log`
**Commit:** `c9e0958`
**Last updated:** 2026-09-05

## Confirmed

- [x] **object_detect_toggle** — Button 2 (PB3) alternates EVENT_OBJECT_DETECTED / EVENT_OBJECT_CLEARED
      Serial log 21:02:34-21:03:09, and again in `lnc_test_tool.py listen` output at 21:43-21:43: clean detected/cleared alternation
- [x] **silence_button** — Silence button posts EVENT_SILENCE_PRESSED
      Fires correctly in serial log and in decoded `listen` output
- [x] **log_file_creation** — Date-named log file (YYYYMMDD.TXT) created and written
      `20260905.TXT` written with correct timestamped event lines
- [x] **rtc_battery_backed** — RTC retains time across reset (battery-backed)
      Boot log: "RTC already running (battery-backed time trusted)"
- [x] **mode_reporting** — Current mode reported correctly in event/keepalive frames
      All frames show mode=NORMAL, consistent with sensor readings. Only NORMAL observed so far - see mode_transitions below
- [x] **keepalive_cadence** — Task_KeepAlive posts a TLV frame every 6s
      `lnc_test_tool.py listen`: embedded RTC seconds 10,16,22,28,34,40,46,52 - exact 6s cadence, zero drift
- [x] **comm_frame_realworld** — Outer STX/TYPE/LEN/PAYLOAD/CHECKSUM/ETX framing decodes cleanly on real UART traffic
      ~17 frames decoded with zero resyncs/corruption in the `listen` capture - validates uart_rx.c + comm_frame.c + Task_CommRx end to end
- [x] **command_set_time** — MSG_TYPE_CMD_SET_TIME updates RTC
      Confirmed working via `lnc_test_tool.py set-time`
- [x] **command_get_time** — MSG_TYPE_CMD_GET_TIME replies with current RTC time
      Confirmed working via `lnc_test_tool.py get-time`
- [x] **command_set_limits** — MSG_TYPE_CMD_SET_LIMITS updates a threshold and ACKs
      Confirmed working via `lnc_test_tool.py set-limits`
- [x] **command_get_data** — MSG_TYPE_CMD_GET_DATA streams matching date-named log files
      Confirmed working via `lnc_test_tool.py get-data`. Known limitation still applies: currently returns same content as GET_EVENTS (no separate continuous data stream yet)
- [x] **command_get_events** — MSG_TYPE_CMD_GET_EVENTS streams matching date-named log files
      Confirmed working via `lnc_test_tool.py get-events`

## Needs Testing

- [ ] **mode_transitions** — Mode changes to WARNING/ERROR when a sensor goes out of range; EVENT_MODE_CHANGED fires only on actual transition
      Force a sensor out of range, confirm transition + no duplicate posts while staying in the new mode
- [ ] **rgb_mode_color** — RGB LED shows green/yellow/red matching current mode
      Visual check on the board, needs mode_transitions test to trigger
- [ ] **rgb_alarm_interaction** — RGB/alarm "last write wins" behavior when mode-change and object-detect happen close together
      Known simplification, not a bug - check it behaves sanely, doesn't corrupt PWM state
- [ ] **alarm_buzzer** — Buzzer sounds on object_detected, silences on silence_pressed
      Log only proves the event fired, not the hardware side effect
- [ ] **flash_config_persistence** — AppConfig_t (thresholds) persists across power cycle
      SET_LIMITS command itself confirmed - still need a power-cycle after setting a non-default limit to confirm it reloads from flash instead of defaults
- [ ] **log_retention_7day** — Log files older than 7 days are deleted (EnforceRetention)
      Needs RTC date advanced 7+ days (via `set-time`), or real multi-day runtime

## Blocked

- [ ] **iwdg_watchdog** — Independent watchdog re-enabled and retested on real hardware
      `MX_IWDG_Init()` still commented out in main.c pending explicit go-ahead

## Known Issues

- [ ] **debug_binary_uart_shared** (cosmetic/usability) — Human-readable debug/log prints and binary TLV protocol frames (keep-alive, events) share the same UART, making a plain terminal show mixed readable text + binary garbage.
      Options: route binary protocol to a second UART and keep debug prints on this one; route debug prints to a separate UART (e.g. ST-LINK VCP) and keep this UART binary-only; or leave as-is and use a proper client (hex tool or PC-side script) to talk the real protocol.
- [ ] **sensor_reading_anomaly** (watch) — `tempADC=0` and `batt=4095` constant across every observed frame.
      4095 is ADC full-scale (12-bit) - could indicate a floating/disconnected pin on the test bench rather than a firmware bug. Worth confirming wiring before assuming a code issue.
- [ ] **possible_silence_double_fire** (watch, unconfirmed) — Two EVENT_SILENCE_PRESSED frames arrived ~1s apart (21:43:06/07) carrying the same embedded RTC second (40) in one `listen` capture.
      Could be an intentional double press during manual testing, or a missing debounce on that interrupt line - needs a deliberate single, isolated press to confirm which.
