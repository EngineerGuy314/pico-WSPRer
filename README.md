



----------

PLEASE SEE THE [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) FOR SIGNIFICANTLY MORE DETAILED INFORMATION.
----------
click [here](https://github.com/EngineerGuy314/pico-WSPRer/raw/main/build/pico-WSPRer.uf2) to download the pre-compiled firmware (original version)
click [here](https://github.com/EngineerGuy314/pico-WSPRer/raw/multi-band/build/pico-WSPRer.uf2) to download the pre-compiled firmware of the new, multi-band version.

Summary
-------

Extremely low-cost Raspberry Pi Pico based WSPR beacon for tracking GPS position and other telemetry from  High Altitude Balloons (HAB), specifically "pico balloons".

Tracker uses the RP2040 microntoller in the Raspberry Pi to directly generate a 20mW RF signal using software trickery. No RF oscillator, TCXO, transmitter or amplifier is needed.

Only two common, readily available and cheap components are needed to implement: A Raspberry Pi Pico, and a tiny GPS module (ATGM336H, or a uBlox clone). Two resistors and some bits of wire and solder hold it together.

Instead of using a TCXO, the extremely precise frequency base needed for the WSPR protocol is obtained by continually "disciplining" the standard crystal oscillator onboard the Pico with the PPS pulses from the GPS module. 

The [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) has more information, instructions and schematics etc.
![img](https://github.com/user-attachments/assets/a7859439-c92a-4207-a469-404ffbfd11a1)
![img2](https://github.com/user-attachments/assets/27b19677-2e85-43d8-b7d3-9103fa6c7361)


