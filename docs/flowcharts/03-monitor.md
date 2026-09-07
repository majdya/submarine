# Monitor (Firmware)

Runs on a periodic timer, reads all four sensor values, classifies each
against Configuration's limits, and only bothers Event when the overall
mode actually changes.

```mermaid
graph TD
  A[Timer tick, ~5s] --> B["Read light + temp (ADC), battery (ADC), humidity+temp (DHT11)"]
  B --> C[Classify each value: Normal / Warning / Error]
  C --> D[Overall mode = worst of the four]
  D --> E[Always: push reading to Log]
  D --> F{Mode changed since last cycle?}
  F -->|Yes| G[Post mode-change event to Event]
  F -->|No| H[No event]
```

Source: `tasks/task_monitor.c`, `sensors_adc.c`, `dht11.c`.
