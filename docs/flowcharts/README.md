# Component Flowcharts

Short, one-diagram-per-component reference for how each piece of the
system actually works. Each file: a one-line description, a small
Mermaid flowchart, and the source file(s) it corresponds to. Kept
intentionally brief - see `docs/PROJECT_PLAN.md` for the original design
rationale and `central-computer/README.md` / the firmware's own code
comments for the full detail behind each box.

## Firmware (LNC, STM32 Nucleo-L476RG)

1. [Init](01-init.md)
2. [Configuration](02-configuration.md)
3. [Monitor](03-monitor.md)
4. [Object Detection & Silence Button](04-object-detection.md)
5. [Event](05-event.md)
6. [Log & Retrieval](06-log.md)
7. [Communication](07-communication.md)
8. [Command Handling](08-command-handling.md)
9. [Keep-Alive](09-keepalive.md)
10. [Watchdog](10-watchdog.md) - currently disabled, see `docs/SESSION_STATUS.md`

## Central Computer (PC app, C++17)

11. [Communication](11-app-communication.md)
12. [Management Command](12-app-management-command.md)
13. [Log](13-app-log.md)
14. [Data Collection & Analysis](14-app-data-collection.md)

## OOP Part & Web Dashboard

15. [Fleet & Menu](15-fleet-menu.md)
16. [Web Dashboard](16-web-dashboard.md)
