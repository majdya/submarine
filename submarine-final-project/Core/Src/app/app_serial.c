#include "app_serial.h"
#include "cmsis_os2.h"
#include "main.h"
#include <string.h>

static osMutexId_t s_uart_mutex;
static const osMutexAttr_t s_uart_mutex_attr = {
    .name = "UartTxMutex",
};

void Serial_Init(void) { s_uart_mutex = osMutexNew(&s_uart_mutex_attr); }

void Serial_Print(const char *msg) {
  if (s_uart_mutex != NULL) {
    osMutexAcquire(s_uart_mutex, osWaitForever);
  }
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  if (s_uart_mutex != NULL) {
    osMutexRelease(s_uart_mutex);
  }
}
