# Hardware Layout & Signal Mapping: STM32L4 + SD Logger Shield + 9-in-1 Shield

## 1. Physical Stack Architecture

```

+-----------------------------------+
|    9-in-1 Sensor Shield (Top)     |
+-----------------------------------+
|  Arduino Data Logger Shield (Mid) |
+-----------------------------------+
|    STM32L4 Nucleo Board (Base)    |
+-----------------------------------+

```

---

## 2. Shared Pin Allocation & Conflict Matrix

| Arduino Header | STM32 Pin | Signal / Function        | Peripheral Domain   | Voltage / Circuit Requirement                   | Shared Line Conflict Notes                                         |
| :------------- | :-------- | :----------------------- | :------------------ | :---------------------------------------------- | :----------------------------------------------------------------- |
| **A0**         | `PA0`     | Potentiometer Input      | `ADC1_IN5`          | $10\text{ k}\Omega / 20\text{ k}\Omega$ Divider | Scaled $0\text{--}5\text{V} \rightarrow 0\text{--}3.3\text{V}$ max |
| **A1**         | `PA1`     | LDR Light Sensor Input   | `ADC1_IN6`          | $10\text{ k}\Omega / 20\text{ k}\Omega$ Divider | Scaled $0\text{--}5\text{V} \rightarrow 0\text{--}3.3\text{V}$ max |
| **A2**         | `PA4`     | LM35 Temperature Sensor  | `ADC1_IN9`          | Direct Connection                               | Output range is $< 1.5\text{V}$                                    |
| **A4**         | `PC1`     | RTC Data (SDA)           | `I2C1_SDA`          | Internal/External Pull-up                       | Shared with 9-in-1 I2C breakout                                    |
| **A5**         | `PC0`     | RTC Clock (SCL)          | `I2C1_SCL`          | Internal/External Pull-up                       | Shared with 9-in-1 I2C breakout                                    |
| **D2**         | `PA10`    | Push Button 1            | `GPIO_Input`        | Internal `GPIO_PULLUP`                          | Active LOW                                                         |
| **D3**         | `PB3`     | Push Button 2            | `GPIO_Input`        | Internal `GPIO_PULLUP`                          | Active LOW                                                         |
| **D4**         | `PB5`     | DHT11 Data Line          | `GPIO Output/Input` | $5\text{V}$-Tolerant Pin                        | Single-wire bit-banged I/O                                         |
| **D5**         | `PB4`     | Passive Buzzer           | `TIM3_CH1` (PWM)    | Digital Drive                                   | Tone frequency generator                                           |
| **D9**         | `PB0`     | RGB LED (Red Channel)    | `TIM3_CH3` (PWM)    | PWM Duty Cycle                                  | Color mixing                                                       |
| **D10**        | `PB6`     | SD Card Chip Select (CS) | `GPIO_Output`       | Push-Pull, Default HIGH                         | **Conflict:** Shares line with RGB Green channel                   |
| **D11**        | `PA7`     | SD Card SPI MOSI         | `SPI1_MOSI`         | Full-Duplex Master                              | **Conflict:** Shares line with RGB Blue channel                    |
| **D12**        | `PA6`     | SD Card SPI MISO         | `SPI1_MISO`         | Full-Duplex Master                              | **Conflict:** Shares line with Status LED 1                        |
| **D13**        | `PA5`     | SD Card SPI SCK          | `SPI1_SCK`          | Full-Duplex Master                              | **Conflict:** Shares line with Status LED 2                        |

---

## 3. Bus Interfaces & Peripheral Routing

- **Analog Front-End (ADC1):**
  - Channels `ADC1_IN5`, `ADC1_IN6`, and `ADC1_IN9` process converted signals.
  - Resistor voltage dividers ($10\text{ k}\Omega / 20\text{ k}\Omega$) clamp $5\text{V}$ analog signals from `A0` and `A1` to a maximum of $3.3\text{V}$ to prevent damage to the STM32 ADC.

- **SPI Bus (SPI1):**
  - Drives the SD Card Reader on the Data Logger Shield using standard Arduino SPI pins (`D11`–`D13`).
  - `D10` (`PB6`) acts as the dedicated Chip Select line.
  - **Bus Collision Handling:** High-frequency clocking and data transfers on SPI1 cause connected LEDs on the 9-in-1 shield to flicker during read/write cycles. `D10` is kept HIGH when the SPI bus is idle.

