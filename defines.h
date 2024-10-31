///////////////////////////////////////////////////////////////////////////////
//
//  EngineerGuy314 
//  https://github.com/EngineerGuy314/pico-WSPRer
//
///////////////////////////////////////////////////////////////////////////////
//
//  defines.h - Project macros.
// 
//  DESCRIPTION
//      The pico-WSPRer project provides WSPR beacon function using only
//  Pi Pico board. *NO* additional hardware such as freq.synth required.
//  That's why its operation is absolutely time critical and has to be done in
//  fixed point arithmetic defined in macros below
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  PROJECT PAGE
//      https://github.com/EngineerGuy314/pico-WSPRer
//
//  ( ORIGINAL PROJECT PAGE
//      https://github.com/RPiks/pico-WSPR-tx )
//
//  LICENCE
//      MIT License (http://www.opensource.org/licenses/mit-license.php)
//
//  Copyright (c) 2023 by EngineerGuy314
//  
//  Permission is hereby granted, free of charge,to any person obtaining a copy
//  of this software and associated documentation files (the Software), to deal
//  in the Software without restriction,including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY,WHETHER IN AN ACTION OF CONTRACT,TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
///////////////////////////////////////////////////////////////////////////////
#ifndef DEFINES_H
#define DEFINES_H

#define DEBUG

#ifdef DEBUG
#define DEBUGPRINTF(x) StampPrintf(x);
#else
#define DEBUGPRINTF(x) { }
#endif

#define FARENHEIT

#ifdef FARENHEIT
#define tempU (tempC*(9.0f/5.0f))+32
#else
#define tempU tempC
#endif

//////////////// Hardware related defines ////////////////////////////
//#define PLL_SYS_MHZ 270UL//115UL // This sets CPU speed. Roman originally had 270UL. 
      // After improvement od PIODCO we are now on 115 MHz (for 20m band) :-)     

// Serial data from GPS module wired to UART0 RX, GPIO 1 (pin 2 on pico). on custom PCB its uart1, GPIO9. taken care of in main.c

		/* default pin definition when building on a Pico */
#define GPS_PPS_PIN_default 2          /* GPS time mark PIN. (labeled PPS on GPS module)*/ //its not actually PIN 2, its GPIO 2, which is physical pin 4 on pico
#define RFOUT_PIN_default 6            /* RF output PIN. (THE FOLLOWING THREE PINS WILL ALSO BE RF, odd ones 180deg OUT OF PHASE from the even ones!!!) */  //its not actually PIN 6, its GPIO 6, which is physical pin 9 on pico//Pin (RFOUT_PIN+1) will also be RF out (inverted value of first pin)
#define GPS_ENABLE_PIN_default 5       /* GPS_ENABLE pin - high to enable GPS (needs a MOSFET ie 2N7000 on low side drive */    //its not actually PIN 5, its GPIO 5, which is physical pin 7 on pico
#define GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN 10  /* GPS_ENABLE pins, (alternate). GPIO 10 11 and 12, wired in parallel, to directly low-side-drive the GPS module instead of using a MOSFET */	
#define ONEWIRE_bus_pin 27 

		/* pin definitions when using custom PCB board */
#define GPS_PPS_PIN_pcb 17          /* GPS time mark PIN. (labeled PPS on GPS module)*/ //its not actually PIN 2, its GPIO 2, which is physical pin 4 on pico
#define RFOUT_PIN_pcb 18            /* RF output PIN. (THE FOLLOWING PIN WILL ALSO BE RF, 180deg OUT OF PHASE!!!) */  //Pin (RFOUT_PIN+1) will also be RF out (inverted value of first pin)
#define GPS_ENABLE_PIN_pcb 16       /* GPS_ENABLE pin, when using custom PCB. inverse logic */  
#define ONEWIRE_bus_pin_pcb 27 

#define LED_PIN  25 /* 25 for pi pico, 13 for Waveshare rp2040-zero  */
#define PADS_BANK0_GPIO0_SLEWFAST_FAST 1u // value for fast slewrate of pad
#define FLASH_TARGET_OFFSET (256 * 1024) //leaves 256k of space for the program
#define CONFIG_LOCATOR4 "AA22AB"       	       //gets overwritten by gps data anyway       

//////////////// Other defines ////////////////////////////
#define FALSE 0                                     /* Something is false. */
#define TRUE 1                                       /* Something is true. */
#define BAD 0                                         /* Something is bad. */
#define GOOD 1                                       /* Something is good. */
#define INVALID 0                                 /* Something is invalid. */
#define VALID 1                                     /* Something is valid. */
#define NO 0                                          /* The answer is no. */
#define YES 1                                        /* The answer is yes. */
#define OFF 0                                       /* Turn something off. */
#define ON 1                                         /* Turn something on. */
#define ZERO 0                                 /* Something in zero state. */

#define RAM __not_in_flash_func         /* Place time-critical func in RAM */
#define RAM_A __not_in_flash("A")        /* Place time-critical var in RAM */

// ANSI escape codes for color
#define RED "\x1b[91m"
#define BRIGHT "\x1b[97m"
#define NORMAL "\x1b[37m"
#define GREEN "\x1b[32m"
#define YELLOW "\x1b[33m"
#define BLUE "\x1b[34m"
#define RESET "\x1b[0m"
#define CLEAR_SCREEN "\x1b[2J"
#define CURSOR_HOME "\x1b[H"
#define UNDERLINE_ON "\033[4m"
#define UNDERLINE_OFF "\033[24m"
#define BOLD_ON "\033[1m"   
#define BOLD_OFF "\033[0m"

/* A macros for arithmetic right shifts, with casting of the argument. */
#define iSAR32(arg, rcount) (((int32_t)(arg)) >> (rcount))
#define iSAR64(arg, rcount) (((int64_t)(arg)) >> (rcount))

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))

  /* A macros for fast fixed point arithmetics calculations */
#define iMUL32ASM(a,b) __mul_instruction((int32_t)(a), (int32_t)(b))
#define iSquare32ASM(x) (iMUL32ASM((x), (x)))
#define ABS(x) ((x) > 0 ? (x) : -(x))
#define INVERSE(x) ((x) = -(x))
#define asizeof(a) (sizeof (a) / sizeof ((a)[0]))

#define SECOND 1                                                  /* Time. */
#define MINUTE (60 * SECOND)
#define HOUR (60 * MINUTE)

#define kHz 1000UL                                                /* Freq. */
#define MHz 1000000UL

/* WSPR constants definitions */
#define WSPR_FREQ_STEP_MILHZ    2930UL     /* FSK freq.bin (*2 this time). */
#define WSPR_MAX_GPS_DISCONNECT_TM  \
        (6 * HOUR)                      /* How long is active without GPS. */

void read_NVRAM(void);
void write_NVRAM(void);
int check_data_validity(void);
// kevin 10_31_24
// changed to return result like check_data_validity
// void check_data_validity_and_set_defaults(void);
int check_data_validity_and_set_defaults(void);

void user_interface(void);
void show_values(void);
void convertToUpperCase(char *str);
void handle_LED(int led_state);
void InitPicoPins(void);
void display_intro(void);
void I2C_init(void);
void I2C_read(void);
void show_TELEN_msg(void); 
void process_TELEN_data(void); 
void onewire_read(void);
void dallas_setup(void);
void datalog_special_functions(void);
void datalog_loop(void);
void reboot_now(void);
void go_to_sleep(void);
void write_to_next_avail_flash(char *text);
void process_chan_num(void);

#endif
