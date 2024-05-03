# pico balloon WSPR tracker
Implements an extremely low-cost WSPR tracker for HAB (high altitude balloons) using only a Raspberry Pi Pico and a cheap GPS module such as a generic ATGM336H as shown (approx $4 as of April 2024). RF power in the 14Mhz (20 meter) band is generated directly by the Pi Pico. Two gpio pins are driven out of phase to feed a half wave dipole antenna trimmed for 20M (typically 38awg copper).

The RF synthesis and basic WSPR code is based on Roman Piksaykin's excellent work at https://github.com/RPiks/pico-WSPR-tx  

The altitude and full 6 character maidenhead grid are calculated from the GPS location data and transmitted along with the callsign. Solar array voltage and rp2040 temperature are also sent as telemetry when utilizing the U4B/Traquito protocol. If you specify a callsign suffix, it can ALSO/INSTEAD transmit WSPR Type 3 messages every ten minutes. These include the 6 character maidenhead grid and the altitude encoded using the Zachtek protocol. Type 3 messaging is arguably more robust than U4B, but less detailed.

The user's callsign and telemetry encoding details are configurable via the pico's USB port and a simple terminal program (ie Putty).

There is an issue with the RP2040 locking up if its input voltage is raised too gradually (as would happen during sunrise). To combat this I have a simple voltage dividor of two resistors across ground and the input voltage. The output if this voltage divider is tied to the RUN input on Pi Pico. The GPS unit stays on during transmission to provide continual frequency shift correction. However, a generic MOSFET or NPN transistor must be included to allow the pico to keep the GPS powered off during initial sunrise boot. See schematic below.

With the original code the Pico was being overclocked to 270Mhz, so the total power draw of the Pico and GPS module was around 100mA at 4 volts. But this version I have the speed down to 135Mhz, which is fine for transmitting on 20M (14Mhz).

# Quick-start
0. Download https://github.com/EngineerGuy314/pico-WSPRer/blob/main/build/pico-WSPRer.uf2 and skip to step 6 (or follow steps 1-5 to compile it yourself)
1. Install Raspberry Pi Pico SDK. Configure environment variables. Test whether it is built successfully.
2. git clone  https://github.com/EngineerGuy314/pico-WSPRer 
3. cd pico-WSPRer. 
4. ./build.sh
5. Check whether output file ./build/pico-WSPRer.uf2 appears.
6. power up pico with BOOTSEL held, copy the .uf2 file into the Pico when it shows up as a jumpdrive.
7. If you want to use the U4B protocol, go to https://traquito.github.io/channelmap/ to find an open channel and make note of id13 (column header), minute and lane (frequency).
8. Connect to pico with a USB cable and a terminal program such as Putty. Hit any key to access setup menu. Configure your callsign and telemetry channel details from step 7. 
9. You can use either one (or both) U4B and Zachtek protocol. If you only use one protocol its first message is sent on the starting-minute you enterred in step 8. If you are using both protocols, it starts the U4B messages on the starting minute, will then pause for two minutes and do the Zachtek messages (each protocol uses 4 minutes total). For Zachtek protocol (at this time) you must specify a numeric suffix that will be appended to your callsign. Enter dash (-) for the Suffix if you only want to use U4B protocol. Enter double dash (--) for Id13 if you only want to use Type3 (zachtek) protocol.
10. If the pico is plugged into a computer via USB while running it will appear as a COM port and basic diagnostic messages can be viewed at 115200 baud.  The amount of messages shown depends on the Verbosity setting in the user config menu.
![pico_WSPRer_schema5](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/95f2b65e-3c5c-47af-a960-af1c65ebdb9c)
![after cutting](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/07774544-43b6-43c6-89e8-56013e6f962f)
Shows ATGM336H GPS receiver and the Pi Pico board after trimming excess weight.

![on_scale](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/f15d8888-dc46-4c07-b3b2-78174d81c816)
Completed V3 unit before flight. GPS receiver is flipped over and glued to the RP2040. Total weight including GPS antenna 3.5g.

![v2 before succesful flight kc3lbr-7](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/6a0a48e6-81e2-477d-8a83-dc0bd025c36f)
Mounted beneath solar array, before first succesful flight.

![pico_WSPRer](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/bfaad70b-ae55-4695-b1ce-e6d6bb5c9d0f)
V1 prototype

![circumnavigation plus 2 days](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/b7c0b3ba-3f2a-43de-9247-e7e59d0fdf66)
V2 was launched and circumnavigated the world in 13 days.

![BEFORE first launch](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/f2614da2-a381-4c51-8be7-da3bd6e34af6)
Typical solar powered tracker and balloon before launch. Balloon is deliberately under-filled with hydrogen to allow room for expansion at high altitude. 12-13km altitude is typicaly reached.

![initial spots](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/ee4be0e1-1591-4dc4-9fc2-510bcd0ae6cd)
Global tracking with such low powered transmissions are possible thanks to the extensive network of WSPR receive stations.

DISCLAIMER: This project transmits on the 20 meter ham radio band: you must have an appropriate amatuer radio license to do so legally. Also, toggling a microcontroller IO pin theoretically generates a square wave, which theoretically has an infinite number of high order harmonics. However, RP2040 io pin circuitry is not particularly efficient at generating very high RF energy. Furthermore, the dipole antenna is trimmed to be resonant at 14.1Mhz and does not do a very good job at radiating anything else. During my testing the amount of RF energy actually emitted outside of the passband was well within limits. You are encouraged to perform your own testing and utilize additionall filtering as needed to meet your local regulations.



