#include "dht11.h"
#include "dwt_delay.h"
#include "main.h"

HAL_StatusTypeDef DHT11_Read(DHT11_Reading_t *out) {
  uint8_t data[5] = {0, 0, 0, 0, 0};
  uint32_t timeout;

  /* Start signal: MCU pulls the open-drain line low >=18ms, then releases
     it (pull-up brings it high) and waits for the sensor's response. */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_RESET);
  HAL_Delay(19);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
  DWT_Delay_us(30);

  timeout = 0;
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_SET) {
    if (++timeout > 1000) {
      goto dht_timeout;
    }
    DWT_Delay_us(1);
  }
  timeout = 0;
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
    if (++timeout > 200) {
      goto dht_timeout;
    }
    DWT_Delay_us(1);
  }
  timeout = 0;
  while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_SET) {
    if (++timeout > 200) {
      goto dht_timeout;
    }
    DWT_Delay_us(1);
  }

  /* 40 data bits: each is a ~50us low phase, then a high phase whose length
     encodes the bit (~26-28us = 0, ~70us = 1). */
  for (int byte_i = 0; byte_i < 5; byte_i++) {
    for (int bit_i = 0; bit_i < 8; bit_i++) {
      timeout = 0;
      while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_RESET) {
        if (++timeout > 200) {
          goto dht_timeout;
        }
        DWT_Delay_us(1);
      }
      DWT_Delay_us(40); /* sample partway into the high phase */
      data[byte_i] <<= 1;
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_SET) {
        data[byte_i] |= 1U;
        timeout = 0;
        while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5) == GPIO_PIN_SET) {
          if (++timeout > 200) {
            goto dht_timeout;
          }
          DWT_Delay_us(1);
        }
      }
    }
  }

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); /* idle high */
  {
    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (out) {
      out->humidity_int = data[0];
      out->humidity_dec = data[1];
      out->temp_int = data[2];
      out->temp_dec = data[3];
    }
    return (checksum == data[4]) ? HAL_OK : HAL_ERROR;
  }

dht_timeout:
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET); /* release line */
  return HAL_TIMEOUT;
}
