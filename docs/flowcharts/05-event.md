# Event (Firmware)

The single consumer of the event queue - fed by Monitor (mode changes),
the EXTI ISR (object/silence), Configuration, and Init. Owns the RGB LED
and buzzer exclusively.

```mermaid
graph TD
  A[Event arrives on queue] --> B["Timestamp it (RTC)"]
  B --> C{Event type}
  C -->|Mode change| D[Set RGB: green/yellow/red]
  C -->|Object detected| E[Trigger alarm buzzer]
  C -->|Silence pressed| F[Turn buzzer off]
  C -->|Other| G[No indicator change]
  D --> H[Write event line to SD log]
  E --> H
  F --> H
  G --> H
  H --> I{Event type is comms-relevant?}
  I -->|Yes| J[Enqueue to Communication outbound queue]
  I -->|No| K[Done]
```

Source: `tasks/task_event.c`, `indicators.c/.h`.