- **I2C Bus (I2C1):**
  - Connects to the DS1307/PCF8523 RTC chip on the Data Logger shield via `A4` (SDA) and `A5` (SCL) to provide timestamps for logged data.

```

```

---

## 4. Review Notes (verified against ST's official NUCLEO-L476RG Arduino-header pinout diagram)

**Confirmed error — D9.** D9 is `PC7`, not `PB0`. `PB0` is actually the Nucleo's **A3** pin. Wiring the RGB Red channel's firmware config to PB0 would target a physically different header pin than where the shield's D9 line actually lands. Correct PWM channel for PC7 is `TIM3_CH2`, not `TIM3_CH3` (the TIM3_CH3 pairing given is only valid if the pin really were PB0).

**Confirmed error — I2C peripheral number.** ST's own header silkscreen labels `A4`/`A5` (`PC1`/`PC0`) as **I2C3** (SDA/SCL), not I2C1. The Arduino header's actual I2C1 pins are `D14` (`PB9`, SDA) and `D15` (`PB8`, SCL) — separate physical pins near the top of CN9. CubeMX won't even offer I2C1 as an alternate function on PC0/PC1, so this needs to become I2C3 in the config, or the RTC needs to move to D14/D15 for true I2C1.

**Undisclosed conflict — D13/PA5 is the Nucleo's onboard LED (LD2).** By default (solder bridge SB42 closed), PA5 also drives the board's built-in green LED. Every SPI SCK pulse to the SD card will visibly toggle that LED unless SB42 is removed. Not fatal, but worth knowing before debugging "why is my board LED flickering during SD writes."

**The stated SPI/RGB conflict isn't actually mitigated by "CS kept HIGH when idle."** RGB Green (D10) and Blue (D11) sit on the exact same electrical node as SD CS and SD MOSI. CS goes low on every SD transaction and MOSI/SCK toggle continuously during transfers — that's not occasional flicker, it makes independent control of those two LED channels impossible while the SD card is in use. Same problem for the two standalone status LEDs on D12/D13 (SD MISO/SCK).

**Bigger picture — this is a pin-budget shortfall, not a wiring quirk.** The original 9-in-1 shield pinout already used every Arduino Uno pin (A0–A5, D0–D13). Stacking an SD+RTC logger shield that also needs D10–D13 (SPI) and A4/A5 (I2C) has nowhere conflict-free to land — and A4/A5 were *also* already earmarked on the 9-in-1 shield's own I2C header (for a possible OLED). Three peripherals are now competing for the same two pins. This isn't fixable by pin re-mapping alone within the Uno footprint; RGB Green/Blue and the two standalone LEDs need to be re-wired with jumper leads to free pins on the Nucleo's Morpho connector (CN7/CN10 expose plenty of spare GPIO, e.g. PC6, PC8, PC9, PC10, PC11, PC12, PD2, PH0, PH1) rather than relying on the pass-through Uno header pins those shields share.

**Not covered by this table:** the IR receiver (D6), the two Extra Digital Header pins D7/D8 (candidate object-detection Echo/Trig), and A3 — all still need their final assignment once the RGB/LED rewiring above is settled. Also worth pinning down: the exact SD/logger shield model and RTC chip (DS1307 vs PCF8523 have different register maps).

---

## 5. Resolved Pin Map (decision: jumper RGB Green/Blue + status LEDs to Morpho header; RTC = DS1307)

