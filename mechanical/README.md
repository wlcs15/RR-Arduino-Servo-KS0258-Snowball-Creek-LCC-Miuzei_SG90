# Under-layout SG90 mount

Print a mount that holds a Miuzei SG90 below the benchwork and presents a horn or crank to the turnout throwbar.

Each end of travel must close one QXAD0141 (or equivalent SPDT microswitch) on COM–NO. Leave mechanical over-travel so a slightly long pulse cannot stall the servo; firmware releases PWM after the destination switch closes.

Keep the 10 k / 4.7 k / 2.2 k / 100 nF parts on the node proto area. Run a Futaba-style 3-pin (GND / 5 V / sense) or a 2-pin (GND / sense) up from the mount.

STL/OpenSCAD for the Snowball Creek–specific mount will live in this directory when the first print is ready.
