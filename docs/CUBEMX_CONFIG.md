# CubeMX Peripheral Configuration Checklist

Do this in STM32CubeIDE's CubeMX pinout/config editor before writing any application code — everything in `firmware/Modules/` and `Drivers_App/` depends on the HAL init calls this step generates. Pin references match `hardware.md` §5 (as corrected 2026-09-03).

## 1. New project & board

New STM32 Project → Board Selector → **NUCLEO-L476RG**. When CubeMX offers to initialize all peripherals with their default mode: say **No**. The default setup claims `PA5` as LD2 GPIO and leaves `PC13` as the user button, which is fine — but don't let it auto-claim anything else, since we're about to hand most pins to the shields instead.

**Revalidated 2026-09-04 against the actual generated `.ioc`:** the project was created from a bare MCU (`board=custom`) rather than the NUCLEO-L476RG board file. This explains several of the mismatches below (RTC and TIM2 enabled with no reason, PB3 still shown reserved as JTDO, no LD2/user-button defaults). Not worth redoing the project over — just correct the items flagged inline.

## 2. Clock configuration (Clock Configuration tab)

No external crystal on this board by default, so drive SYSCLK from HSI16 through the PLL — CubeMX's default (MSI @ 4MHz feeding SYSCLK directly, PLL configured but unused) is too slow for FreeRTOS + SPI + ADC + PWM all running together and must be switched over explicitly:

1. PLL source mux → **HSI** (16 MHz) — usually already the default.
2. PLLM `/1`, PLLN `x10`, PLLR `/2` → gives VCO 160MHz / 2 = **80 MHz**.
3. **System Clock Mux → PLLCLK** (not MSI — this is the step that's easy to leave on the default and not notice, since the PLL block can be fully configured on paper while SYSCLK is still quietly running off MSI).
4. Click "Resolve Clock Issues" if CubeMX flags anything — it auto-fills APB1/APB2 prescalers and adjusts Flash wait-states/voltage scaling for you.

Verified 2026-09-03 against an actual screenshot of this step: it's easy to end up with the PLL correctly configured but SYSCLK still reading 4 MHz because the System Clock Mux itself was never switched off MSI — double check that specific selector, not just the PLL multiplier boxes. Note the final APB1 timer clock once this is done (should read 80MHz), since it determines your PWM frequency range once TIM3's prescaler/period are chosen in step 5.

## 3. GPIO pins (Pinout & Configuration → find each pin on the chip diagram, set its mode)

| Pin | Mode | Config |
| :-- | :-- | :-- |
| `PA10` (D2, Button 1) | GPIO_Input | Pull-up; enable EXTI10 line, falling edge (alarm-silence button) |
| `PB3` (D3, Button 2 / object-detect) | GPIO_Input | Pull-up; enable EXTI3 line, falling edge |
| `PB6` (D10, SD CS) | GPIO_Output | Push-pull, no pull, initial output level **High** |
| `PB5` (D4, DHT11 data) | GPIO_Output | Open-drain, pull-up — single-wire bit-banged protocol; driver will flip direction at runtime |
| `PC5` (Morpho, LED1) | GPIO_Output | Push-pull |
| `PB2` (Morpho, LED2) | GPIO_Output | Push-pull |

Leave `PC13` (Nucleo's own onboard button) and `PA5`/LD2 alone for now — LD2 will get reclaimed as SPI1_SCK in step 4, which is expected (see hardware.md's note about it flickering during SD writes).

**Revalidated 2026-09-04, both are currently wrong in the generated `.ioc`:**
- `PA10`/`PB3` are both plain polled `GPIO_Input`, not EXTI — no interrupt line enabled on either yet. Fine for now (bring-up used them unread), but needs fixing before the Event module relies on them.
- `PB3` still shows as `PB3 (JTDO-TRACESWO)` in the `.ioc`, meaning **SYS → Debug hasn't been set to Serial Wire yet** — go to System Core → SYS → Debug and pick "Serial Wire", or this pin risks staying claimed by the debug port instead of behaving as a clean GPIO.
- `PB5` is currently plain push-pull `GPIO_Output`, not open-drain+pull-up as specified — fix before writing the DHT11 driver.

## 4. SPI1 — SD card (Connectivity → SPI1 → Mode: Full-Duplex Master)

Pins auto-route once SPI1 is enabled and you confirm `PA5`/`PA6`/`PA7` in the pinout view: SCK=`PA5` (D13), MISO=`PA6` (D12), MOSI=`PA7` (D11). CS (`PB6`) is handled as a plain GPIO from step 3, not SPI1_NSS — leave hardware NSS disabled, we're doing software chip-select.

Parameter settings: Data size 8 bits, first bit MSB, prescaler set for a safe initial SD init speed (start ~400kHz for the card's init sequence, you'll switch to a faster clock in the FatFs driver after init — this is standard SD-over-SPI practice, not a CubeMX setting to worry about now beyond picking a workable starting prescaler).

**Revalidated 2026-09-04:** the `.ioc` currently has **TIM2** enabled (base timer, Period=39999, internal clock, no PWM channels) instead of TIM3 below — TIM2 was never part of this plan. Still needs a decision: what TIM2 is for (if anything) and whether to remove it; TIM3 with its 4 PWM channels still needs to be added from scratch.

## 5. TIM3 — buzzer + RGB LED (Timers → TIM3)

Enable Channels 1–4, all as PWM Generation:
- CH1 → `PB4` (D5, buzzer)
- CH2 → `PC7` (D9, RGB Red)
- CH3 → `PC8` (Morpho, RGB Green)
- CH4 → `PB1` (Morpho, RGB Blue)

Set Prescaler + Period (ARR) so the PWM frequency suits an LED (a few hundred Hz to a few kHz, imperceptible flicker) — the buzzer's actual tone will be driven by reprogramming CH1's frequency at runtime for different notes/alarm patterns, so don't worry about picking "the buzzer frequency" here, just a sane default.

## 6. ADC — potentiometer, LDR, LM35 (Analog)

**Revalidated 2026-09-04:** these three pins are NOT all on ADC1 — CubeMX's actual silicon mapping splits them: `PA0` (Battery) → **ADC1_IN5**, but `PA1` (Light) → **ADC2_IN6** and `PA4` (Temperature) → **ADC2_IN9**. So enable **both ADC1 and ADC2** in the Analog section, not just ADC1 — PA0's channel under ADC1, PA1/PA4's channels under ADC2. (The Humidity/PB0 pin is also on ADC2, IN15 — still parked per the earlier decision to defer that mismatch.)

Simplest config for now: independent mode on each ADC, no continuous conversion, no DMA — the Monitor task will trigger one-shot conversions per channel every 5s via `HAL_ADC_Start`/`HAL_ADC_PollForConversion` (called once per ADC instance). DMA + scan mode is a fine later optimization, not needed to get moving.

## 7. I2C3 — DS1307 RTC (Connectivity → I2C3)

Enable I2C3, confirm pins land on `PC0` (SCL, A5) and `PC1` (SDA, A4) in the pinout view — **not I2C1**, which is a different peripheral on different pins (`PB8`/`PB9`). Standard mode, 100kHz is plenty for a DS1307. This is a plain I2C peripheral talking to an external RTC chip — CubeMX's "RTC" peripheral (the STM32's *internal* RTC) is not used in this design and should stay disabled.

