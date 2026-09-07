# Configuration (Firmware)

Owns the Normal/Warning limit table (temp, humidity, light, battery) that
Monitor classifies readings against. Backed by the last Flash page so
limits survive a reset.

```mermaid
graph TD
  A[Boot] --> B{Valid config in Flash?}
  B -->|Yes, checksum OK| C[Load stored limits]
  B -->|No / first boot| D[Load placeholder defaults]
  C --> E[AppConfig ready]
  D --> E
  E --> F[Monitor reads limits every cycle]
  G[SET_LIMITS command arrives] --> H[Update one PARAM's Normal/Warning bounds]
  H --> I[Persist to Flash]
  I --> E
```

Source: `app_config.c/.h`, `flash_config_store.c/.h`.
