#include "uart_rx.h"
#include "cmsis_os2.h"
#include "main.h"

/* Power-of-2 so the wrap (& (SIZE-1)) is a cheap mask instead of a
   modulo - 64 bytes is generous for command frames arriving well below
   this UART's 115200 baud ceiling relative to how often a task actually
   drains it. */
#define UART_RX_BUF_SIZE 64u

static uint8_t s_ring[UART_RX_BUF_SIZE];
/* Single producer (the RX ISR) / single consumer (one task calls
   UartRx_Pop) - plain volatile indices are safe for that without extra
   locking on this single-core Cortex-M4, and osSemaphoreRelease (ISR) /
   osSemaphoreAcquire (task) already form the happens-before edge that
   makes the consumer's view of s_ring consistent. */
static volatile uint32_t s_head = 0; /* next write index - ISR only */
static volatile uint32_t s_tail = 0; /* next read index - task only */

/* HAL_UART_Receive_IT's destination for the next byte - must be static
   (not a local/stack variable), since the HAL driver holds a pointer to
   it across the entire wait for that byte to arrive. */
static uint8_t s_rx_scratch;

static osSemaphoreId_t s_byte_sem;
static const osSemaphoreAttr_t s_byte_sem_attr = {
    .name = "UartRxByteSem",
};

void UartRx_Init(void) {
  /* Max count == buffer size: one token per byte actually sitting in the
     ring buffer, so a task blocked in UartRx_Pop wakes exactly once per
     byte, never spuriously and never more than what's really there. */
  s_byte_sem = osSemaphoreNew(UART_RX_BUF_SIZE, 0, &s_byte_sem_attr);
}

void UartRx_Start(void) { HAL_UART_Receive_IT(&huart2, &s_rx_scratch, 1); }

void UartRx_ByteReceivedFromISR(void) {
  uint32_t next_head = (s_head + 1u) & (UART_RX_BUF_SIZE - 1u);
  if (next_head != s_tail) {
    s_ring[s_head] = s_rx_scratch;
    s_head = next_head;
    osSemaphoreRelease(s_byte_sem); /* CMSIS-RTOS2: valid from ISR context */
  }
  /* else: consumer has fallen a full buffer behind - drop this byte
     rather than overwrite an unread one, but still re-arm below so a
     momentary stall never permanently wedges reception. */

  HAL_UART_Receive_IT(&huart2, &s_rx_scratch, 1);
}

uint8_t UartRx_Pop(uint8_t *out, uint32_t timeout_ms) {
  if (!out || osSemaphoreAcquire(s_byte_sem, timeout_ms) != osOK) {
    return 0;
  }
  if (s_tail == s_head) {
    return 0; /* defensive only - the semaphore count should already
                 guarantee a byte is waiting whenever Acquire succeeds */
  }
  *out = s_ring[s_tail];
  s_tail = (s_tail + 1u) & (UART_RX_BUF_SIZE - 1u);
  return 1;
}
