



----------

PLEASE SEE THE [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) FOR SIGNIFICANTLY MORE DETAILED INFORMATION.
----------
click [here](https://github.com/EngineerGuy314/pico-WSPRer/raw/main/build/pico-WSPRer.uf2) to download the latest pre-compiled firmware

Summary
-------

This project implements an extremely low-cost WSPR beacon for tracking GPS position and other telemetry from  High Altitude Balloons (HAB), specifically "pico balloons".

What unique about this tracker is that it uses a RP2040 (Raspberry Pi Pico) to directly generate the RF signal using software trickery. No radio oscillator, transmitter or amplifier is needed.

Also unique to this tracker is that it requires only two common, readily available and cheap components: A Raspberry Pi Pico, and a tiny GPS module (ATGM336H, or a uBlox clone). Two resistors and some bits of wire and solder hold it together.

Instead of using a TCXO, the extremely precise frequency base needed for the WSPR protocol is obtained by continually "disciplining" the standard crystal oscillator onboard the Pico with the PPS pulses from the GPS module. 

The [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) has more information, instructions and schematics etc.