**Revalidated 2026-09-04: the internal RTC is currently enabled in the `.ioc` (System Core → RTC → Activate Clock Source is ON) — this contradicts the above and still needs a decision from you: disable it (System Core → RTC → uncheck Activate), or confirm you want to drop the DS1307 and use the internal RTC instead.** Not resolved yet.

## 8. USART2 — Central Computer link (should already be enabled by the board selector)

Confirm `PA2`(TX)/`PA3`(RX), Asynchronous mode, and that it's routed through the ST-LINK's virtual COM port (this is the Nucleo-64's default — CubeMX's board selector usually sets this up automatically). Baud rate: pick something both firmware and the PC-side Central Computer app will agree on later (115200 is a safe default).

## 9. IWDG — watchdog (System Core → IWDG)

Enable IWDG. Set the prescaler/reload so the timeout comfortably exceeds your longest expected gap between Watchdog-task refreshes (with margin) — exact numbers can wait until the Watchdog task's refresh period is decided in Phase 2, but enabling the peripheral now means the timeout is picked deliberately rather than left at a CubeMX default nobody looked at.

## 10. FreeRTOS middleware (Middleware → FREERTOS)

Interface: CMSIS_V2. Leave default task list empty for now (each module's task gets created in application code, not generated here) — just confirm heap size (`configTOTAL_HEAP_SIZE`, e.g. start at 8–15 KB and adjust once real module stacks exist) and tick rate (1000 Hz default is fine).

## 11. Project Manager tab

- Toolchain: STM32CubeIDE.
- **C++ note:** CubeMX generates `main.c` and all HAL init code as C. Per the project's mixed C/C++ + `extern "C"` convention, don't fight this — leave CubeMX's generated files as `.c`, and add your own `.cpp` files under `Modules/`/`Drivers_App/` alongside them. Any C++ file that needs a HAL/CMSIS header wraps the include in `extern "C" { #include "..." }` (or rely on the CubeMX-generated headers, which already guard themselves with `#ifdef __cplusplus extern "C" {`). Task entry functions passed to `osThreadNew`/`xTaskCreate` must be plain C-linkage functions — write them as small trampolines that call into your C++ module objects.
- Code generation: "Generate peripheral initialization as a pair of .c/.h files per peripheral" — on, keeps things organized as the project grows.

## After generating code

Build immediately with zero application logic added — confirm it compiles and flashes clean before writing a single module. That's the real "peripherals configured" checkpoint; everything in Phase 1 onward builds on top of a project that already boots.

### Clock config — confirmed working (2026-09-03)

Verified via screenshot: SYSCLK/HCLK/APB1/APB2/USART2/I2C/ADC all read 80MHz. Actual path used was **MSI(4MHz) → PLLM/1 → PLLN×40 → PLLR/2 = 80MHz** (PLL source = MSI, not HSI as originally suggested above) — a different but equally valid route to the same 80MHz target; ST's own reference configs commonly use exactly this MSI×40 recipe for the L476. No changes needed, this step is done.
