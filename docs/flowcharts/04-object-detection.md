# Object Detection & Silence Button (Firmware)

Both are EXTI (interrupt) driven, not a polling task - the ISR posts
straight to the Event queue for the lowest possible latency.

```mermaid
graph TD
  A[GPIO EXTI interrupt fires] --> B{Which pin?}
  B -->|Object-detect pin| C{Debounced state}
  C -->|Object now present| D[Post EVENT_OBJECT_DETECTED]
  C -->|Object now absent| E[Post EVENT_OBJECT_CLEARED]
  B -->|Silence button pin| F[Post EVENT_SILENCE_PRESSED]
  D --> G[Event task handles it]
  E --> G
  F --> G
```

Source: `stm32l4xx_it.c` (EXTI callback), `buttons.c/.h`.
