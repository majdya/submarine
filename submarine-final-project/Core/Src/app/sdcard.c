#include "sdcard.h"
#include "main.h"

/* NOTE: hspi1's BaudRatePrescaler is currently DIV2 (~40MHz off an 80MHz
   APB2 clock). A real SD card init sequence (CMD0/CMD8/ACMD41) needs
   <=400kHz until the card leaves idle state - the prescaler will need to
   change (and be raised back up afterward) before doing real SD init. */
HAL_StatusTypeDef SD_RawByteExchange(uint8_t tx, uint8_t *rx) {
  uint8_t rx_byte = 0;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); /* CS select */
  HAL_StatusTypeDef st =
      HAL_SPI_TransmitReceive(&hspi1, &tx, &rx_byte, 1, 50);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); /* CS deselect */
  if (rx) {
    *rx = rx_byte;
  }
  return st;
}
