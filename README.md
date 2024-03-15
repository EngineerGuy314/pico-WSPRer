# pico balloon WSPR tracker

This code is forked from Roman Piksaykin's excellent work at https://github.com/RPiks/pico-WSPR-tx  

I fixed a couple bugs and added some extra functionality. This version calculates the altitude and full 6 character maidenhead grid based on the GPS data and transmits it along with the callsign. It uses the wspr "Type 3" message format to send the full 6 character grid, and encodes the altitude into the power field (like a Zachtek).

This allows a Raspberry Pi Pico to function as a pico-balloon WSPR tracker, with the only other required hardware being a cheap GPS module such as ATGM336H.

With the original code the Pico was being overclocked to 270Mhz, so the total power draw of the Pico and GPS module was around 100mA at 4 volts. But this version I have the speed down to 135Mhz, which is fine for transmitting on 20M (14Mhz). At this speed the pico/GPS combo draws less than 80mA at 4v.

There is an issue with the RP2040 locking up if its input voltage is raised too gradually. To combat this I put a voltage dividor of two 2k resistors across ground and the input voltage. The output if this voltage divider is tied to the RUN input on Pi Pico. Preliminary testing indicates this to be sufficient.

# Quick-start
1. Install Raspberry Pi Pico SDK. Configure environment variables. Test whether it is built successfully.
2. git clone https://github.com/EngineerGuy314/pico-WSPRer 
4. cd pico-WSPRer
5. modify main.c with your personal callsign on line 73. You can include a single digit suffix
6. ./build.sh
7. Check whether output file ./build/pico-WSPRer.uf2 appears.
8. Load the .uf2 file (2) into the Pico.
9. The operating HF band is 20 meter. different bands can be tried by changing the frequency on line 72 in main.c
10. WSPR type-1 messages will be sent every ten minutes (hh:00, hh:10, ...) and the type-3 message will be sent immediately after
11. if the pico is plugged into a computer via USB while running it will appear as a COM port and diagnostic messages can be viewed at 115200 baud.
12. please excuse my crude programming style :-)

![pico_WSPRer](https://github.com/EngineerGuy314/pico-WSPRer/assets/123671395/bfaad70b-ae55-4695-b1ce-e6d6bb5c9d0f)
