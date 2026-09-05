#include "peripheral_selftest.h"
#include "buttons.h"
#include "dht11.h"
#include "indicators.h"
#include "main.h"
#include "rtc_ds1307.h"
#include "sdcard.h"
#include "sensors_adc.h"
#include <stdio.h>
#include <string.h>

/* This file is the ONLY place in app/ that prints or transmits anything.
   Every function below calls a pure driver function from the other app/
   files and formats its result - it holds no hardware access of its own. */

static void Print(const char *msg) {
  HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
}

static void Test_ADC(void) {
  uint32_t light_raw = 0, temp_raw = 0;
  ADC1_ReadLightTemp(&light_raw, &temp_raw);
  uint32_t battery_raw = ADC2_ReadBatteryRaw();

  uint32_t light_mv = (light_raw * 3300u) / 4095u;
  uint32_t temp_mv = (temp_raw * 3300u) / 4095u;
  uint32_t battery_mv = (battery_raw * 3300u) / 4095u;

  char msg[128];
  int len =
      snprintf(msg, sizeof(msg),
               "[ADC] light=%lu(%lumV) temp=%lu(%lumV) batt=%lu(%lumV)\r\n",
               (unsigned long)light_raw, (unsigned long)light_mv,
               (unsigned long)temp_raw, (unsigned long)temp_mv,
               (unsigned long)battery_raw, (unsigned long)battery_mv);
  if (len > 0) {
    Print(msg);
  }
}

static void Test_LEDs(void) {
  LED1_Toggle();
  LED2_Toggle();
  Print("[LED] LED1 (PC5) & LED2 (PB2) toggled\r\n");
}

static void Test_Buttons(void) {
  GPIO_PinState silence = Button_ReadSilence();
  GPIO_PinState objdet = Button_ReadObjectDetect();
  char msg[96];
  int len = snprintf(msg, sizeof(msg),
                     "[BUTTONS] silence(PA10)=%s objdetect(PB3)=%s\r\n",
                     (silence == GPIO_PIN_RESET) ? "PRESSED" : "released",
                     (objdet == GPIO_PIN_RESET) ? "PRESSED" : "released");
  if (len > 0) {
    Print(msg);
  }
}

static void Test_Buzzer(void) {
  Buzzer_SetDuty(400);
  // RGB_SetDuty(999, 0, 0);
  HAL_Delay(500);
  Buzzer_SetDuty(0);
  // RGB_SetDuty(0, 0, 0);
  Print("[BUZZER] chirp + red pulse commanded (TIM3 CH1-4)\r\n");
}

static void Test_RGB(void) {
  // Buzzer_SetDuty(400);
  RGB_SetDuty(999, 0, 0);
  HAL_Delay(500);
  // Buzzer_SetDuty(0);
  RGB_SetDuty(0, 0, 0);
  Print("[RGB] chirp + red pulse commanded (TIM3 CH1-4)\r\n");
}

static void Test_DHT11(void) {
  DHT11_Reading_t r = {0, 0, 0, 0};
  HAL_StatusTypeDef st = DHT11_Read(&r);
  char msg[128];
  int len;
  if (st == HAL_OK) {
    len = snprintf(msg, sizeof(msg),
                   "[DHT11] humidity=%u.%u%% temp=%u.%uC (checksum OK)\r\n",
                   r.humidity_int, r.humidity_dec, r.temp_int, r.temp_dec);
  } else if (st == HAL_ERROR) {
    len = snprintf(msg, sizeof(msg),
                   "[DHT11] checksum MISMATCH raw=%u,%u,%u,%u\r\n",
                   r.humidity_int, r.humidity_dec, r.temp_int, r.temp_dec);
  } else {
    len = snprintf(msg, sizeof(msg),
                   "[DHT11] TIMEOUT - no response (check wiring/pull-up)\r\n");
  }
  if (len > 0) {
    Print(msg);
  }
}

static void Test_RTC(void) {
  uint8_t seconds = 0xFF;
  HAL_StatusTypeDef st = RTC_ReadSecondsRaw(&seconds);
  char msg[96];
  int len;
  if (st == HAL_OK) {
    len = snprintf(msg, sizeof(msg),
                   "[RTC] DS1307 seconds reg=0x%02X (I2C3 OK)\r\n", seconds);
  } else {
    len = snprintf(msg, sizeof(msg),
                   "[RTC] I2C3 error status=%d - check wiring/address\r\n",
                   (int)st);
  }
  if (len > 0) {
    Print(msg);
  }
}

static void Test_SDCard(void) {
  uint8_t rx = 0;
  HAL_StatusTypeDef st = SD_RawByteExchange(0xFF, &rx);
  char msg[96];
  int len;
  if (st == HAL_OK) {
    len = snprintf(msg, sizeof(msg),
                   "[SD/SPI3] raw byte exchange OK, rx=0x%02X (not a real SD "
                   "init yet)\r\n",
                   rx);
  } else {
    len = snprintf(msg, sizeof(msg), "[SD/SPI3] SPI error status=%d\r\n",
                   (int)st);
  }
  if (len > 0) {
    Print(msg);
  }
}

void RunPeripheralSelfTest(void) {
  Print("--- Peripheral self-test ---\r\n");
  Test_ADC();
  Test_LEDs();
  Test_Buttons();
  // Test_Buzzer();
  Test_RGB();
  Test_DHT11();
  Test_RTC();
  Test_SDCard();
}
