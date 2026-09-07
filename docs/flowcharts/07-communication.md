# Communication (Firmware)

Two tasks either side of one TLV-framed UART link: CommRx parses inbound
bytes and routes them; Comm drains a priority-ordered outbound queue.

```mermaid
graph TD
  A[UART RX interrupt] --> B[Push byte to ring buffer]
  B --> C[Task_CommRx: pop + TLV-decode a frame]
  C --> D{Frame type}
  D -->|Management command| E[Hand off to Command Handling]
  D -->|Retrieval instruction| F[Hand off to Log & Retrieval]

  G[Task_Comm: outbound loop] --> H{Any message queued?}
  H -->|Keep-alive pending| I[Send it - highest priority]
  H -->|Event pending| J[Send it - medium priority]
  H -->|Data report pending| K[Send it - lowest priority]
  I --> H
  J --> H
  K --> H
```

Source: `tasks/task_comm.c`, `tasks/task_comm_rx.c`, `app_comm.c/.h`,
`uart_rx.c/.h`, and the shared `tlv.c/comm_frame.c` (also compiled
unmodified into the PC app - see `central-computer/`).
