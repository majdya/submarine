# Log & Retrieval (Firmware)

Writes both periodic Monitor readings and Event log lines to date-named
files on the SD card, with 7-day rolling retention; also serves
GET_DATA/GET_EVENTS retrieval requests from the same files.

```mermaid
graph TD
  A[Log queue: Monitor reading or Event line] --> B[Open/append today's YYYYMMDD.TXT on SD]
  B --> C{New day since last write?}
  C -->|Yes| D[Delete file older than 7 days, if any]
  C -->|No| E[Continue]
  D --> E
  E --> F[Write formatted line]

  G[GET_DATA / GET_EVENTS request] --> H[Look up requested date's file on SD]
  H --> I[Stream matching lines back to Communication]
```

Source: `tasks/task_log.c`, `app_log.c/.h`, `log_query.c/.h`, `sdcard.c/.h`.
