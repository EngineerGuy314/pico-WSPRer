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
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   
#include "hardware/watchdog.h"
#include "hardware/uart.h"

#define d_force_xmit_for_testing NO

// Serial data from GPS module wired to UART0 RX, GPIO 1 (pin 2 on pico), 
#define GPS_PPS_PIN 2          /* GPS time mark PIN. (labeled PPS on GPS module)*/ //its not actually PIN 2, its GPIO 2, which is physical pin 4 on pico
#define RFOUT_PIN 6            /* RF output PIN. (THE FOLLOWING PIN WILL ALSO BE RF, 180deg OUT OF PHASE!!!) */                                 //its not actually PIN 6, its GPIO 6, which is physical pin 9 on pico
//            Pin (RFOUT_PIN+1) will also be RF out (inverted value of first pin)
#define GPS_ENABLE_PIN 5       /* GPS_ENABLE pin - high to enable GPS (needs a MOSFET ie 2N7000 on low side drive */    //its not actually PIN 5, its GPIO 5, which is physical pin 7 on pico

#define FLASH_TARGET_OFFSET (256 * 1024) //leaves 256k of space for the program
#define CONFIG_LOCATOR4 "AA22AB"       	       //gets overwritten by gps data anyway

WSPRbeaconContext *pWSPR;

char _callsign[7];        //these get set via terminal, and then from NVRAM on boot
char _id13[3];
char _start_minute[2];
char _lane[2];
char _suffix[2];
char _verbosity[2];
char _oscillator[2];

