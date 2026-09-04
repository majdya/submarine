# Central Computer / Submarine Fleet Management (OOP)

Per the final project spec (SW-FD-LNC-001), this is a single C++ program covering two tied-together parts:

1. **Central Computer** (spec section 3) — talks to the LNC end unit (the STM32 firmware in
   `../submarine-final-project/`) over the UART/VCP link. Modules: Communication (LNC-facing),
   Management Command, Log, Data Collection & Analysis.
2. **OOP Part — Submarine Fleet Management System** — the spec explicitly treats the Central
   Computer as an object owned by each combat submarine, so this extends part 1 with the
   fleet/mission/messaging class hierarchy (Submarine base class, ResearchSubmarine /
   CombatSubmarine subclasses, missions, inter-submarine messaging) plus the console menu.

Not started yet — placeholder until we begin this phase.
