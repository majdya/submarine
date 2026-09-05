#ifndef APP_PERIPHERAL_SELFTEST_H
#define APP_PERIPHERAL_SELFTEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* Runs Test_ADC / Test_LEDs / Test_Buttons / Test_BuzzerRGB / Test_DHT11 /
   Test_RTC / Test_SDCard in sequence, one call = one pass over every
   configured peripheral. Call DWT_Init() once before the first call
   (needed by Test_DHT11's timing). */
void RunPeripheralSelfTest(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_PERIPHERAL_SELFTEST_H */
