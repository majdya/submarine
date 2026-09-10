# STM32 CubeMX Configuration & Setup

Complete checklist for configuring STM32CubeIDE's CubeMX pinout/peripheral editor before writing application code.

**Last verified:** 2026-09-09 (consolidated from CUBEMX_CONFIG.md + CUBEMX_CHECKLIST.md)

---

## 1. New Project & Board Selection

New STM32 Project → Board Selector → **NUCLEO-L476RG**. When CubeMX offers to initialize all peripherals with their default mode: say **No**. The default setup claims `PA5` as LD2 GPIO and leaves `PC13` as the user button, which is fine — but don't let it auto-claim anything else, since we're about to hand most pins to the shields instead.

> **Note:** The current project was created from a bare MCU (`board=custom`) rather than the NUCLEO-L476RG board file. This explains some historical mismatches. Not worth redoing — just apply the corrections below.

---

## 2. Clock Configuration (Clock Configuration tab)

**Status:** ✅ DONE (verified 2026-09-03, screenshot confirmed)

No external crystal on this board by default, so drive SYSCLK from HSI16 through the PLL. The default (MSI @ 4MHz feeding SYSCLK directly, PLL configured but unused) is too slow for FreeRTOS + SPI + ADC + PWM all running together and **must** be switched over explicitly:

1. PLL source mux → **HSI** (16 MHz) — usually already the default.
2. PLLM `/1`, PLLN `x10`, PLLR `/2` → gives VCO 160MHz / 2 = **80 MHz**.
   - *OR* (equally valid): MSI(4MHz) → PLLM/1 → PLLN×40 → PLLR/2 = 80MHz (ST's reference recipe)
3. **System Clock Mux → PLLCLK** (not MSI — this is the critical step that's easy to miss!)
4. Click "Resolve Clock Issues" if CubeMX flags anything — it auto-fills APB1/APB2 prescalers and adjusts Flash wait-states/voltage scaling.

**Verified result:** SYSCLK/HCLK/APB1/APB2/USART2/I2C/ADC all read 80MHz. ✅

---

## 3. GPIO Pins (Pinout & Configuration)

**Action:** Find each pin on the chip diagram and set its mode as listed below.

| Pin | Mode | Config | Purpose | Status |
|:---|:---|:---|:---|:---|
| `PA10` (D2, Button 1) | GPIO_EXTI10 | Pull-up, falling edge | Alarm-silence button | ⚠️ Currently plain GPIO; fix to EXTI10 |
| `PB3` (D3, Button 2) | GPIO_EXTI3 | Pull-up, falling edge | Object-detect button | ⚠️ Currently plain GPIO; still shows JTDO; fix SYS→Debug to Serial Wire first |
| `PB6` (D10, SD CS) | GPIO_Output | Push-pull, no pull, **High** | SD card chip select | ✅ |
| `PB5` (D4, DHT11 data) | GPIO_Output | Open-drain, pull-up | Single-wire DHT protocol | ⚠️ Currently push-pull; change to open-drain |
| `PC5` (Morpho, LED1) | GPIO_Output | Push-pull | Status LED 1 | ✅ |
| `PB2` (Morpho, LED2) | GPIO_Output | Push-pull | Status LED 2 | ✅ |

**Pre-step (before the above):** System Core → SYS → Debug = **Serial Wire** (allows `PB3` to be a clean GPIO, not debug JTDO).

**Leave as-is:** `PC13` (Nucleo's onboard button) and `PA5` (LD2, will become SPI1_SCK in step 4 — expected to flicker during SD writes).

---

## 4. SPI1 — SD Card (Connectivity → SPI1)

**Status:** ⚠️ Marked "Not configured" in checklist; **verify against current .ioc**

Mode: Full-Duplex Master. Pins auto-route once SPI1 is enabled:
- SCK = `PA5` (D13)
- MISO = `PA6` (D12)
- MOSI = `PA7` (D11)
- CS = `PB6` (D10, handled as plain GPIO from step 3, not SPI1_NSS — hardware NSS disabled)

**Parameter settings:** 8-bit data, MSB first, prescaler for safe initial SD init speed (start ~400kHz, then faster in FatFs driver after init).

**Known issue:** The `.ioc` currently has **TIM2** enabled (Period=39999) instead of TIM3. Decision needed: remove TIM2, add TIM3 as in step 5 below.

---

## 5. TIM3 — Buzzer + RGB LED (Timers → TIM3)

**Status:** ⚠️ Marked "Not configured"; **needs to be added**

Enable Channels 1–4, all as PWM Generation:
- CH1 → `PB4` (D5, buzzer)
- CH2 → `PC7` (D9, RGB Red)
- CH3 → `PC8` (Morpho, RGB Green)
- CH4 → `PB1` (Morpho, RGB Blue)

Set Prescaler + Period (ARR) so the PWM frequency suits an LED (a few hundred Hz to a few kHz, imperceptible flicker). Buzzer tone is driven at runtime by reprogramming CH1's frequency for different notes/patterns — pick a sane default here, don't overthink "the buzzer frequency".

---

## 6. ADC — Environmental Sensors (Analog)

**Status:** ⚠️ PARTIAL — see contradict note below

Channels split across two ADC instances:
- `PA0` (Battery, Potentiometer) → **ADC1_IN5** ✅
- `PA1` (Light, LDR) → **ADC2_IN6** ⚠️ (CUBEMX_CONFIG says ADC1; should be ADC2)
- `PA4` (Temperature, LM35) → **ADC2_IN9** ⚠️ (CUBEMX_CONFIG says ADC1; should be ADC2)
- `PB0` (Humidity, deferred) → ADC2_IN15 (parked for now)

**Enable both ADC1 and ADC2** in the Analog section. Configure:
- Independent mode on each ADC
- No continuous conversion
- No DMA (yet — one-shot conversions via Monitor task's `HAL_ADC_Start`/`HAL_ADC_PollForConversion` every 5s is fine for now)

**Resolved contradiction:** Checklist row 23 claimed "Battery ADC on ADC2" but CONFIG said "ADC1_IN5" — the spec is ADC1_IN5 per the pinout. Checklist was wrong; use the ADC1 assignment.

---

## 7. I2C3 — DS1307 RTC (Connectivity → I2C3)

**Status:** ⚠️ **UNRESOLVED** — decision pending

Enable I2C3, confirm pins land on `PC0` (SCL, A5) and `PC1` (SDA, A4) — **not I2C1** (`PB8`/`PB9`). Standard mode, 100kHz is plenty.

> **Critical decision:** The internal RTC (System Core → RTC → Activate Clock Source) is currently **enabled** in the `.ioc`, which contradicts using the external DS1307 chip. Choose one:
> - **Option A:** Keep DS1307 (I2C3 external chip) — disable the internal RTC (System Core → RTC → uncheck Activate).
> - **Option B:** Drop DS1307, use internal RTC instead — leave it enabled, skip I2C3 setup.
>
> **Recommendation:** Stick with DS1307 (Option A) unless you have a specific reason to change. Previous work assumed I2C3 + DS1307.

---

## 8. USART2 — Central Computer Link (should already be enabled)

**Status:** ✅ DONE

Confirm `PA2` (TX) / `PA3` (RX), Asynchronous mode, routed through the ST-LINK's virtual COM port (default on Nucleo-64). Baud rate: **115200** (matches the Central Computer app default).

---

## 9. IWDG — Watchdog (System Core → IWDG)

**Status:** ✅ Enabled

Set prescaler/reload so the timeout **comfortably exceeds** your longest expected gap between Watchdog-task refreshes (with margin). Exact numbers can wait until the Watchdog task's refresh period is decided, but enabling it now means the timeout is picked deliberately, not left at a CubeMX default.

---

## 10. FreeRTOS Middleware (Middleware → FREERTOS)

**Status:** ⚠️ **Needs to be added** (marked missing in checklist)

Interface: **CMSIS_V2**. Leave default task list empty for now (each module's task gets created in application code, not generated here).

**Settings:**
- Heap size: Start at 8–15 KB, adjust once real module stacks exist
- Tick rate: 1000 Hz default is fine

---

## 11. Project Manager Tab

**Status:** ✅ DONE

- Toolchain: STM32CubeIDE.
- **C++ convention:** CubeMX generates `main.c` and all HAL init code as C. Don't fight it — add your own `.cpp` files under `Modules/`/`Drivers_App/` alongside CubeMX's `.c` files. Any C++ file that needs a HAL/CMSIS header wraps the include in `extern "C" { #include "..." }` (CubeMX-generated headers already guard themselves).
- Task entry functions passed to `osThreadNew`/`xTaskCreate` must be plain C-linkage functions — write them as small trampolines that call into your C++ module objects.
- Code generation: "Generate peripheral initialization as a pair of .c/.h files per peripheral" — on (keeps things organized).

---

## 12. After Generating Code

**CRITICAL:** Build immediately with zero application logic added. Confirm it compiles and flashes clean before writing a single module. That's the real "peripherals configured" checkpoint.

---

## Quick Status Summary

| Item | Status | Action |
|---|---|---|
| Board/MCU | ✅ Selected | None |
| Clock (80MHz) | ✅ Done | None |
| USART2 | ✅ Done | None |
| IWDG | ✅ Enabled | None |
| Button 1 (PA10) | ⚠️ Plain GPIO | **Change to EXTI10** |
| Button 2 (PB3) | ⚠️ Plain GPIO, JTDO | **Set SYS→Debug=Serial Wire, change to EXTI3** |
| DHT11 (PB5) | ⚠️ Push-pull | **Change to Open-Drain** |
| RGB/LED (PC5, PB2) | ✅ Done | None |
| SD CS (PB6) | ✅ Done | None |
| **SPI1 (SD card)** | ⚠️ Not configured | **Enable Full-Duplex Master** |
| **TIM2** | ⚠️ Enabled (unused) | **Remove** |
| **TIM3 (buzzer+RGB)** | ⚠️ Not configured | **Enable CH1-CH4 PWM** |
| ADC1/ADC2 | ⚠️ Partial | **Enable both; verify PA1/PA4 on ADC2** |
| **I2C3 (RTC)** | ⚠️ Unresolved | **Decide: DS1307 (I2C3) or internal RTC** |
| **FreeRTOS** | ⚠️ Not added | **Add CMSIS_V2 middleware** |

---

## References

- **Pin mapping:** See `docs/hardware.md` (stack layout, pin conflicts, signal routing)
- **Wiring guide:** See `docs/WIRING_GUIDE.md` (physical connections for shields)
- **Build/test:** See `submarine-final-project/TEST_TRACKING.md` (hardware test checklist)
