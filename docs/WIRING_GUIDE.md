# Physical Wiring Guide

Written from the resolved pin map in `hardware.md` §5/§7. Most of the 20 Uno-header pins just stack straight through with no extra work — six specific signals need rework because of the SPI/I2C conflict already documented. Read the whole "pins needing rework" section before connecting anything, since two of the six require physically isolating a pin from the stack, not just adding a wire.

## 0. Before touching anything

- Nucleo unplugged from USB.
- Have on hand: a multimeter, small jumper wires (male-male and female-male depending on your shields' headers), two resistors per divider (10 kΩ + 20 kΩ, or the closest E12 values you have — 10k+22k works fine too, ratio is what matters), and fine pliers if any pins need bending.
- Physical stack order per your diagram: 9-in-1 shield (top) → SD/RTC logger shield (middle) → Nucleo-L476RG (bottom). Don't fully seat the top shield until the rework below is done.

## 1. Power and ground — no action needed

Both shields draw power through the standard Arduino power pins (5V, 3.3V, GND) that pass straight through the stack automatically. The 9-in-1 shield should be pulling from the 5V pin (it's a 5V-native board) and the RTC/SD shield from 3.3V (per its spec sheet) — worth a quick multimeter check on each shield's power pin *before* stacking, just to confirm each board is actually wired to the rail it claims, rather than assuming.

## 2. Pins that stack straight through — no rework

A2 (LM35), D2/D3 (buttons), D4 (DHT11), D5 (buzzer), D9 (RGB Red). These pins have nothing else competing for them, so plug straight through the stack normally.

## 3. Pins needing a voltage divider: A0 (potentiometer), A1 (LDR)

The 9-in-1 shield outputs 0–5V on these; the STM32's ADC input is 3.3V max. **Do not let these pass straight through the stack** — the raw 5V would land directly on the Nucleo pin.

For each of A0 and A1: identify the shield's output pad for that signal (the pin that would otherwise plug into the Nucleo's A0/A1). Instead of a direct connection, build this small network outside the stack:

```
Shield's A0/A1 output ---[10k resistor]---+---[20k resistor]--- GND
                                            |
                                            +--- Nucleo PA0 / PA1
```

The junction between the two resistors is what connects to the Nucleo's pin — this divides the 0–5V swing down to 0–3.33V. If your shield's header pins are the pluggable kind, you'll need to either bend the A0/A1 pins on the shield up so they don't insert into the header below, and instead run a wire from the shield's A0/A1 pad through the divider to the Nucleo, or build the divider on a small breadboard sitting between the two if you'd rather not bend anything.

## 4. Pins needing full reroute to the Morpho header: D10, D11, D12, D13

These four pins are physically shared between the 9-in-1 shield (RGB Green, RGB Blue, LED1, LED2) and the SD/RTC shield (SPI SCK/MISO/MOSI/CS) once stacked — they can't both use the same electrical node. The SD shield needs to keep D10–D13 for its own SPI bus, so the 9-in-1 shield's four signals move to the Morpho header instead:

| Signal | Move from | Move to (Morpho, CN10) |
| :-- | :-- | :-- |
| RGB Green | D10 | `PC8` |
| RGB Blue | D11 | `PB1` |
| Standalone LED1 | D12 | `PC5` |
| Standalone LED2 | D13 | `PB2` |

For each: bend that pin up on the 9-in-1 shield (so it does **not** insert into the header stack below it — this is what stops it from electrically joining the SD shield's SPI lines) and run a jumper wire from that now-isolated pin directly to the corresponding Morpho pin on the Nucleo. The Morpho header (CN10) is the separate 2-row header on the side of the Nucleo board, still accessible even with the shields stacked on top since it's outside the Uno-shaped footprint.

If your shield uses soldered-in pin headers rather than a socket, bending is the practical option (small needle-nose pliers, bend at the base close to the PCB, away from the other pins) — it's a standard technique for exactly this kind of shield conflict and doesn't damage the shield as long as you don't repeatedly flex the same pin. If you'd rather not bend anything, the alternative is to not fully seat the 9-in-1 shield into the stack at all, and instead run individual jumper wires from every one of its pins to their targets (more wires, but nothing gets bent).

## 5. RTC/SD shield specifics

- SD card: just insert it into the shield's card slot — no wiring, the shield's onboard circuitry handles the SPI-to-card interface.
- DS1307 backup battery: check the shield has a CR2032 (or similar) coin cell seated in its holder. Without it, the DS1307 loses the time/date every time power is removed, which defeats the point of an RTC for this project.

## 6. Before powering on

1. With everything wired but the Nucleo still unplugged, use the multimeter in continuity mode to confirm: no short between 5V and 3.3V, no short between either rail and GND, and that D10–D13 on the Nucleo side are *not* shorted to the bent-up pins on the 9-in-1 shield (confirms the isolation actually took).
2. Power the stack from USB and check actual voltages at a few test points (5V rail, 3.3V rail, and the divided A0/A1 outputs with the potentiometer at both extremes) before plugging in the CubeMX-flashed firmware.
3. Only then move on to the CubeMX-generated blink/UART smoke test from `CUBEMX_CONFIG.md`'s last step.