Keep on the shared Arduino/Uno header (no rewiring needed — these don't conflict with the SD/RTC shield):

| Signal | Pin | Peripheral |
| :--- | :--- | :--- |
| Potentiometer (battery) | A0 / `PA0` | `ADC1_IN5` |
| LDR (light) | A1 / `PA1` | `ADC1_IN6` |
| LM35 (temp, if used alongside/instead of DHT11) | A2 / `PA4` | `ADC1_IN9` |
| Push Button 1 | D2 / `PA10` | GPIO EXTI, pull-up, active low |
| Push Button 2 | D3 / `PB3` | GPIO EXTI, pull-up, active low |
| DHT11 data | D4 / `PB5` | GPIO bit-banged |
| Buzzer | D5 / `PB4` | `TIM3_CH1` PWM |
| RGB Red | D9 / `PC7` | `TIM3_CH2` PWM (corrected from the earlier PB0/CH3 error) |
| SD Card CS | D10 / `PB6` | GPIO output |
| SD Card MOSI | D11 / `PA7` | `SPI1_MOSI` |
| SD Card MISO | D12 / `PA6` | `SPI1_MISO` |
| SD Card SCK | D13 / `PA5` | `SPI1_SCK` (note: also the Nucleo's onboard LD2 LED — expect it to flicker during SD writes) |
| RTC (DS1307) SDA | A4 / `PC1` | `I2C3_SDA` (corrected from I2C1) |
| RTC (DS1307) SCL | A5 / `PC0` | `I2C3_SCL` (corrected from I2C1) |
| ST-LINK VCP (LNC↔Central Computer UART) | D0/D1 / `PA3`/`PA2` | `USART2` |

Rewired via jumper leads to the Morpho header (CN10) to escape the SPI/I2C conflict:

| Signal | Morpho pin | Peripheral |
| :--- | :--- | :--- |
| RGB Green | `PC8` (CN10) | `TIM3_CH3` PWM |
| RGB Blue | `PB1` (CN10) | `TIM3_CH4` PWM |
| Standalone LED1 | `PC5` (CN10) | GPIO output |
| Standalone LED2 | `PB2` (CN10) | GPIO output |

**Correction (2026-09-03):** an earlier version of this table put RGB Green on `PC6`/`TIM3_CH1` — but the buzzer (D5/`PB4`) already occupies `TIM3_CH1`. Both pins are valid alternate-function options *for the same channel*, so they'd have been forced to share one identical PWM waveform (Green's brightness tied to the buzzer's tone/duty) rather than being independently controllable. Moved Green to `TIM3_CH3` (`PC8`) and Blue to `TIM3_CH4` (`PB1`) instead, so Buzzer=CH1, Red=CH2, Green=CH3, Blue=CH4 — four genuinely independent channels, all still on one TIM3 instance.

Still unassigned, to be settled once the object-detection sensor is chosen: IR receiver (originally D6/`PB10`), Extra Digital Header D7/`PA8` and D8/`PA9` (Echo/Trig candidates), spare analog A3/`PB0`.

**DS1307 shield voltage — confirmed safe (2026-09-03).** The specific shield in use is spec'd for a stable 3.3V output/logic level ("3.3V Output Voltage Compatibility... safe and reliable performance with low-voltage microcontrollers"), so its I2C pull-ups reference 3.3V rather than 5V — the earlier caution about over-volting PC0/PC1 doesn't apply here. Still confirm the shield is actually powered from the Nucleo's 3V3 pin (not 5V) per its documentation, since a 3.3V-logic shield typically expects a 3.3V supply rail too, not just 3.3V-safe I/O.

Shield identity confirmed: compact (2x2x2 cm) DS1307 RTC + SD card data-logging shield, Arduino-form-factor, 3.3V-native. No separate model/brand name given, but the 3.3V-native design is the operative fact for wiring.

---

## 6. Shield identification (for reference)

- **9-in-1 sensor shield:** "Multi-functional sensor expansion board for Arduino" — DHT11 + LM35 + RGB LED + IR receiver + buzzer, built specifically for Arduino UNO R3 (i.e. native 5V logic — confirms why A0/A1 need the voltage dividers already noted in §3; no 3.3V-native claim like the RTC shield). 5x5x5 cm, onboard voltage regulator. No brand/model given beyond this description.
- **RTC/SD logger shield:** compact (2x2x2 cm) DS1307 + SD card data-logging shield, natively 3.3V I/O (see §5) — safe to wire directly to the STM32's 3.3V GPIO.

---

## 7. Object Detection sensor — resolved (no new parts available)

No ultrasonic sensor, dedicated IR-obstacle module, or spare IR LED emitter is available, and no new parts can be added — so the bare IR receiver (D6) can't be turned into a break-beam detector and no other sensor option is on the table.

**Decision:** repurpose **Push Button 2 (D3 / `PB3`)** as a manual object-detected/object-cleared toggle. The spec only requires one physical button (silencing the alarm, already Button 1 / D2 / `PA10`) — Button 2 has no assigned role otherwise, so this costs no extra hardware and no pin changes. The Object Detection module still exposes the same detected/cleared interface to Event/Communication either way; only the physical trigger is a button press instead of a real distance/IR reading. Worth a line in the project write-up noting this is a simulated input due to hardware constraints.

IR receiver (D6) and Extra Digital Header D7/D8 remain unused for now — free for anything else that comes up, or simply left unpopulated.
