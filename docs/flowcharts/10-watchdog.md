# Watchdog (Firmware)

**Status: currently disabled** (`MX_IWDG_Init()` is commented out in
`main.c` - re-enabling it previously caused unwanted resets; root cause
was tracked to `AppState`'s health gate, but re-enabling is pending an
explicit go-ahead - see `docs/SESSION_STATUS.md`).

```mermaid
graph TD
  A[Task_Watchdog runs periodically] --> B{AppState.system_healthy?}
  B -->|Yes| C[HAL_IWDG_Refresh]
  B -->|No, e.g. Init not finished| D[Skip refresh -> IWDG will reset the MCU]
  C --> E[Sleep until next cycle]
  D --> E
```

Source: `tasks/task_watchdog.c`, `app_state.c/.h`, `main.c` (IWDG init
currently commented out).
