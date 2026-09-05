#ifndef APP_SERIAL_H
#define APP_SERIAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Must be called once from MX_FREERTOS_Init's RTOS_MUTEX section, before
   any task that prints starts. */
void Serial_Init(void);

/* Mutex-protected HAL_UART_Transmit on huart2. Every task that prints
   (Log, Communication, Init, peripheral_selftest) goes through this -
   with multiple tasks now running concurrently, two unsynchronized
   HAL_UART_Transmit calls on the same handle would interleave/corrupt the
   output on the wire. Safe to call before Serial_Init() runs (falls back
   to an unguarded transmit) so early boot prints still work. */
void Serial_Print(const char *msg);

/* Same mutex-protected HAL_UART_Transmit as Serial_Print, but for raw
   binary data (len bytes, may contain embedded 0x00) rather than a
   NUL-terminated C string - used for framed comm_frame.h messages, which
   Serial_Print would silently truncate at the first 0x00 byte. */
void Serial_Write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_SERIAL_H */
