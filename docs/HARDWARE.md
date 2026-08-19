# Hardware

Primary stack from the design conversation:

- Arduino Mega 2560
- Snowball Creek LCC shield (MCP2515 CAN, SPI)
- Keyestudio KS0258 16-channel PCA9685 servo shield
- Miuzei SG90 9 g servo, 3D-printed under-layout mount
- Two QXAD0141 / ASIN B07YQQ3R7W 3-terminal microswitches (SPDT)

Uno is supported for a single turnout. Mega is the intended node: 16 PCA9685 channels and enough analog pins for one resistor ladder per turnout.

## Stacking

1. Mega 2560 on the bench.
2. Snowball Creek LCC shield on the Mega. Align the 6-pin ICSP header.
3. KS0258 on top of the LCC shield **or** wire a KS0258 / PCA9685 as a module to Mega SDA/SCL 20/21.

KS0258 is an Uno-footprint I2C shield. It ties PCA9685 SDA/SCL to **A4/A5**.

| Board | Hardware I2C | What the KS0258 shield actually hits |
| --- | --- | --- |
| Uno / Nano | A4/A5 | Correct |
| Mega 2560 | 20/21 | A4/A5, **not** 20/21 |

On Mega, jumper shield **A4 → 20** and **A5 → 21**, then use `Wire` on 20/21. After that jumper, do not use A4/A5 as analog inputs.

KS0258 proto holes are the intended place to solder the 10 k / 4.7 k / 2.2 k ladder and the 100 nF filter for channel 0.

## Snowball Creek pins (do not reuse)

| Pin | Function |
| --- | --- |
| D2 | MCP2515 IRQ |
| D3 | Isolated DCC (cut SB1 to free) |
| D5, D6 | On-board LEDs (cut SB2 / SB3 to free) |
| D7 | 256 K SPI EEPROM CS |
| D8 | MCP2515 CS |
| ICSP | SPI (Mega 50/51/52) |

A0–A3 and A6–A15 stay free on Mega. A0 is the default limit-ladder input for turnout 0.

When `RR_USE_KS0258` is 0 on Mega, onboard servo PWM is **D44** (then D45, D46). Do not use D9/D10: the Waveshare 4" TFT uses D9 (backlight) and D10 (LCD_CS), and D3–D13 in full.

The Waveshare 4" TFT (ILI9486, XPT2046) also uses D3, D4, D5, D6, D7, D8. That overlaps Snowball Creek D3/D5/D6/D7/D8. **Do not stack Snowball Creek and the 4" TFT** on any Arduino-footprint board.

Power: leave JP2/JP3 **off** unless you intend to power the Arduino from the LCC bus. Never power stall-capable servos from the Mega 5 V pin. Feed KS0258 **V+** from a separate 5 V supply (KS0258 V+ max 6 V). Tie all grounds together: Mega, KS0258 logic, servo V+, and switch harness.

## Servo (Futaba / JR 3-pin)

KS0258 channel header, brown / red / orange:

| Wire | Function | KS0258 |
| --- | --- | --- |
| Brown | GND | − |
| Red | +5 V servo | V+ |
| Orange | PWM | SIG |

Miuzei SG90: 4.8–6 V. Default pulses in firmware are 1000 µs (closed) and 2000 µs (thrown). Tune after the mount is installed so the horn reaches each switch without stalling.

One SG90 can draw ~650 mA stalled. Budget the V+ supply for every servo that can move at once.

## Dual limit-switch harness (one analog pin)

No commercial Futaba-style plug was found that already includes the divider. The harness is DIY.

Each QXAD0141 is used as SPST: **COM** and **NO**. The switch is closed when the turnout reaches that end.

```
                    Mega / shield proto area

                         5 V
                          |
                       [10 kΩ]          pull-up
                          |
                          +------------------+
                          |                  |
                    Analog pin            [100 nF]  to GND
                          |                  |
                          +------------------+
                          |
               +----------+----------+
               |                     |
            [4.7 kΩ]              [2.2 kΩ]
               |                     |
          Switch A              Switch B
          Thrown                Closed
          COM ── NO             COM ── NO
               |                     |
              GND                   GND
```

Suggested Futaba-style 3-pin for the harness (keyed the same way as the servo lead):

| Pin | Color (match servo) | Function |
| --- | --- | --- |
| 1 | Brown | GND (both COM returns) |
| 2 | Red | 5 V to the 10 kΩ pull-up (or omit if pull-up lives on the node) |
| 3 | Orange | Analog sense (junction of the two series resistors) |

Prefer putting the three resistors and the capacitor **on the node proto area**, and running only GND + sense + (optional) 5 V out to the layout. That keeps the precision parts off the moving mount.

### Expected 10-bit ADC (5 V AREF, ideal resistors)

| State | Voltage | ADC (0–1023) | Firmware decode |
| --- | --- | --- | --- |
| Neither | ~5.0 V | ~1023 | `Neither` if raw > 900 |
| Only A (Thrown) | ~1.6 V | ~330 | `Thrown` if raw > 250 |
| Only B (Closed) | ~0.9 V | ~185 | `Closed` if raw > 150 |
| Both | ~0.6 V | ~125 | `Both` otherwise |

Print raw ADC during bring-up and adjust `LimitLadderConfig` if your resistors or switch resistance shift the bands. Keep the pull-up at or below 10–20 kΩ so the Mega analog input is not too high-impedance.

## Channel map (Mega + stacked KS0258)

| Turnout | PCA9685 ch | Analog pin | Notes |
| --- | --- | --- | --- |
| 0 | 0 | A0 | Default bring-up |
| 1 | 1 | A1 | |
| 2 | 2 | A2 | |
| 3 | 3 | A3 | |
| 4–13 | 4–13 | A6–A15 | Skip A4/A5 when the shield uses them for I2C |
| 14–15 | 14–15 | — | Drive only, or add a mux later |

Uno with a stacked KS0258 has A4/A5 taken, so only A0–A3 remain: four analog-feedback turnouts.

## Mechanical

Under-layout 3D-printed SG90 mount. Each end of travel must actuate one microswitch cleanly. Leave a little over-travel so the horn cannot stall the servo if a pulse is a few degrees long. Firmware stops PWM when the destination limit closes.
