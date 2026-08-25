# JMRI panels

Canonical copies of Layout Editor panels live here.
The live JMRI profile stays in `~/.jmri/` (Linux) or `%USERPROFILE%\.jmri\` (Windows).
Do not commit the live profile.

Copy a panel into the JMRI railroad folder, then **File → Load**.

## Mega only (v1.02)

```text
docs/jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml
```

CAN in JMRI is the RR-CirKits gateway (STM32 VID 0483:5740 @ 57600), **not** the Mega USB serial.

| Panel turnout | Node / channel | Throw event | Close event |
| --- | --- | --- | --- |
| Left servo KS0258 (TO3) | Mega `.A5.02` ch0 | `05.01.01.01.A5.02.00.00` | `05.01.01.01.A5.02.00.01` |
| Right servo KS0258 (TO4) | Mega `.A5.02` ch1 | `05.01.01.01.A5.02.01.00` | `05.01.01.01.A5.02.01.01` |

## Mega + ESP32 loop (v2.02)

```text
docs/jmri/Layout-2026-08-24-For_Mega_ESP32_Loop.xml
```

Oval main (two tracks + 180° end curves, not vertical straights) with a north siding and a south siding.
Mega is the west pair; ESP32 is the east pair. ESP32 uses OpenLCB over Wi-Fi
GridConnect (JMRI hub TCP **12021**), not Snowball Creek.

| Panel turnout | Node / channel | Throw event | Close event |
| --- | --- | --- | --- |
| Mega KS0258 ch0 (TO3) | `.A5.02` ch0 | `…A5.02.00.00` | `…A5.02.00.01` |
| Mega KS0258 ch1 (TO4) | `.A5.02` ch1 | `…A5.02.01.00` | `…A5.02.01.01` |
| ESP32 PCA9685 ch0 (TO5) | `.A5.03` ch0 | `…A5.03.00.00` | `…A5.03.00.01` |
| ESP32 PCA9685 ch1 (TO6) | `.A5.03` ch1 | `…A5.03.01.00` | `…A5.03.01.01` |

Throw/close pulses are still 1000/2000 µs until trimmed. Leave Mega serial
closed while clicking (DTR resets the node). ESP32 servo hardware is still
waiting on a genuine Adafruit PCA9685; the node currently logs throw/close.
