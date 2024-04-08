# pico balloon WSPR tracker
This code is based on Roman Piksaykin's excellent work at https://github.com/RPiks/pico-WSPR-tx  

This program allows a Raspberry Pi Pico to function as a pico-balloon WSPR tracker, with the only other required hardware being a cheap GPS module such as ATGM336H.

The program calculates the altitude and full 6 character maidenhead grid based on the GPS data and transmits it along with the callsign, utilizing the U4B/Traquito protocol. Solar array voltage and rp2040 temperature are also sent.

The user's callsign and telemetry encoding details are now configurable via the pico's USB port and a simple terminal program (ie Putty).

With the original code the Pico was being overclocked to 270Mhz, so the total power draw of the Pico and GPS module was around 100mA at 4 volts. But this version I have the speed down to 135Mhz, which is fine for transmitting on 20M (14Mhz).

There is an issue with the RP2040 locking up if its input voltage is raised too gradually. To combat this I put a voltage dividor of two resistors across ground and the input voltage. The output if this voltage divider is tied to the RUN input on Pi Pico. Preliminary testing indicates this to be sufficient.

New revision utilizes a transistor to power down the GPS unit during transmission. See schematic below. In this configuration a 6 cell Solar Array that can provide 80mA at 3.4v will be sufficient.

# Quick-start
0. download https://github.com/EngineerGuy314/pico-WSPRer/blob/main/build/pico-WSPRer.uf2 and skip to step 6 (or follow steps 1-5 to compile it yourself)
1. Install Raspberry Pi Pico SDK. Configure environment variables. Test whether it is built successfully.
2. git clone  https://github.com/EngineerGuy314/pico-WSPRer 
3. cd pico-WSPRer5. 
4. ./build.sh
5. Check whether output file ./build/pico-WSPRer.uf2 appears.
6. power up pico with BOOTSEL held, copy the .uf2 file into the Pico when it shows up as a jumpdrive.
7. Go to https://traquito.github.io/channelmap/ to find an open channel and make note of id13 (column header), minute and lane (frequency)");
8. Connect to pico with a USB cable and a terminal program such as Putty. Hit any key to access setup menu. Configure your callsign and telemetry channel details from step 7. 
9. WSPR type-1 messages will be sent every ten minutes (hh:00, hh:10, ...) followed by the telemetry with a coded callsign
10. If the pico is plugged into a computer via USB while running it will appear as a COM port and diagnostic messages can be viewed at 115200 baud.
![pico_WSPRer_schema3](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/3f5834ce-b7f6-4771-9a32-b0801d9130fa)
![v2 before succesful flight kc3lbr-7](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/6a0a48e6-81e2-477d-8a83-dc0bd025c36f)
Mounted beneath solar array, before first succesful flight.
![pico_WSPRer](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/bfaad70b-ae55-4695-b1ce-e6d6bb5c9d0f)
V1 prototype




