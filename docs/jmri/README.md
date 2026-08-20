# JMRI panels (Mega + Snowball Creek + KS0258)

Canonical copies of the Layout Editor panels used with this node live here.
The live JMRI profile stays in `~/.jmri/` (Linux) or `%USERPROFILE%\.jmri\` (Windows).
Do not commit the live profile.

Copy a panel into the JMRI railroad folder, then **File → Load**:

```text
~/.jmri/My_JMRI_Railroad.jmri/Layout-2026-08-20-For_Mega_Two_Servos.xml
```

Node **05.01.01.01.A5.02** (OwlThree). CAN in JMRI is the RR-CirKits gateway
(STM32 VID 0483:5740 @ 57600), **not** the Mega USB serial.

| Panel turnout | KS0258 | Throw event | Close event |
| --- | --- | --- | --- |
| Left servo KS0258 (TO3) | ch0 | `05.01.01.01.A5.02.00.00` | `05.01.01.01.A5.02.00.01` |
| Right servo KS0258 (TO4) | ch1 | `05.01.01.01.A5.02.01.00` | `05.01.01.01.A5.02.01.01` |

Throw/close pulses are still 1000/2000 µs until trimmed. Leave the Mega serial
monitor closed while clicking (DTR resets the node).
