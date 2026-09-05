#include "sdcard.h"
#include "main.h"

/* NOTE: hspi3's BaudRatePrescaler is currently DIV2 (~40MHz off an 80MHz
   APB1 clock - APB1 and APB2 are both undivided from the 80MHz SYSCLK on
   this board). A real SD card init sequence (CMD0/CMD8/ACMD41) needs
   <=400kHz until the card leaves idle state - the prescaler will need to
   change (and be raised back up afterward) before doing real SD init.

   SPI3 (PC10/11/12) + manual CS on PD2 was chosen deliberately instead of
   SPI1 (PA5/6/7 + PB6): those SPI1 pins are the Nucleo Arduino header's
   D13/D12/D11/D10, which are physically shared with the sensor shield's
   RGB LED. PC10-12 and PD2 are Morpho-only pins with no such conflict. */
HAL_StatusTypeDef SD_RawByteExchange(uint8_t tx, uint8_t *rx) {
  uint8_t rx_byte = 0;
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET); /* CS select */
  HAL_StatusTypeDef st =
      HAL_SPI_TransmitReceive(&hspi3, &tx, &rx_byte, 1, 50);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET); /* CS deselect */
  if (rx) {
    *rx = rx_byte;
  }
  return st;
}
