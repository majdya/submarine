# Fleet & Menu (OOP Part)

The 10-operation console menu - pure logic in `Menu`, no I/O - operating
on `Fleet`'s owned submarines. Also the same object graph the web
dashboard drives.

```mermaid
graph TD
  A[Menu operation invoked] --> B{Which operation?}
  B -->|Add| C[Fleet::addSubmarine - reject duplicate serial]
  B -->|Search / Display| D[Fleet::findBySerial]
  B -->|Assign mission| E[Submarine::assignMission - fails if already assigned]
  B -->|Update mission| F[Edit the live Mission object's fields]
  B -->|End mission| G[Move Mission to history, mark available]
  B -->|Associate combat subs| H[CombatSubmarine::addParticipatingSubmarine]
  B -->|Send message| I[Resolve both serials, verify shared mission, deliver]
  B -->|Display messages| J[Return that submarine's received messages]
```

Source: `menu.h/.cpp`, `fleet.h/.cpp`, `submarine.h/.cpp`,
`research_submarine.h/.cpp`, `combat_submarine.h/.cpp`, `mission.h`,
`message.h`.
