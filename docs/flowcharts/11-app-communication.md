# Communication (Central Computer / PC app)

Mirrors the firmware's Communication module on the PC side - a reader
thread over a real (or loopback) transport, decoding the same TLV frames.

```mermaid
graph TD
  A[Reader thread: transport.readSome] --> B[Feed bytes to FrameDecoder]
  B --> C{Complete frame?}
  C -->|No| A
  C -->|Yes, reply to a pending command| D[Deliver to the waiting caller]
  C -->|Yes, unsolicited KEEPALIVE/EVENT| E[Update LiveSnapshot + notify Log/DataCollection]
  F[ManagementCommand::sendX] --> G[Encode TLV frame, transport.write]
  G --> H[Block waiting for reply, with timeout]
```

Source: `comm_link.h/.cpp`, `protocol_bridge.h/.cpp`,
`transport/serial_transport.h/.cpp`, `transport/loopback_transport.h`.
