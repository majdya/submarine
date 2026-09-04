# CubeMX Checklist — fill in "Your Notes" as you go

Work through this at your own pace. Fill in the **Your Notes** column for each row (e.g. "done", "done - picked X instead", "skipped, see reason", "stuck - dropdown doesn't show this"). When you're ready for feedback, tell me and I'll read the whole file and give you one consolidated response instead of commenting row by row.

| Item | Status (as of 2026-09-04) | Action needed | Your Notes |
|---|---|---|---|
| Board/MCU selection | Bare MCU (`board=custom`), not NUCLEO-L476RG board file | Not worth redoing, informational only | |
| Clock config (HSI→PLL→80MHz) | Done, verified working | None | |
| PA2/PA3 — USART2 TX/RX | Done | None | |
| USART2 (115200, 8N1, VCP) | Done | None | |
| IWDG | Enabled | None (refresh already added in code) | |
| PC5 (LED1) | GPIO Output | None | |
| PB2 (LED2) | GPIO Output | None | |
| PB6 (SD CS) | GPIO Output, push-pull, High | None | |
| PA10 (Button 1 / alarm-silence) | Plain input, label typo "SILENICE" | Set to GPIO_EXTI10, falling edge, pull-up; fix label spelling to SILENCE | |
| PB3 (Button 2 / object-detect) | Plain input, still shows JTDO | Set to GPIO_EXTI3, falling edge, pull-up | |
| PB5 (DHT11 data) | Push-pull output | Change to Open Drain, pull-up | |
| SYS -> Debug mode | Not set (default) | Set to Serial Wire | |
| RTC (internal) | Removed | None | |
| TIM2 | Removed | None | |
| ADC1 | Enabled | None | |
| ADC2 | Enabled | None | |
| Battery ADC (PA0/IN5) | In conversion list, on ADC2 | Restore "Battery ADC" label | |
| Light ADC (PA1/IN6) | In conversion list, on ADC1 | Restore "Light ADC" label | |
| Temperature ADC (PA4/IN9) | Reserved, NOT in conversion list yet | Add to a channel rank list; restore label | |
| Humidity ADC (PB0/IN15) | Parked (known conceptual mismatch) | Deferred by your choice, no action | |
| SPI1 (SD card, PA5/6/7) | Not configured | Enable, Full-Duplex Master | |
| TIM3 (buzzer + RGB, 4ch PWM) | Not configured | Enable CH1-CH4 as PWM | |
| I2C3 (DS1307, PC0/PC1) | Not configured | Enable, confirm pins, 100kHz | |
| FreeRTOS | Not added | Middleware -> CMSIS_V2 | |
| Project Manager / toolchain | CMake, C generation confirmed working | None | |
