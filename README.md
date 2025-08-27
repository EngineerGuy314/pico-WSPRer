



----------

PLEASE SEE THE [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) FOR SIGNIFICANTLY MORE DETAILED INFORMATION.
----------
click [here](https://github.com/EngineerGuy314/pico-WSPRer/raw/main/build/pico-WSPRer.uf2) to download the pre-compiled firmware (newest version, NOW INCLUDES MULTI-BAND, 6M through 40M)

Summary
-------

**--UPDATE August 2025--** There will probably be no further updates to this project. I am now focusing on my [JAWBONE](https://github.com/EngineerGuy314/JAWBONE) tracker. It utilizes a MS5351, like most other trackers. This provides much greater performance and is no longer reliant on a strong GPS signal for stability. Unfortunately, it is no longer practical to construct the JAWBONE out of a Raspberry Pi Pico. It is still based on a RP2040, but on a custom PCB.

Extremely low-cost Raspberry Pi Pico based WSPR beacon for tracking GPS position and other telemetry from  High Altitude Balloons (HAB), specifically "pico balloons".

Tracker uses the RP2040 microntoller in the Raspberry Pi to directly generate a 20mW RF signal using software trickery. No RF oscillator, TCXO, transmitter or amplifier is needed.

Only two common, readily available and cheap components are needed to implement: A Raspberry Pi Pico, and a tiny GPS module (ATGM336H, or a uBlox clone). Two resistors and some bits of wire and solder hold it together.

Instead of using a TCXO, the extremely precise frequency base needed for the WSPR protocol is obtained by continually "disciplining" the standard crystal oscillator onboard the Pico with the PPS pulses from the GPS module. 

The [WIKI](https://github.com/EngineerGuy314/pico-WSPRer/wiki/pico%E2%80%90WSPRer-(aka-Cheapest-Tracker-in-the-World%E2%84%A2)) has more information, instructions and schematics etc. The project also includes gerber files to make a custom PCB version of the tracker, that is lighter but uses the same firmware as the Pi Pico version.

A special thanks to the giants on whose shoulders I stood on to make this project:
  Roman Piksaykin (https://github.com/RPiks/) for the original program base and Kazu (https://github.com/kaduhi/pico-fractional-pll) for the vastly superior method of generating RF with the rp2040's internal PLL

![img](https://github.com/user-attachments/assets/a7859439-c92a-4207-a469-404ffbfd11a1)
![img2](https://github.com/user-attachments/assets/27b19677-2e85-43d8-b7d3-9103fa6c7361)
![v2](https://github.com/user-attachments/assets/7a1ebb38-9a00-44dd-9709-f951d1f45a56)
