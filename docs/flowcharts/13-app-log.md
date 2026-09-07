# Log (Central Computer / PC app)

Persists everything received from the LNC (data + events) to local
files, with the same day-based retention logic ported from the firmware.

```mermaid
graph TD
  A[KEEPALIVE / EVENT / GET_DATA reply arrives] --> B[Format as a log line]
  B --> C[Append to today's date-named file]
  C --> D{Retention window exceeded?}
  D -->|Yes| E["Remove file(s) past the retention cutoff"]
  D -->|No| F[Done]
```

Source: `log_module.h/.cpp`.
