#ifndef APP_UART_RX_H
#define APP_UART_RX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Interrupt-driven single-byte UART RX into a small ring buffer -
   nothing in this project received anything over UART before this.
   Driver-level only: knows about bytes, not about comm_frame.h framing
   or command meaning (that's task_comm_rx.c/app_command.c). */

/* Must be called once from MX_FREERTOS_Init's RTOS_SEMAPHORES section
   (creates the byte-ready counting semaphore), before UartRx_Start() or
   any task calls UartRx_Pop(). */
void UartRx_Init(void);

/* Arms the first single-byte HAL_UART_Receive_IT on huart2. Call once,
   after MX_USART2_UART_Init() has run and USART2's NVIC interrupt is
   enabled, and after UartRx_Init(). */
void UartRx_Start(void);

/* Call this from HAL_UART_RxCpltCallback (ISR context) when huart is
   huart2 - pushes the byte HAL just placed in the driver's one-byte
   scratch buffer into the ring buffer, signals the byte-ready semaphore,
   and re-arms the next single-byte receive unconditionally (even if the
   ring buffer was full and this byte had to be dropped) so reception
   never wedges. */
void UartRx_ByteReceivedFromISR(void);

/* Task context: blocks up to timeout_ms (or osWaitForever, per
   cmsis_os2.h) for the next received byte. Returns 1 and fills *out if a
   byte was popped, 0 on timeout. */
uint8_t UartRx_Pop(uint8_t *out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_UART_RX_H */
