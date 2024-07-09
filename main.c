/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://github.com/RPiks/pico-WSPR-tx
//  PROJECT PAGE
//  https://github.com/EngineerGuy314/pico-WSPRer
///////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/multicore.h"
#include "hf-oscillator/lib/assert.h"
#include "hardware/flash.h"
#include <defines.h>
#include <piodco.h>
#include <WSPRbeacon.h>
#include "debug/logutils.h"
#include <protos.h>
#include <utilities.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/uart.h"

WSPRbeaconContext *pWSPR;

char _callsign[7];        //these get set via terminal, and then from NVRAM on boot
char _id13[3];
char _start_minute[2];
char _lane[2];
char _suffix[2];
char _verbosity[2];
char _oscillator[2];
char _custom_PCB[2];        

static absolute_time_t LED_sequence_start_time;
static int GPS_PPS_PIN;     //these get set based on values in defines.h
static int RFOUT_PIN;
static int GPS_ENABLE_PIN;
int force_transmit = 0;

PioDco DCO = {0};

int main()
{
	InitPicoClock();			// Sets the system clock generator
	StampPrintf("\n");DoLogPrint(); // needed asap to wake up the USB stdio port 
	
	gpio_init(LED_PIN); 
	gpio_set_dir(LED_PIN, GPIO_OUT); //initialize LED output
	
	for (int i=0;i < 20;i++)     //do some blinkey on startup, allows time for power supply to stabilize before GPS unit enabled
	{
        gpio_put(LED_PIN, 1); 
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
		sleep_ms(100);
	}

	read_NVRAM();				//reads values of _callsign ... _verbosity from NVRAM
    InitPicoPins();				// Sets GPIO pins roles and directions and also ADC for voltage and temperature measurements (NVRAM must be read BEFORE this, otherwise dont know how to map IO)

    printf("\npico-WSPRer version: %s %s\nWSPR beacon init...",__DATE__ ,__TIME__);	//messages are sent to USB serial port, 115200 baud
	
	uint32_t XMIT_FREQUENCY;
	switch(_lane[0])                                     //following lines set lane frequencies for 20M u4b operation. The center freuency for Zactkep (wspr 3) xmitions is hard set in WSPRBeacon.c to 14097100UL
		{
			case '1':XMIT_FREQUENCY=14097020UL;break;
			case '2':XMIT_FREQUENCY=14097060UL;break;
			case '3':XMIT_FREQUENCY=14097140UL;break;
			case '4':XMIT_FREQUENCY=14097180UL;break;
			default: XMIT_FREQUENCY=14097100UL;        //in case an invalid lane was read from EEPROM
		}	

   
	WSPRbeaconContext *pWB = WSPRbeaconInit(
        _callsign,/** the Callsign. */
        CONFIG_LOCATOR4,/**< the default QTH locator if GPS isn't used. */
        10,             /**< Tx power, dbm. */
        &DCO,           /**< the PioDCO object. */
        XMIT_FREQUENCY,
        0,           /**< the carrier freq. shift relative to dial freq. */ //not used
        RFOUT_PIN,       /**< RF output GPIO pin. */
		(uint8_t)_start_minute[0]-'0',   /**< convert ASCI digits to ints  */
		(uint8_t)_id13[0]-'0',   
		(uint8_t)_suffix[0]-'0'   
        );
    assert_(pWB);
    pWSPR = pWB;
    pWB->_txSched.force_xmit_for_testing = force_transmit;
	pWB->_txSched.led_mode = 0;  //0 means no serial comms from  GPS (critical fault if it remains that way)
	pWB->_txSched.verbosity=(uint8_t)_verbosity[0]-'0';       /**< convert ASCI digit to int  */
	pWB->_txSched.suffix=(uint8_t)_suffix[0]-'0';    /**< convert ASCI digit to int (value 253 if dash was entered) */
	pWB->_txSched.oscillatorOff=(uint8_t)_oscillator[0]-'0';
	strcpy(pWB->_txSched.id13,_id13);

	multicore_launch_core1(Core1Entry);
    StampPrintf("RF oscillator initialized.");

	int uart_number=(uint8_t)_custom_PCB[0]-'0';  //custom PCB uses Uart 1 if selected, otherwise uart 0
    DCO._pGPStime = GPStimeInit(uart_number, 9600, GPS_PPS_PIN, PLL_SYS_MHZ); //the 0 defines uart0, so the RX is GPIO 1 (pin 2 on pico). TX to GPS module not needed
    assert_(DCO._pGPStime);


	DCO._pGPStime->user_setup_menu_active=0;
	DCO._pGPStime->forced_XMIT_on=force_transmit;
	DCO._pGPStime->verbosity=(uint8_t)_verbosity[0]-'0'; 
    int tick = 0;int tick2 = 0;  //used for timing various messages
	LED_sequence_start_time = get_absolute_time();
	
    for(;;)   //loop every ~ half second
    {		
        if(WSPRbeaconIsGPSsolutionActive(pWB))
        {
            const char *pgps_qth = WSPRbeaconGetLastQTHLocator(pWB);  //GET MAIDENHEAD       - this code in original fork wasnt working due to error in WSPRbeacon.c
            if(pgps_qth)
            {
                strncpy(pWB->_pu8_locator, pgps_qth, 6);     //does full 6 char maidenhead 				
//		        strcpy(pWB->_pu8_locator,"AA1ABC");          //DEBUGGING TO FORCE LOCATOR VALUE				
            }
        }        
        WSPRbeaconTxScheduler(pWB, YES);   
                
		if (pWB->_txSched.verbosity>=5)
		{
				if(0 == ++tick % 20)      //every ~20 secs dumps context.  
				 WSPRbeaconDumpContext(pWB);
		}	

		if (getchar_timeout_us(0)>0)   //looks for input on USB serial port only
		{
		DCO._pGPStime->user_setup_menu_active=1;	
		user_interface();   
		}
		const float conversionFactor = 3.3f / (1 << 12);          //read temperature
		adc_select_input(4);	
		float adc = (float)adc_read() * conversionFactor;
		float tempC = 27.0f - (adc - 0.706f) / 0.001721f;		
		if (tempC < -50) { tempC  += 89; }			          //wrap around for overflow, per U4B protocol
		if (tempC > 39) { tempC  -= 89; }

		pWB->_txSched.temp_in_Celsius=tempC;           
		DCO._pGPStime->temp_in_Celsius=tempC;
		
		adc_select_input(3);  //if setup correctly, ADC3 reads Vsys   // read voltage
		float volts = 3*(float)adc_read() * conversionFactor;         //times 3 because of onboard voltage divider
			if (volts < 3.00) { volts += 1.95; }			          //wrap around for overflow, per U4B protocol
			if (volts > 4.95) { volts -= 1.95; }
		pWB->_txSched.voltage=volts;

		pWB->_txSched.TELEN_val1=rand() % 630000;   //the values  in TELEN_val1 and TELEN_val2 will get sent as TELEN #1 (extended Telemetry) (a third packet in the U4B protocol)
		pWB->_txSched.TELEN_val2=rand() % 153000;	// max values are 630k and 153k
		
		if (pWB->_txSched.verbosity>=1)
		{
				if(0 == ++tick2 % 4)      //every ~2 sec
				StampPrintf("Temp: %0.1f  Volts: %0.1f  Altitude: %0.0f  Satellite count: %d\n", tempU,volts,DCO._pGPStime->_altitude ,DCO._pGPStime->_time_data.sat_count );		
		}
			//////////////////////// LED HANDLING /////////////////////////////////////////////////////////
			//orig code had a 900mS pause here. I only pause a total of 500ms, and spend it polling the time to handle LED state
			/*
			LED MODE:
				0 - no serial comms to GPS module
				1 - No valid GPS, not transmitting
				2 - Valid GPS, waiting for time to transmitt
				3 - Valid GPS, transmitting
				4 - no valid GPS, but (still) transmitting anyway
			x rapid pulses to indicate mode, followed by pause. 0 is special case, continous rapid blink
			*/
		
		for (int i=0;i < 10;i++)
			{
				handle_LED(pWB->_txSched.led_mode);
				sleep_ms(50);
			}
		DoLogPrint(); 	
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
void handle_LED(int led_state)
/**
 * @brief Handles setting LED to display mode.
 * 
 * @param led_state 1,2,3 or 4 to indicate the number of LED pulses. 0 is a special case indicating serial comm failure to GPS
 */
{
 static int tik;
 uint64_t t = absolute_time_diff_us(LED_sequence_start_time, get_absolute_time());
 int i = t / 400000ULL;     //400mS total period of a LED flash

  if (led_state==0) 						//special case indicating serial comm failure to GPS. blink as rapidly as possible 
		  {
			if(0 == ++tik % 2) gpio_put(LED_PIN, 1); else gpio_put(LED_PIN, 0);     //very rapid
		  }
  else
  {
		  if (i<(led_state+1))
				{
				 if(t -(i*400000ULL) < 50000ULL)           //400mS total period of a LED flash, 50mS on pulse duration
							gpio_put(LED_PIN, 1);
				 else 
							gpio_put(LED_PIN, 0);
				}
		  if (t > 2500000ULL) 	LED_sequence_start_time = get_absolute_time();     //resets every 2.5 secs (total repeat length of led sequence).
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Prints out hex listing of the settings NVRAM to stdio
 * 
 * @param buf Address of NVRAM to list
 * @param len Length of storage to list
 */
void print_buf(const uint8_t *buf, size_t len) {
		printf("\nNVRAM dump:\n");
 for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Reads part of the program memory where the user settings are saved
 * prints hexa listing of data and calls function which check data validity
 * 
 */
void read_NVRAM(void)
{
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET); //a pointer to a safe place after the program memory

print_buf(flash_target_contents, FLASH_PAGE_SIZE); //256

strncpy(_callsign, flash_target_contents, 6);
strncpy(_id13, flash_target_contents+6, 2);
strncpy(_start_minute, flash_target_contents+8, 1);
strncpy(_lane, flash_target_contents+9, 1);
strncpy(_suffix, flash_target_contents+10, 1);
strncpy(_verbosity, flash_target_contents+11, 1);
strncpy(_oscillator, flash_target_contents+12, 1);
strncpy(_custom_PCB, flash_target_contents+13, 1);

check_data_validity();
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Checks validity of user settings and if something is wrong, it sets "factory defaults"
 * and writes it back to NVRAM
 * 
 */
void check_data_validity(void)
{
//do some basic plausibility checking on data							
	if ( ((_callsign[0]<'A') || (_callsign[0]>'Z')) && ((_callsign[0]<'0') || (_callsign[0]>'9'))    ) {   strncpy(_callsign,"ABC123",6);     ; write_NVRAM();} 
	if ( (_suffix[0]<'0') || (_suffix[0]>'9')) {_suffix[0]='-'; write_NVRAM();} //by default, disable zachtek suffix
	if ( (_id13[0]!='0') && (_id13[0]!='1') && (_id13[0]!='Q')&& (_id13[0]!='-')) {strncpy(_id13,"Q0",2); write_NVRAM();}
	if ( (_start_minute[0]!='0') && (_start_minute[0]!='2') && (_start_minute[0]!='4')&& (_start_minute[0]!='6')&& (_start_minute[0]!='8')) {_start_minute[0]='0'; write_NVRAM();}
	if ( (_lane[0]!='1') && (_lane[0]!='2') && (_lane[0]!='3')&& (_lane[0]!='4')) {_lane[0]='2'; write_NVRAM();}
	if ( (_verbosity[0]<'0') || (_verbosity[0]>'9')) {_verbosity[0]='1'; write_NVRAM();} //set default verbosity to 1
	if ( (_oscillator[0]<'0') || (_oscillator[0]>'1')) {_oscillator[0]='1'; write_NVRAM();} //set default oscillator to switch off after the trasmission
	if ( (_custom_PCB[0]<'0') || (_custom_PCB[0]>'1')) {_custom_PCB[0]='0'; write_NVRAM();} 
}

/**
 * @brief Function that implements simple user interface via UART
 * 
 */
void user_interface(void)
{
    char c;
	char str[10];

gpio_put(GPS_ENABLE_PIN, 0);                   //shutoff gps to prevent serial input  (probably not needed anymore)
sleep_ms(100);
gpio_put(LED_PIN, 1); //LED on.	

printf("\n\n\n\n\n\n\n\n\n\n\n\n");
printf("Pico-WSPRer (pico whisper-er) by KC3LBR,  version: %s %s\n",__DATE__ ,__TIME__);
printf("https://github.com/EngineerGuy314/pico-WSPRer\n");
printf("forked from: https://github.com/RPiks/pico-WSPR-tx\n\n");
printf("additional functionality, fixes and documention added by https://github.com/serych\n\n");
printf("consult https://traquito.github.io/channelmap/ to find an open channel \nand make note of id13 (column headers), minute and lane (frequency)\n");

show_values();

    for(;;)
	{	
		printf("Enter the command (X,C,S,I,M,L,V,O,P,T): ");	
		c=getchar_timeout_us(60000000);		   //just in case user setup menu was enterred during flight, this will reboot after 60 secs
		printf("%c\n", c);
		if (c==255) {printf("\n\n TIMEOUT WAITING FOR INPUT, REBOOTING FOR YOUR OWN GOOD!!");sleep_ms(100);watchdog_enable(100, 1);for(;;)	{}}
		if (c>90) c-=32; //make it capital either way
		switch(c)
		{
			case 'X': printf("\n\nGOODBYE");watchdog_enable(100, 1);for(;;)	{}
			case 'C':get_user_input("Enter callsign: ",_callsign,sizeof(_callsign)); convertToUpperCase(_callsign); write_NVRAM(); break;
			case 'S':get_user_input("Enter single digit numeric suffix: ", _suffix, sizeof(_suffix)); write_NVRAM(); break;
			case 'I':get_user_input("Enter id13: ", _id13,sizeof(_id13)); convertToUpperCase(_id13); write_NVRAM(); break;
			case 'M':get_user_input("Enter starting Minute: ", _start_minute, sizeof(_start_minute)); write_NVRAM(); break;
			case 'L':get_user_input("Enter Lane (1,2,3,4): ", _lane, sizeof(_lane)); write_NVRAM(); break;
			case 'V':get_user_input("Verbosity level (0-9): ", _verbosity, sizeof(_verbosity)); write_NVRAM(); break;
			case 'O':get_user_input("Oscillator off (0,1): ", _oscillator, sizeof(_oscillator)); write_NVRAM(); break;
			case 'P':get_user_input("custom Pcb mode (0,1): ", _custom_PCB, sizeof(_custom_PCB)); write_NVRAM(); break;
			case 'T':
				printf("Antenna tuning mode. Enter frequency (for example 14.097) or 0 for exit.\n\t");
				char _tuning_freq[7];
				float frequency;
				while(1)
				{
					get_user_input("Frequency to generate (MHz): ", _tuning_freq, sizeof(_tuning_freq));  //blocking until next input
					frequency = atof(_tuning_freq);
					if (!frequency) {break;}
					printf("Generating %.3f MHz\n", frequency);
					pWSPR->_pTX->_u32_dialfreqhz = (uint32_t)frequency * MHZ;
					pWSPR->_txSched.force_xmit_for_testing = YES;
					return;  // returns to main loop
				}
			case 13:  break;
			case 10:  break;
			default: printf("\nYou pressed: %c - 0x%02x , INVALID choice!! ",c,c);sleep_ms(2000);break;		
		}
		check_data_validity();
		show_values();
	}
}

/**
 * @brief Function that writes out the current set values of parameters
 * 
 */
void show_values(void)
{
printf("\n\nCurrent values:\n\tCallsign:%s\n\tSuffix:%s\n\tId13:%s\n\tMinute:%s\n\tLane:%s\n\tVerbosity:%s\n\tOscillator Off:%s\n\tcustom Pcb IO mappings:%s\n\n",_callsign,_suffix,_id13,_start_minute,_lane,_verbosity,_oscillator,_custom_PCB);
printf("VALID commands: \n\n\tX: eXit configuraiton and reboot\n\tC: change Callsign (6 char max)\n\t");
printf("S: change Suffix (added to callsign for WSPR3) enter '-' to disable WSPR3\n\t");
printf("I: change Id13 (two alpha numeric chars, ie Q8) enter '--' to disable U4B\n\t");
printf("M: change starting Minute (0,2,4,6,8)\n\tL: Lane (1,2,3,4) corresponding to 4 frequencies in 20M band\n\t");
printf("V: Verbosity level (0 for no messages, 9 for too many) \n\tO: Oscillator off after trasmission (0,1) \n\tP: custom Pcb mode IO mappings (0,1)\n\tT: antenna Tuning mode (freq)\n");

}
/**
 * @brief Writes the user entered data into NVRAM
 * 
 */
void write_NVRAM(void)
{
    uint8_t data_chunk[FLASH_PAGE_SIZE];

	strncpy(data_chunk,_callsign, 6);
	strncpy(data_chunk+6,_id13,  2);
	strncpy(data_chunk+8,_start_minute, 1);
	strncpy(data_chunk+9,_lane, 1);
	strncpy(data_chunk+10,_suffix, 1);
	strncpy(data_chunk+11,_verbosity, 1);
	strncpy(data_chunk+12,_oscillator, 1);
	strncpy(data_chunk+13,_custom_PCB, 1);

	uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_TARGET_OFFSET, data_chunk, FLASH_PAGE_SIZE);
	restore_interrupts (ints);

}
/**
 * @brief Converts string to upper case
 * 
 * @param str string to convert
 * @return No return value, string is converted directly in the parameter *str  
 */
void convertToUpperCase(char *str) {
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}
/**
 * @brief Initializes Pico pins
 * 
 */
void InitPicoPins(void)
{
/*  gpio_init(18); 
	gpio_set_dir(18, GPIO_OUT); //GPIO 18 used for fan control when testing TCXO stability */

	int use_custom_PCB_mappings=(uint8_t)_custom_PCB[0]-'0'; 
	if (use_custom_PCB_mappings==0)                            //do not use parallel IO low-side drive if using custom PCB
	{		
	GPS_PPS_PIN = GPS_PPS_PIN_default;
	RFOUT_PIN = RFOUT_PIN_default;
	GPS_ENABLE_PIN = GPS_ENABLE_PIN_default;
	gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output 
	gpio_put(GPS_ENABLE_PIN, 1);   //turn on output to enable the MOSFET
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN, GPIO_OUT); //alternate way to enable the GPS is to pull down its ground (aka low-side drive) using 3 GPIO in parallel (no mosfet needed). 2 do: make these non-hardcoded
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+1); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+1, GPIO_OUT); //no need to actually write a value to these outputs. Just enabling them as outputs is fine, they default to the off state when this is done. perhaps thats a dangerous assumption? 
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+2); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+2, GPIO_OUT);
	}
		else                          //if using custom PCB
		{	
			GPS_PPS_PIN = GPS_PPS_PIN_pcb;
			RFOUT_PIN = RFOUT_PIN_pcb;
			GPS_ENABLE_PIN = GPS_ENABLE_PIN_pcb;
			gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output (INVERSE LOGIC on custom PCB, so just initialize it, leave it at zero state)	

			gpio_init(6); gpio_set_dir(6, GPIO_OUT);gpio_put(6, 1); //these are required ONLY for v0.1 of custom PCB (ON/OFF and nReset of GPS module, which later are just left disconnected)
			gpio_init(5); gpio_set_dir(5, GPIO_OUT);gpio_put(5, 1); //these are required ONLY for v0.1 of custom PCB (ON/OFF and nReset of GPS module, which later are just left disconnected)

	}

	gpio_init(PICO_VSYS_PIN);  		//Prepare ADC to read Vsys
	gpio_set_dir(PICO_VSYS_PIN, GPIO_IN);
	gpio_set_pulls(PICO_VSYS_PIN,0,0);
    adc_init();
    adc_set_temp_sensor_enabled(true); 	//Enable the onboard temperature sensor


    // RF pins are initialised in /hf-oscillator/dco2.pio. Here is only pads setting
    // trying to set the power of RF pads to maximum and slew rate to fast (Chapter 2.19.6.3. Pad Control - User Bank in the RP2040 datasheet)
    // possible values: PADS_BANK0_GPIO0_DRIVE_VALUE_12MA, ..._8MA, ..._4MA, ..._2MA
    // values of constants are the same for all the pins, so doesn't matter if we use PADS_BANK0_GPIO6_DRIVE_VALUE_12MA or ..._GPIO0_DRIVE...
    /*  Measurements have shown that the drive value and slew rate settings do not affect the output power. Therefore, the lines are commented out.
    hw_write_masked(&padsbank0_hw->io[RFOUT_PIN],
                (PADS_BANK0_GPIO0_DRIVE_VALUE_12MA << PADS_BANK0_GPIO0_DRIVE_LSB) || PADS_BANK0_GPIO0_SLEWFAST_FAST,
                PADS_BANK0_GPIO0_DRIVE_BITS || PADS_BANK0_GPIO0_SLEWFAST_BITS);           // first RF pin 
    hw_write_masked(&padsbank0_hw->io[RFOUT_PIN+1],
                (PADS_BANK0_GPIO0_DRIVE_VALUE_12MA << PADS_BANK0_GPIO0_DRIVE_LSB) || PADS_BANK0_GPIO0_SLEWFAST_FAST,
                PADS_BANK0_GPIO0_DRIVE_BITS || PADS_BANK0_GPIO0_SLEWFAST_BITS);           // second RF pin
    */            

}

/**
* @note:
* Verbosity notes:
* 0: none
* 1: temp/volts every second, message if no gps
* 2: GPS status every second
* 3:          messages when a xmition started
* 4: x-tended messages when a xmition started 
* 5: dump context every 20 secs
* 6: show PPB every second
* 7: Display GxRMC and GxGGA messages
* 8: display ALL serial input from GPS module
*/