int main()
{
	
    gpio_init(PICO_DEFAULT_LED_PIN); 
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); //initialize LED output
    gpio_init(PICO_VSYS_PIN);  		//Prepare ADC to read Vsys
	gpio_set_dir(PICO_VSYS_PIN, GPIO_IN);
	gpio_set_pulls(PICO_VSYS_PIN,0,0);
    adc_init();
    adc_set_temp_sensor_enabled(true); 	//Enable the onboard temperature sensor
	StampPrintf("\n");
	
	for (int i=0;i < 20;i++)     //do some blinkey on startup, allows time for power supply to stabilize before GPS unit enabled
	{
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(100);
	}

	read_NVRAM();	//reads values of _callsign ... _verbosity from NVRAM
    StampPrintf("pico-WSPRer version: %s %s\n",__DATE__ ,__TIME__);	//messages are sent to USB serial port, 115200 baud
    InitPicoHW();
    PioDco DCO = {0};
	StampPrintf("WSPR beacon init...");
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
    pWB->_txSched.force_xmit_for_testing = d_force_xmit_for_testing;
	pWB->_txSched.led_mode = 0;  //waiting for GPS
	pWB->_txSched.Xmission_In_Process = 0;  //probably not used anymore
	pWB->_txSched.output_number_toEnable_GPS = GPS_ENABLE_PIN;
	pWB->_txSched.verbosity=(uint8_t)_verbosity[0]-'0';       /**< convert ASCI digit to int  */
	pWB->_txSched.suffix=(uint8_t)_suffix[0]-'0';    /**< convert ASCI digit to int (value 253 if dash was entered) */
	pWB->_txSched.oscillatorOff=(uint8_t)_oscillator[0]-'0';
	strcpy(pWB->_txSched.id13,_id13);

	multicore_launch_core1(Core1Entry);
    StampPrintf("RF oscillator initialized.");


	gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output output
	gpio_put(GPS_ENABLE_PIN, 1); 									   // to power up GPS unit
	gpio_init(8); gpio_set_dir(8, GPIO_OUT); //alternate way to enable the GPS is to pull down its ground (aka low-side drive) using 3 GPIO in parallel (no mosfet needed)
	gpio_init(9); gpio_set_dir(9, GPIO_OUT); 
	gpio_init(10); gpio_set_dir(10, GPIO_OUT);
	

    DCO._pGPStime = GPStimeInit(0, 9600, GPS_PPS_PIN); //the 0 defines uart0, so the RX is GPIO 1 (pin 2 on pico). TX to GPS module not needed
    assert_(DCO._pGPStime);
	DCO._pGPStime->user_setup_menu_active=0;
	DCO._pGPStime->forced_XMIT_on=d_force_xmit_for_testing;
	DCO._pGPStime->verbosity=(uint8_t)_verbosity[0]-'0'; 
    int tick = 0;int tick2 = 0;  //used for timing various messages

    for(;;)   //loop every ~ half second
    {
		//GET MAIDENHEAD       - this code in original fork wasnt working due to error in WSPRbeacon.c
        if(WSPRbeaconIsGPSsolutionActive(pWB))
        {
            const char *pgps_qth = WSPRbeaconGetLastQTHLocator(pWB);
            if(pgps_qth)
            {
                strncpy(pWB->_pu8_locator, pgps_qth, 6);     //does full 6 char maidenhead 				
//		        strcpy(pWB->_pu8_locator,"FN12AB");                                                     //DEBUGGING FORCE LOCATOR!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
				
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
		pWB->_txSched.temp_in_Celsius=tempC;           
		DCO._pGPStime->temp_in_Celsius=tempC;

		adc_select_input(3);  //if setup correctly, ADC3 reads Vsys   // read voltage
		float volts = 3*(float)adc_read() * conversionFactor;         //times 3 because thats what it said to do on the internet
			if (volts < 3.00) { volts += 1.95; }			          //wrap around for overflow, per U4B protocol
			if (volts > 4.95) { volts -= 1.95; }
		pWB->_txSched.voltage=volts;

		if (pWB->_txSched.verbosity>=1)
		{
				if(0 == ++tick2 % 4)      //every ~2 sec
				StampPrintf("Temp: %0.1f  Volts: %0.1f  Altitude: %0.0f  Satellite count: %d\n", tempU,volts,DCO._pGPStime->_altitude ,DCO._pGPStime->_time_data.sat_count );
		
		}

			//////////////////////// LED HANDLING /////////////////////////////////////////////////////////
			//orig code had a 900mS pause here, i handle it by waiting for led times
			
		gpio_put(PICO_DEFAULT_LED_PIN, 1); //LED on. how long it stays on depends on "mode"0,1,2 ~= no gps, waiting for slot, xmitting
		if (pWB->_txSched.led_mode==0)
		{
		sleep_ms(20);
		DoLogPrint();
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(440);
		}
		if (pWB->_txSched.led_mode==1)
		{
		sleep_ms(260);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(260);
		}
		if (pWB->_txSched.led_mode==2)
		{
		sleep_ms(440);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(30);
		}
	
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Prints out hex listing of the settings NVRAM to stdio
 * 
 * @param buf Address of NVRAM to list
 * @param len Length of storage to list
 */
void print_buf(const uint8_t *buf, size_t len) {
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
}

/**
 * @brief Function that implements simple user interface via UART
 * 
 */
void user_interface(void)
{
    char c;
	char str[10];

    gpio_put(GPS_ENABLE_PIN, 0);                   //shutoff gps to prevent serial input  (probably not needed anymore due to previous line)
	sleep_ms(100);
	gpio_put(PICO_DEFAULT_LED_PIN, 1); //LED on.	
printf("\n\n\n\n\n\n\n\n\n\n\n\n");
printf("Pico-WSPRer (pico whisper-er) by KC3LBR,  version: %s %s\n",__DATE__ ,__TIME__);
printf("https://github.com/EngineerGuy314/pico-WSPRer\n");
printf("forked from: https://github.com/RPiks/pico-WSPR-tx\n\n");
printf("consult https://traquito.github.io/channelmap/ to find an open channel \nand make note of id13 (column headers), minute and lane (frequency)\n");

show_values();

    for(;;)
	{		
		c=getchar_timeout_us(60000000);		   //just in case user setup menu was enterred during flight, this will reboot after 60 secs
		if (c==255) {printf("\n\n TIMEOUT WAITING FOR INPUT, REBOOTING FOR YOUR OWN GOOD!!");watchdog_enable(100, 1);for(;;)	{}}
		if (c>90) c-=32; //make it capital either way
		switch(c)
		{
			case 'X': printf("\n\nGOODBYE");watchdog_enable(100, 1);for(;;)	{}
			case 'C':printf("Enter callsign: ");		 scanf(" %s", _callsign); _callsign[6]=0;convertToUpperCase(_callsign); write_NVRAM();break;
			case 'S':printf("Enter single digit numeric suffix: "); scanf(" %s", _suffix); _suffix[1]=0; write_NVRAM();break;
			case 'I':printf("Enter id13: ");			 scanf(" %s", _id13);_id13[2]=0; convertToUpperCase(_id13); write_NVRAM();break;
			case 'M':printf("Enter starting Minute: ");  scanf(" %s", _start_minute); _start_minute[1]=0; write_NVRAM(); break;
			case 'L':printf("Enter Lane (1,2,3,4): ");   scanf(" %s", _lane);_lane[1]=0;  write_NVRAM();break;
			case 'V':printf("Verbosity level (0-9): ");   scanf(" %s", _verbosity);_verbosity[1]=0;  write_NVRAM();break;
			case 'O':printf("Oscillator off (0,1): ");   scanf(" %s", _oscillator);_oscillator[1]=0;  write_NVRAM();break;
			case 13:  break;
			case 10:  break;
			default: printf("\n\n\n\n\n\n\nyou pressed %c %02x , INVALID choice!! ",c,c);show_values();break;		
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
printf("\n\nCurrent values:\n\tCallsign:%s\n\tSuffix:%s\n\tId13:%s\n\tMinute:%s\n\tLane:%s\n\tVerbosity:%s\n\tOscillator Off:%s\n\n",_callsign,_suffix,_id13,_start_minute,_lane,_verbosity, _oscillator);
printf("VALID commands: \n\n\tX: eXit configuraiton and reboot\n\tC: change Callsign (6 char max)\n\t");
printf("S: change Suffix (added to callsign for WSPR3) enter '-' to disable WSPR3\n\t");
printf("I: change Id13 (two alpha numeric chars, ie Q8) enter '--' to disable U4B\n\t");
printf("M: change starting Minute (0,2,4,6,8)\n\tL: Lane (1,2,3,4) corresponding to 4 frequencies in 20M band\n\t");
printf("V: Verbosity level (0 for no messages, 9 for too many) \n\tO: Oscillator off after trasmission (0,1)\n\n");

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
