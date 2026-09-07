# Management Command (Central Computer / PC app)

The outward-facing API a caller (console `Menu`, or the dashboard) uses
to actually talk to the LNC - one method per command, all going through
Communication.

```mermaid
graph TD
  A[Caller: e.g. setLimits, getTime, getData] --> B[Build TLV command frame]
  B --> C[CommLink sends it, waits for reply]
  C --> D{Reply / ACK received in time?}
  D -->|Yes| E[Return parsed result to caller]
  D -->|No, timeout| F[Return failure - hardware not responding]
```

Source: `management_command.h/.cpp`.
