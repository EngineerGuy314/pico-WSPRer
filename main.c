/////////////////////////////////////////////////////////////////////////////
//
//  PROJECT PAGE
//  https://github.com/EngineerGuy314/pico-WSPRer
//
//  Much of the code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/multicore.h"
#include "hf-oscillator/lib/assert.h"
#include "hardware/flash.h"
#include <WSPRbeacon.h>
#include <defines.h>
#include <piodco.h>
#include "debug/logutils.h"
#include <protos.h>
#include <math.h>
#include <utilities.h>
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "pico/sleep.h"      
#include "hardware/rtc.h" 
#include "onewire/onewire_library.h"    // onewire library functions
#include "onewire/ow_rom.h"             // onewire ROM command codes
#include "onewire/ds18b20.h"            // ds18b20 function codes


WSPRbeaconContext *pWSPR;
WSPRbeaconContext *pWB;

char _callsign[7];        //these get set via terminal, and then from NVRAM on boot
char _id13[3];
char _start_minute[2];
char _lane[2];
char _suffix[2];
char _verbosity[2];
// this isn't modifiable by user but still checked for correct default value
char _oscillator[2];
char _custom_PCB[2];   
char _TELEN_config[5]; 
char _battery_mode[2];
char _Klock_speed[4];         
char _Datalog_mode[2]; 
char _U4B_chan[4];
//**********
// kevin 10_30_24
char _Band[3]; // string with 10, 12, 15, 17, 20 legal. null at end
//**********

static uint32_t telen_values[4];  //consolodate in an array to make coding easier
static absolute_time_t LED_sequence_start_time;
static int GPS_PPS_PIN;     //these get set based on values in defines.h, and also if custom PCB selected in user menu
static int RFOUT_PIN;
static int GPS_ENABLE_PIN;
int PLL_SYS_MHZ;
uint gpio_for_onewire;
int force_transmit = 0;
uint32_t fader; //for creating "breathing" effect on LED to indicate corruption of NVRAM
uint32_t fade_counter;
int maxdevs = 10;
uint64_t OW_romcodes[10];
float onewire_values[10];
int number_of_onewire_devs;
OW one_wire_interface;   //onewire interface

PioDco DCO = {0};

// 0 should never happen (init_rf_freq will always init from saved nvram/live state)
uint32_t XMIT_FREQUENCY=0;

//*******************************
uint32_t init_rf_freq(void)
{
// kevin 10_30_24
// base frequencies for different bands
// the pico should be able to do up to 10M band with appropriate clock frequency? (250Mhz? or ??)
// 136000, 474200, 1836600, 3568600, 5364700, 7038600, 10138700, 14095600, 18104600, 21094600, 24924600, 28124600, 50293000, 70091000, 144489000};
// will support 20M, 17M, 15M, 12M, 10M

enum BASE_FREQS {
    BF20M=14095600UL,
    BF17M=18104600UL,
    BF15M=21094600UL,
    BF12M=24924600UL,
    BF10M=28124600UL
};

uint32_t BASE_FREQ_USED;
switch(atoi(_Band))
{
    case 20: BASE_FREQ_USED=BF20M; break;
    case 17: BASE_FREQ_USED=BF17M; break;
    case 15: BASE_FREQ_USED=BF15M; break;
    case 12: BASE_FREQ_USED=BF12M; break;
    case 10: BASE_FREQ_USED=BF10M; break;
    default: BASE_FREQ_USED=BF20M; // default to 20M in case of error cases
}

XMIT_FREQUENCY=BASE_FREQ_USED + 1400UL; // offset from base for start of passband. same for all bands

// add offset based on lane ..same for every band
switch(_lane[0])                                     
    // Center frequency for Zachtek (wspr 3) is hard set in WSPRBeacon.c to 14097100UL
    {
        // old code for 20M:
        // case '1':XMIT_FREQUENCY=14097020UL; break;
        // case '2':XMIT_FREQUENCY=14097060UL; break;
        // case '3':XMIT_FREQUENCY=14097140UL; break;
        // case '4':XMIT_FREQUENCY=14097180UL; break;
        // default: XMIT_FREQUENCY=14097100UL; // in case invalid lane was read from EEPROM. This is center passband?? (not a valid lane?)
        // new code:
        case '1':XMIT_FREQUENCY+=20UL;  break;
        case '2':XMIT_FREQUENCY+=60UL;  break;
        case '3':XMIT_FREQUENCY+=140UL; break;
        case '4':XMIT_FREQUENCY+=180UL; break;
        default: XMIT_FREQUENCY+=100UL; // in case invalid lane was read from EEPROM. This is center passband?? (not a valid lane?)
    }	

printf("\nrf_freq_init _Band %s BASE_FREQ_USED %d XMIT_FREQUENCY %d _Klock_speed %s\n", _Band, BASE_FREQ_USED, XMIT_FREQUENCY, _Klock_speed);
    return XMIT_FREQUENCY;
}
//**************************

int main()
{
	StampPrintf("\n");DoLogPrint(); // needed asap to wake up the USB stdio port (because StampPrintf includes stdio_init_all();). why though?
	for (int i=0;i < 20;i++) {printf("*");sleep_ms(100);}			
	gpio_init(LED_PIN); 
	gpio_set_dir(LED_PIN, GPIO_OUT); //initialize LED output
		
	for (int i=0;i < 20;i++)     //do some blinky on startup, allows time for power supply to stabilize before GPS unit enabled
	{
        gpio_put(LED_PIN, 1); 
        sleep_ms(100);
        gpio_put(LED_PIN, 0);
		sleep_ms(100);
	}
	read_NVRAM(); //reads values of _callsign,  _verbosity etc from NVRAM. MUST READ THESE *BEFORE* InitPicoPins
    //************
    // kevin 10_31_24 ..actually change to defaults if any bad found..to avoid hang cases caused by illegal values
    // after "too high clock" recovery with flash nuke uf2, and clock gets fixed, this will fix other values to legal defaults
    // if (check_data_validity()==-1)  //if data was bad, breathe LED for 10 seconds and reboot. or if user presses a key enter setup
    if (check_data_validity_and_set_defaults()==-1)  //if data was bad (and got fixed!) , breathe LED for 10 seconds and reboot. or if user presses a key enter setup
    //************
	{
	    printf("\nBAD values in NVRAM detected! will reboot in 10 seconds... press any key to enter user-setup menu..\n");
	    fader=0;fade_counter=0;
        while (getchar_timeout_us(0)==PICO_ERROR_TIMEOUT) //looks for input on USB serial port only @#$%^&!! they changed this function in SDK 2.0!. used to use -1 for no input, now its -2 PICO_ERROR_TIMEOUT
			{
			 fader+=1;
			 if ((fader%5000)>(fader/100))
			 gpio_put(LED_PIN, 1); 
				else
			 gpio_put(LED_PIN, 0);	
			 if (fader>500000) 
				{
					fader=0;
					fade_counter+=1;
						if (fade_counter>10) {watchdog_enable(100, 1);for(;;)	{} } //after ~10 secs force a reboot
				}
			}	
		DCO._pGPStime->user_setup_menu_active=1;	//if we get here, they pressed a button
		user_interface();  
	}
	process_chan_num(); //sets minute/lane/id from chan number. usually redundant at this point, but can't hurt
	
	if (getchar_timeout_us(0)>0)   //looks for input on USB serial port only. Note: getchar_timeout_us(0) returns a -2 (as of sdk 2) if no keypress. Must do this check BEFORE setting Clock Speed in Case you bricked it
    {
		DCO._pGPStime->user_setup_menu_active=1;	
		user_interface();   
    }
    //***************
    // kevin 10_31_24
	if (InitPicoClock(PLL_SYS_MHZ)==-1) // Tries to set the system clock generator	
    {
        // example of bad _Klock_speed is 205
        printf("FAILED with PLL_SYS_MHZ %d trying to reset _Klock_speed (and NVRAM) to default 115", PLL_SYS_MHZ);
	    strcpy(_Klock_speed,"115"); 
        write_NVRAM();
        PLL_SYS_MHZ = 115;
	    InitPicoClock(PLL_SYS_MHZ);	// This should work now
    }
    // don't have to redo if it passed the first time
    //***************
        
	InitPicoPins();				// Sets GPIO pins roles and directions and also ADC for voltage and temperature measurements (NVRAM must be read BEFORE this, otherwise dont know how to map IO)
	I2C_init();
    printf("\nThe pico-WSPRer version: %s %s\nWSPR beacon init...",__DATE__ ,__TIME__);	//messages are sent to USB serial port, 115200 baud

    //*******************************
    // kevin 10_31_24
    uint32_t XMIT_FREQUENCY;
    XMIT_FREQUENCY = init_rf_freq();

    pWB = WSPRbeaconInit(
        _callsign,/** the Callsign. */
        CONFIG_LOCATOR4,/**< the default QTH locator if GPS isn't used. */
        10,             /**< Tx power, dbm. */
        &DCO,           /**< the PioDCO object. */
        XMIT_FREQUENCY,
        0,           /**< the carrier freq. shift relative to dial freq. */ //not used
        RFOUT_PIN,       /**< RF output GPIO pin. */
		(uint8_t)_start_minute[0]-'0',   /**< convert ASCI digits to ints  */
		(uint8_t)_id13[0]-'0',   (uint8_t)_suffix[0]-'0',
		_TELEN_config		
        );

    assert_(pWB);
    pWSPR = pWB;  //this lets things outside this routine access the WB context
    pWB->_txSched.force_xmit_for_testing = force_transmit;
	pWB->_txSched.led_mode = 0;  //0 means no serial comms from  GPS (critical fault if it remains that way)
	pWB->_txSched.verbosity=(uint8_t)_verbosity[0]-'0';       /**< convert ASCI digit to int  */
	pWB->_txSched.suffix=(uint8_t)_suffix[0]-'0';    /**< convert ASCI digit to int (value 253 if dash was entered) */
	pWB->_txSched.oscillatorOff=(uint8_t)_oscillator[0]-'0';
	pWB->_txSched.low_power_mode=(uint8_t)_battery_mode[0]-'0';
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
	if (_Datalog_mode[0]=='1') datalog_loop();
	
    absolute_time_t loop_us_start;
    absolute_time_t loop_us_end;
    int64_t loop_us_elapsed;
    int64_t loop_ms_elapsed;

    // copied from loop_us_end while in the loop (at bottom)
    loop_us_start = get_absolute_time();
    for(;;)   //loop every ~ half second
    {		

		onewire_read();
		I2C_read();
		
		if(WSPRbeaconIsGPSsolutionActive(pWB))
        {
            const char *pgps_qth = WSPRbeaconGetLastQTHLocator(pWB);  //GET MAIDENHEAD - this code in original fork wasnt working due to error in WSPRbeacon.c
            if(pgps_qth)
            {
                strncpy(pWB->_pu8_locator, pgps_qth, 6); //does full 6 char maidenhead 				
                // strcpy(pWB->_pu8_locator,"AA1ABC");   //DEBUGGING TO FORCE LOCATOR VALUE				
            }
        }        
        WSPRbeaconTxScheduler(pWB, YES, GPS_PPS_PIN);   
                
        tick = tick + 1;
		if (pWB->_txSched.verbosity>=5)
		{
            if(0==(tick % 20)) // every ~20 * 0.5 = 10 secs
            {
                WSPRbeaconDumpContext(pWB);
                //****************
                // kevin 10_30_24
                StampPrintf("\n_Band %s _lane (u4b freq bin) %s XMIT_FREQUENCY %d\n", _Band, _lane[0], XMIT_FREQUENCY);
                //****************
            }
        }	

		if (getchar_timeout_us(0)>0)   //looks for input on USB serial port only. Note: getchar_timeout_us(0) returns a -2 (as of sdk 2) if no keypress. But if you force it into a Char type, becomes something else
		{
		DCO._pGPStime->user_setup_menu_active=1;	
		user_interface();   
		}

		const float conversionFactor = 3.3f / (1 << 12);          //read temperature
		adc_select_input(4);	
		float adc = (float)adc_read() * conversionFactor;
		float tempC = 27.0f - (adc - 0.706f) / 0.001721f;		
        
        // don't really want this? modulo arith is done before using for wspr encoding
		// if (tempC < -50) { tempC  += 89; }			          //wrap around for overflow, per U4B protocol
		// if (tempC > 39) { tempC  -= 89; }
        // should be 1 deg C granularity..but maybe leave it with more precision for seeing small temp changes in the stdout
		pWB->_txSched.temp_in_Celsius=tempC;           
		DCO._pGPStime->temp_in_Celsius=tempC;
		
		adc_select_input(3);  //if setup correctly, ADC3 reads Vsys   // read voltage
		float volts_adc = 3*(float)adc_read() * conversionFactor;     // times 3 because of onboard voltage divider
        //*****************
        // kevin 10_30_24
        // make volts just valid multiple of 0.05 (aligns with u4b granularity)
        int v = volts_adc * 100; 
        // floor divide because int
        v = (v / 5) * 5;
        float volts = v / 100;
        //*****************
        // don't really want this? modulo arith is done before using for wspr encoding
        // if (volts < 3.00) { volts += 1.95; } //wrap around for overflow, per U4B protocol
        // if (volts > 4.95) { volts -= 1.95; }
		pWB->_txSched.voltage=volts;

 		process_TELEN_data();                          //if needed, this puts data into TELEN variables. You can remove this and set the data yourself as shown in the next two lines
		//pWB->_txSched.TELEN1_val1=rand() % 630000;   //the values  in TELEN_val1 and TELEN_val2 will get sent as TELEN #1 (extended Telemetry) (a third packet in the U4B protocol)
		//pWB->_txSched.TELEN1_val2=rand() % 153000;	//max values are 630k and 153k
		
				
        if(0==(tick % 10)) // every ~10 * 0.5 = 5 secs
        {
            //********************
            // kevin 10_30_24 changed volts to 2 digits of precision (to see 0.05 0.10 0.15 etc)
            // only valid 0.05 increments forced above
            // should temp be aligned to single digit degrees above?..
            // maybe allow more precision in stdout compared to telemetry

            // example stdout running on usb. this will print 5.00v, 
            // 00d00:00:40.599513 [0065] Temp: 86.0  Volts: 5.00  Altitude: 2368  Satellite count: 13
            // but: telemetry will wrap and say 3.00v when decoded.  
            // 4.95v is max no-wrap report for u4b/traquito 3-4.95v range assumption)

            // grab/save grid to see if 2d gps changes..only valid fixes?
            char pWB_grid6[7] = "------";
		    // if(WSPRbeaconIsGPSsolutionActive(pWB))
            // FIX! should I just look at pWB data all the time? doesn't matter if gps fix is realtime good fix?
            if (DCO._pGPStime->_time_data._u8_is_solution_active)
            {
                strncpy( pWB_grid6, pWB->_pu8_locator, 6);
                pWB_grid6[6] = 0; // null term
            }

            // FIX! in WSPRbeacon.c...why isn't altitude captured when grid is captured, for the two transmissions. Or is it?
            // why is this grabbed from gpstime.c (altitude_snapshot) rather than capturing it in pWB at the same time as grid?
            // this is a live altitude which can change during transmission?
            if (pWB->_txSched.verbosity>=1) 
                StampPrintf("Temp: %.1f  Volts: %0.2f  Altitude: %0.0f  Satellite count: %d pWB_grid6: %s\n", tempU,volts,DCO._pGPStime->_altitude, DCO._pGPStime->_time_data.sat_count, pWB_grid6);

            //********************
            if (pWB->_txSched.verbosity>=3) 
                printf("TELEN Vals 1 through 4:  %d %d %d %d\n",telen_values[0],telen_values[1],telen_values[2],telen_values[3]);
        }
		
		for (int i=0;i < 10;i++) //orig code had a 900mS pause here. I only pause a total of 500ms, and spend it polling the time to handle LED state
			{
				handle_LED(pWB->_txSched.led_mode); 
				sleep_ms(50); 
			}
		DoLogPrint(); 	
        //***************
        // kevin 10_31_24 FIX! put in a conditional delay that depends on clock frequency
        // faster cpu clock will want more delay? (won't affect the PIO block doing RF)
        // time the loop

        // static uint64_t to_us_since_boot	( absolute_time_t t	)	
        // convert an absolute_time_t into a number of microseconds since boot.
        //****************
        
        loop_us_end = get_absolute_time();
        loop_us_elapsed = absolute_time_diff_us(loop_us_start, loop_us_end);
        // floor divide to get milliseconds
        loop_ms_elapsed = loop_us_elapsed / 1000ULL;

		if (pWB->_txSched.verbosity>=5)
		{
            if(0==(tick % 20)) // every ~20 * 0.5 = 10 secs
            {
                StampPrintf("main/20: _Band %s loop_ms_elapsed: %d millisecs loop_us_start: %llu microsecs loop_us_end: %llu microsecs", _Band, loop_ms_elapsed, loop_us_start, loop_us_end);
            }
        }	

        // next start is this end
        loop_us_start = loop_us_end;
        // will always 0 or greater? (unless bug with time)
        /*
        if ((loop_ms_elapsed < 500) && (loop_ms_elapsed > 0)) {
	        sleep_ms(500 - loop_ms_elapsed);
            
        }
        */
        //****************
	}
}
///////////////////////////////////
static void sleep_callback(void) {
    printf("RTC woke us up\n");
}

void process_TELEN_data(void)
{
		const float conversionFactor = 3300.0f / (1 << 12);   //3.3 * 1000. the 3.3 is from vref, the 1000 is to convert to mV. the 12 bit shift is because thats resolution of ADC

		for (int i=0;i < 4;i++)
		{	 		
		   switch(_TELEN_config[i])
			{
				case '-':  break; //do nothing, telen chan is disabled
				case '0': adc_select_input(0); telen_values[i] = round((float)adc_read() * conversionFactor);  		  break;
				case '1': adc_select_input(1); telen_values[i] = round((float)adc_read() * conversionFactor);  		  break;
				case '2': adc_select_input(2); telen_values[i] = round((float)adc_read() * conversionFactor); 		  break;
				case '3': adc_select_input(3); telen_values[i] = round((float)adc_read() * conversionFactor * 3.0f);  break;  //since ADC3 is hardwired to Battery via 3:1 voltage devider, make the conversion here
				case '4':					   telen_values[i] = pWSPR->_txSched.minutes_since_boot;   				  break; 			
				case '5': 	 				   telen_values[i] = pWSPR->_txSched.minutes_since_GPS_aquisition;		  break;			
				case '6': 	if (onewire_values[_TELEN_config[i]-'6']>0)     telen_values[i] = onewire_values[_TELEN_config[i]-'6']*100; else	telen_values[i] = 20000 + (-1*onewire_values[_TELEN_config[i]-'6'])*100;	  break;	
				case '7': 	if (onewire_values[_TELEN_config[i]-'6']>0)     telen_values[i] = onewire_values[_TELEN_config[i]-'6']*100; else	telen_values[i] = 20000 + (-1*onewire_values[_TELEN_config[i]-'6'])*100;	  break;	
				case '8': 	if (onewire_values[_TELEN_config[i]-'6']>0)     telen_values[i] = onewire_values[_TELEN_config[i]-'6']*100; else	telen_values[i] = 20000 + (-1*onewire_values[_TELEN_config[i]-'6'])*100;	  break;	
				case '9': 	if (onewire_values[_TELEN_config[i]-'6']>0)     telen_values[i] = onewire_values[_TELEN_config[i]-'6']*100; else	telen_values[i] = 20000 + (-1*onewire_values[_TELEN_config[i]-'6'])*100;	  break;				
			}	
		}

//onewire_values		
		pWSPR->_txSched.TELEN1_val1=telen_values[0];   // will get sent as TELEN #1 (extended Telemetry) (a third packet in the U4B protocol)
		pWSPR->_txSched.TELEN1_val2=telen_values[1];	// max values are 630k and 153k for val and val2
		pWSPR->_txSched.TELEN2_val1=telen_values[2];   //will get sent as TELEN #2 (extended Telemetry) (a 4th packet in the U4B protocol)
		pWSPR->_txSched.TELEN2_val2=telen_values[3];	// max values are 630k and 153k for val and val2

}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void handle_LED(int led_state)
/**
 * @brief Handles setting LED to display mode.
 * 
 * @param led_state 1,2,3 or 4 to indicate the number of LED pulses. 0 is a special case indicating serial comm failure to GPS
 */
 			//////////////////////// LED HANDLING /////////////////////////////////////////////////////////
			
			/*
			LED MODE:
				0 - no serial comms to GPS module
				1 - No valid GPS, not transmitting
				2 - Valid GPS, waiting for time to transmitt
				3 - Valid GPS, transmitting
				4 - no valid GPS, but (still) transmitting anyway
			x rapid pulses to indicate mode, followed by pause. 0 is special case, continous rapid blink
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

	printf(CLEAR_SCREEN);printf(BRIGHT);
	printf(BOLD_ON);printf(UNDERLINE_ON);
	printf("\nNVRAM dump: \n");printf(BOLD_OFF); printf(UNDERLINE_OFF);
 for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
    }
	printf(NORMAL);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
void display_intro(void)
{
printf(CLEAR_SCREEN);
printf(CURSOR_HOME);
printf(BRIGHT);
printf("\n\n\n\n\n\n\n\n\n\n\n\n");
printf("================================================================================\n\n");printf(UNDERLINE_ON);
printf("Pico-WSPRer (pico whisper-er) by KC3LBR,  version: %s %s\n\n",__DATE__ ,__TIME__);printf(UNDERLINE_OFF);
printf("Instructions and source: https://github.com/EngineerGuy314/pico-WSPRer\n");
printf("Forked from: https://github.com/RPiks/pico-WSPR-tx\n");
printf("Additional functions, fixes and documention by https://github.com/serych\n\n");
printf("Consult https://traquito.github.io/channelmap/ to find an open channel \nand make note of id13 (column headers), minute and lane (frequency)\n");
printf("\n================================================================================\n");

printf(RED);printf("press anykey to continue");printf(NORMAL); 
char c=getchar_timeout_us(60000000);	//wait 
printf(CLEAR_SCREEN);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void show_TELEN_msg()
{
printf(BRIGHT);
printf("\n\n\n\n");printf(UNDERLINE_ON);
printf("TELEN CONFIG INSTRUCTIONS:\n\n");printf(UNDERLINE_OFF);
printf(NORMAL); 
printf("* There are 4 possible TELEN values, corresponding to TELEN 1 value 1,\n");
printf("  TELEN 1 value 2, TELEN 2 value 1 and TELEN 2 value 2.\n");
printf("* Enter 4 characters (legal 0-9 or -) in TELEN_config. use a '-' (minus) to disable one \n");
printf("  or more values.\n* example: '----' disables all telen \n");
printf("* example: '01--' sets Telen 1 value 1 to type 0, \n  Telen 1 val 2 to type 1,  disables all of TELEN 2 \n"); printf(BRIGHT);printf(UNDERLINE_ON);
printf("\nTelen Types:\n\n");printf(UNDERLINE_OFF);printf(NORMAL); 
printf("-: disabled, 0: ADC0, 1: ADC1, 2: ADC2, 3: ADC3,\n");
printf("4: minutes since boot, 5: minutes since GPS fix aquired \n");
printf("6-9: OneWire temperature sensors 1 though 4 \n");
printf("A: custom: OneWire temperature sensor 1 hourly low/high \n");
printf("B-Z: reserved for Future: I2C devices, other modes etc \n");
printf("\n(ADC values come through in units of mV)\n");
printf("See the Wiki for more info.\n\n");
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Function that implements simple user interface via UART
 * 
 * For every new config variable to be added to the interface:
	1: create a global character array at top of main.c 
	2: add entry in read_NVRAM()
	3: add entry in write_NVRAM()
	4: add limit checking in check_data_validity()
	5: add limit checking in check_data_validity_and_set_defaults()
	6: add TWO entries in show_values() (to display name and value, and also to display which key is used to change it)
	7: add CASE statement entry in user_interface()
	8: Either do something with the variable locally in Main.c, or if needed elsewhere:
		-- add a member to the GPStimeContext or WSPRbeaconContext structure
		-- add code in main.c to move the data from the local _tag to the context structure
		-- do something with the data elsewhere in the program
 */
void user_interface(void)                                //called if keystroke from terminal on USB detected during operation.
{
int c;
char str[10];

gpio_put(GPS_ENABLE_PIN, 0);                   //shutoff gps to prevent serial input  (probably not needed anymore)
sleep_ms(100);
gpio_put(LED_PIN, 1); //LED on.	

display_intro();
show_values();          /* shows current VALUES  AND list of Valid Commands */

    for(;;)
	{	
																 printf(UNDERLINE_ON);printf(BRIGHT);
		// printf("\nEnter the command (X,C,S,U,[I,M,L],V,P,T,B,D,K,F):");printf(UNDERLINE_OFF);printf(NORMAL);	
        //******
        // kevin 10_30_24
		printf("\nEnter the command (X,C,S,U,[I,M,L],V,P,T,B,D,K,F,A):");printf(UNDERLINE_OFF);printf(NORMAL);	
        //
		c=getchar_timeout_us(60000000);		   //just in case user setup menu was enterred during flight, this will reboot after 60 secs
		printf("%c\n", c);
		if (c==PICO_ERROR_TIMEOUT) {printf(CLEAR_SCREEN);printf("\n\n TIMEOUT WAITING FOR INPUT, REBOOTING FOR YOUR OWN GOOD!\n");sleep_ms(100);watchdog_enable(100, 1);for(;;)	{}}
		if (c>90) c-=32; //make it capital either way
		switch(c)
		{
			case 'X':printf(CLEAR_SCREEN);printf("\n\nGOODBYE");watchdog_enable(100, 1);for(;;)	{}
			//case 'R':printf(CLEAR_SCREEN);printf("\n\nCorrupting data..");strncpy(_callsign,"!^&*(",6);write_NVRAM();watchdog_enable(100, 1);for(;;)	{}  //used for testing NVRAM check on boot feature
			case 'C':get_user_input("Enter callsign: ",_callsign,sizeof(_callsign)); convertToUpperCase(_callsign); write_NVRAM(); break;
			case 'S':get_user_input("Enter single digit numeric suffix: ", _suffix, sizeof(_suffix)); convertToUpperCase(_suffix); write_NVRAM(); break;
			case 'U':get_user_input("Enter U4B channel: ", _U4B_chan, sizeof(_U4B_chan)); process_chan_num(); write_NVRAM(); break;
			case 'I':get_user_input("Enter id13: ", _id13,sizeof(_id13)); convertToUpperCase(_id13); write_NVRAM(); break; //still possible but not listed or recommended
			case 'M':get_user_input("Enter starting Minute: ", _start_minute, sizeof(_start_minute)); write_NVRAM(); break; //still possible but not listed or recommended. i suppose needed for when to start standalone beacon or Zachtek
			case 'L':get_user_input("Enter Lane (1,2,3,4): ", _lane, sizeof(_lane)); write_NVRAM(); break; //still possible but not listed or recommended
			case 'V':get_user_input("Verbosity level (0-9): ", _verbosity, sizeof(_verbosity)); write_NVRAM(); break;
            // this isn't modifiable by user but still checked for correct default value
			/*case 'O':get_user_input("Oscillator off (0,1): ", _oscillator, sizeof(_oscillator)); write_NVRAM(); break;*/
			case 'P':get_user_input("custom Pcb mode (0,1): ", _custom_PCB, sizeof(_custom_PCB)); write_NVRAM(); break;
			case 'T':show_TELEN_msg();get_user_input("TELEN config: ", _TELEN_config, sizeof(_TELEN_config)); convertToUpperCase(_TELEN_config); write_NVRAM(); break;
			case 'B':get_user_input("Battery mode (0,1): ", _battery_mode, sizeof(_battery_mode)); write_NVRAM(); break;
			case 'D':get_user_input("Data-log mode (0,1,Wipe,Dump): ", _Datalog_mode, sizeof(_Datalog_mode));
						convertToUpperCase(_Datalog_mode);
						if ((_Datalog_mode[0]=='D') || (_Datalog_mode[0]=='W') ) 
								{
									datalog_special_functions();
									_Datalog_mode[0]='0';
								}						 
							write_NVRAM(); 
						break;

			case 'K':get_user_input("Klock speed (default 115): ", _Klock_speed, sizeof(_Klock_speed)); write_NVRAM(); break;
			case 'F':
				printf("Fixed Frequency output (antenna tuning mode). Enter frequency (for example 14.097) or 0 for exit.\n\t");
				char _tuning_freq[7];
				float frequency;
				while(1)
				{
					get_user_input("Frequency to generate (MHz): ", _tuning_freq, sizeof(_tuning_freq));  //blocking until next input
					frequency = atof(_tuning_freq);
					if (!frequency) {break;}
					printf("Generating %.3f MHz\n", frequency);
					pWSPR->_pTX->_u32_dialfreqhz = (uint32_t)(frequency * MHZ);
					pWSPR->_txSched.force_xmit_for_testing = YES;
					return;  // returns to main loop
				}
            //********************
            // kevin 10_30_24
			case 'A':
                    get_user_input("Enter Band (10,12,15,17,20): ", _Band, sizeof(_Band)); 
                    // redo the channel selection if we change bands, since U4B definition changes per band 
                    process_chan_num(); 
                    write_NVRAM(); 
                    init_rf_freq();
                    XMIT_FREQUENCY = init_rf_freq();
					pWSPR->_pTX->_u32_dialfreqhz = XMIT_FREQUENCY;
                    break;
            //********************
			case 13:  break;
			case 10:  break;
			default: printf(CLEAR_SCREEN); printf("\nYou pressed: %c - (0x%02x), INVALID choice!! ",c,c);sleep_ms(1000);break;		
		}
		int result = check_data_validity_and_set_defaults();
		show_values();
	}
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Reads part of the program memory where the user settings are saved
 * prints hexa listing of data and calls function which check data validity
 * 
 */
void read_NVRAM(void)
{
const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET); //a pointer to a safe place after the program memory

print_buf(flash_target_contents, FLASH_PAGE_SIZE); //256

//***********
// kevin 10_31_24 null terminate in case it's printf'ed with %s
// FIX! shouldn't all of these (all chars) have null terminate?
strncpy(_callsign, flash_target_contents, 6); _callsign[6]=0;
strncpy(_id13, flash_target_contents+6, 2); _id13[2]=0;
strncpy(_start_minute, flash_target_contents+8, 1); _start_minute[1]=0;
strncpy(_lane, flash_target_contents+9, 1); _lane[1]=0;
strncpy(_suffix, flash_target_contents+10, 1); _suffix[1]=0;
strncpy(_verbosity, flash_target_contents+11, 1); _verbosity[1]=0;
// this isn't modifiable by user but still written and checked for default value
strncpy(_oscillator, flash_target_contents+12, 1); _oscillator[1]=0;
strncpy(_custom_PCB, flash_target_contents+13, 1); _custom_PCB[1]=0;
strncpy(_TELEN_config, flash_target_contents+14, 4); _TELEN_config[4]=0;
strncpy(_battery_mode, flash_target_contents+18, 1); _battery_mode[1]=0;
strncpy(_Klock_speed, flash_target_contents+19, 3); _Klock_speed[3]=0; //null terminate cause later will use atoi
PLL_SYS_MHZ =atoi(_Klock_speed); 
strncpy(_Datalog_mode, flash_target_contents+22, 1); _Datalog_mode[1]=0;
strncpy(_U4B_chan, flash_target_contents+23, 3); _U4B_chan[3]=0; //null terminate cause later will use atoi
strncpy(_Band, flash_target_contents+26, 2); _Band[2]=0; //null terminate cause later will use atoi
//***********
 
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Writes the user entered data into NVRAM
 * 
 */
void write_NVRAM(void)
{
    uint8_t data_chunk[FLASH_PAGE_SIZE];  //256 bytes

	strncpy(data_chunk,_callsign, 6);
	strncpy(data_chunk+6,_id13,  2);
	strncpy(data_chunk+8,_start_minute, 1);
	strncpy(data_chunk+9,_lane, 1);
	strncpy(data_chunk+10,_suffix, 1);
	strncpy(data_chunk+11,_verbosity, 1);
    // this isn't modifiable by user but still checked for correct default value
	strncpy(data_chunk+12,_oscillator, 1);
	strncpy(data_chunk+13,_custom_PCB, 1);
	strncpy(data_chunk+14,_TELEN_config, 4);
	strncpy(data_chunk+18,_battery_mode, 1);
	strncpy(data_chunk+19,_Klock_speed, 3);
	strncpy(data_chunk+22,_Datalog_mode, 1);
	strncpy(data_chunk+23,_U4B_chan, 3);
    //****************
    // kevin 10_30_24
	strncpy(data_chunk+26,_Band, 2);
    //****************
	

	uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);  //a "Sector" is 4096 bytes             FLASH_TARGET_OFFSET,FLASH_SECTOR_SIZE,FLASH_PAGE_SIZE = 040000x, 4096, 256
	flash_range_program(FLASH_TARGET_OFFSET, data_chunk, FLASH_PAGE_SIZE);  //writes 256 bytes (one "page") (16 pages per sector)
	restore_interrupts (ints);												//you could theoretically write 16 pages at once (a whole sector)

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Checks validity of user settings and if something is wrong, it sets "factory defaults"
 * and writes it back to NVRAM
 * 
 */

//*************************
// kevin 10_31_24
// int check_data_validity(void)
// create result to return like check_data_validity does
int check_data_validity_and_set_defaults(void)
//*************************
{
    int result=1;	
    //*************************
    // kevin 10_31_24 
    // do some basic plausibility checking on data, set reasonable defaults if memory was uninitialized							
    // or has bad values for some reason
    // create result to return like check_data_validity does
    // FIX! should do full legal callsign check? (including spaces at end)
    // be sure to null terminate so we can print the callsign
	if ( ((_callsign[0]<'A') || (_callsign[0]>'Z')) && ((_callsign[0]<'0') || (_callsign[0]>'9')) ) {strcpy(_callsign,"AB1CDE"); write_NVRAM(); result=-1;} 
    // kevin 10_31_24 didn't have the '-' in the ok check (check_data_availability did)
	if ( ((_suffix[0]<'0') || (_suffix[0]>'9')) && (_suffix[0]!='-') && _suffix[0]!='X') {strcpy(_suffix,"-"); write_NVRAM(); result=-1;} //by default, disable zachtek suffix
    // change to strcpy for null terminate
	if ( (_id13[0]!='0') && (_id13[0]!='1') && (_id13[0]!='Q')&& (_id13[0]!='-')) {strcpy(_id13,"Q0"); write_NVRAM(); result=-1;}
    // no null term added here, but a reboot will reload with null term for all now (see read_NVRAM)
	if ( (_start_minute[0]!='0') && (_start_minute[0]!='2') && (_start_minute[0]!='4')&& (_start_minute[0]!='6')&& (_start_minute[0]!='8')) {strcpy(_start_minute,"0"); write_NVRAM(); result=-1;}
	if ( (_lane[0]!='1') && (_lane[0]!='2') && (_lane[0]!='3')&& (_lane[0]!='4')) {strcpy(_lane,"2"); write_NVRAM(); result=-1;}
	if ( (_verbosity[0]<'0') || (_verbosity[0]>'9')) {strcpy(_verbosity,"1"); write_NVRAM(); result=-1;} //set default verbosity to 1

    // kevin 10_31_24 only allow 1 (oscillator off)..not in menu any more. 0 illegal. cover bad nvram
	if ( (_oscillator[0]<'1') || (_oscillator[0]>'1')) {strcpy(_oscillator,"1"); write_NVRAM(); result=-1;} //set default oscillator to switch off after the transmission
	if ( (_custom_PCB[0]<'0') || (_custom_PCB[0]>'1')) {strcpy(_custom_PCB,"0"); write_NVRAM(); result=-1;} //set default IO mapping to original Pi Pico configuration
    // kevin 10_31_24 check_data_validity() allowed just [0] to be '-' and considered valid
    // 0-9 and - are legal. _
    // make sure to null terminate
	if ( (_TELEN_config[0]<'0' || _TELEN_config[0]>'9') && _TELEN_config[0]!='-') {strcpy(_TELEN_config,"----"); write_NVRAM(); result=-1;}
    // kevin 10_31_24 check the other 3 bytes also?
	if ( (_TELEN_config[1]<'0' || _TELEN_config[1]>'9') && _TELEN_config[1]!='-') {strcpy(_TELEN_config,"----"); write_NVRAM(); result=-1;}
	if ( (_TELEN_config[2]<'0' || _TELEN_config[2]>'9') && _TELEN_config[2]!='-') {strcpy(_TELEN_config,"----"); write_NVRAM(); result=-1;}
	if ( (_TELEN_config[3]<'0' || _TELEN_config[3]>'9') && _TELEN_config[3]!='-')  {strcpy(_TELEN_config,"----"); write_NVRAM(); result=-1;}

	if ( _battery_mode[0]<'0' || _battery_mode[0]>'1') {strcpy(_battery_mode,"0"); write_NVRAM(); result=-1;} //
    
    //*********
    // kevin 10_31_24 . keep the upper limit at 250 to avoid nvram getting
    // a freq that won't work. will have to load flash nuke uf2 to clear nram
    // if that happens, so that default Klock will return?
    // if so: Download the [UF2 file]
    // https://datasheets.raspberrypi.com/soft/flash_nuke.uf2
    // code is
    // https://github.com/raspberrypi/pico-examples/blob/master/flash/nuke/nuke.c
    // may require some iterations of manually setting all the configs by hand 
    // after getting the nuke uf2 (it autoruns) and then reloading pico-WSPRer.uf2
    // hmm. I suppose we could call this routine to fix nvram at the beginning, so if the 
    // clock gets fixed, then the defaults will get fixed (where errors exist)
    // be sure to null terminate
	if ( (atoi(_Klock_speed)<100) || (atoi(_Klock_speed)>250)) {strcpy(_Klock_speed,"115"); write_NVRAM(); result=-1;} 
    //*********
	if ( (_Datalog_mode[0]!='0') && (_Datalog_mode[0]!='1') && (_Datalog_mode[0]!='D') && (_Datalog_mode[0]!='W')) {_Datalog_mode[0]='0'; write_NVRAM(); result=-1;}
    // be sure to null terminate
	if ( (atoi(_U4B_chan)<0) || (atoi(_U4B_chan)>599)) {strcpy(_U4B_chan,"599"); write_NVRAM(); result=-1;} 
    //****************
    // kevin 10_30_24
    switch(atoi(_Band))
    {
        case 10: break;
        case 12: break;
        case 15: break;
        case 17: break;
        case 20: break;
        default: 
            strcpy(_Band,"20"); 
            write_NVRAM(); 
            // figure out the XMIT_FREQUENCY for new band, and set _32_dialfreqhz
            // have to do this whenever we change bands
            XMIT_FREQUENCY = init_rf_freq();
		    pWSPR->_pTX->_u32_dialfreqhz = XMIT_FREQUENCY;
            result=-1;
            break;
    }
    return result;
    //****************
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Checks validity of user settings and returns -1 if something wrong. Does NOT set defaults or alter NVRAM.
 * 
 */
int check_data_validity(void)
{
    int result=1;	
    //do some basic plausibility checking on data				
	if ( ((_callsign[0]<'A') || (_callsign[0]>'Z')) && ((_callsign[0]<'0') || (_callsign[0]>'9')) ) {result=-1;} 
	if ( ((_suffix[0]<'0') || (_suffix[0]>'9')) && (_suffix[0]!='-') && (_suffix[0]!='X') ) {result=-1;} 
	if ( (_id13[0]!='0') && (_id13[0]!='1') && (_id13[0]!='Q')&& (_id13[0]!='-')) {result=-1;}
	if ( (_start_minute[0]!='0') && (_start_minute[0]!='2') && (_start_minute[0]!='4')&& (_start_minute[0]!='6')&& (_start_minute[0]!='8')) {result=-1;}
	if ( (_lane[0]!='1') && (_lane[0]!='2') && (_lane[0]!='3')&& (_lane[0]!='4')) {result=-1;}
	if ( (_verbosity[0]<'0') || (_verbosity[0]>'9')) {result=-1;} 
    // kevin 10_31_24 only allow 1 (oscillator off)..not in menu any more. 0 illegal. cover bad nvram
	if ( (_oscillator[0]<'1') || (_oscillator[0]>'1')) {result=-1;} 
	if ( (_custom_PCB[0]<'0') || (_custom_PCB[0]>'1')) {result=-1;} 
    // kevin 10_31_24 0-9 and - are legal
	if ( (_TELEN_config[0]<'0' || _TELEN_config[0]>'9') && _TELEN_config[0]!='-') {result=-1;}
    // kevin 10_31_24 check the 3 other bytes the same way
	if ( (_TELEN_config[1]<'0' || _TELEN_config[1]>'9') && _TELEN_config[1]!='-') {result=-1;}
	if ( (_TELEN_config[2]<'0' || _TELEN_config[2]>'9') && _TELEN_config[2]!='-') {result=-1;}
	if ( (_TELEN_config[3]<'0' || _TELEN_config[3]>'9') && _TELEN_config[3]!='-') {result=-1;}
	if ( _battery_mode[0]<'0' || _battery_mode[0]>'1') {result=-1;} 	
    // kevin 10_31_24..make 250 the upper limit to avoid wedging the chip with unworkable freq
	if ( (atoi(_Klock_speed)<100) || (atoi(_Klock_speed)>250)) {result=-1;} 	
    // kevin 10_31_24 'D' and 'W' are also valid in check_data_validity_and_set_defaults()
    if ( (_Datalog_mode[0]!='0') && (_Datalog_mode[0]!='1') && (_Datalog_mode[0]!='D') && (_Datalog_mode[0]!='W')) {result=-1;}

	if ( (atoi(_U4B_chan)<0) || (atoi(_U4B_chan)>599)) {result=-1;} 
    //****************
    // kevin 10_31_24
    switch(atoi(_Band))
    {
        case 10: break;
        case 12: break;
        case 15: break;
        case 17: break;
        case 20: break;
        default: result=-1;
    }
    //****************
    return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Function that writes out the current set values of parameters
 * 
 */
void show_values(void) /* shows current VALUES  AND list of Valid Commands */
{
								printf(CLEAR_SCREEN);printf(UNDERLINE_ON);printf(BRIGHT);
printf("\n\nCurrent values:\n");printf(UNDERLINE_OFF);printf(NORMAL);
printf("\n\tCallsign:%s\n\t",_callsign);
printf("Suffix:%s\n\t",_suffix);
printf("U4b channel:%s",_U4B_chan);
printf(" (Id13:%s",_id13);
printf(" Start Minute:%s",_start_minute);
printf(" Lane:%s)\n\t",_lane);
printf("Verbosity:%s\n\t",_verbosity);
// this isn't modifiable by user but still checked for correct default value
/*printf("Oscillator Off:%s\n\t",_oscillator);*/
printf("custom Pcb IO mappings:%s\n\t",_custom_PCB);
printf("TELEN config:%s\n\t",_TELEN_config);
printf("Klock speed:%sMhz  (default: 115)\n\t",_Klock_speed);
printf("Datalog mode:%s\n\t",_Datalog_mode);
//*************
// kevin 10_30_24
printf("Band:%s\n\t",_Band);
printf("XMIT_FREQUENCY:%d\n\t",XMIT_FREQUENCY);
//*************
printf("Battery (low power) mode:%s\n\n",_battery_mode);

							printf(UNDERLINE_ON);printf(BRIGHT);
printf("VALID commands: ");printf(UNDERLINE_OFF);printf(NORMAL);

printf("\n\n\tX: eXit configuraiton and reboot\n\tC: change Callsign (6 char max)\n\t");
printf("S: change Suffix ( for WSPR3/Zachtek) use '-' to disable WSPR3\n\t");
printf("U: change U4b channel # (0-599)\n\t");
printf("A: change bAnd (10,12,15,17,20 default 20)\n\t");
/*printf("I: change Id13 (two alpha numeric chars, ie Q8) use '--' to disable U4B\n\t");
printf("M: change starting Minute (0,2,4,6,8)\n\tL: Lane (1,2,3,4) corresponding to 4 frequencies in 20M band\n\t");*/ //it is still possible to directly change these, but its not shown
printf("V: Verbosity level (0 for no messages, 9 for too many) \n\t");
/*printf("O: Oscillator off after transmission (default: 1) \n\t");*/
printf("P: custom Pcb mode IO mappings (0,1)\n\t");
printf("T: TELEN config\n\t");
printf("K: Klock speed  (default: 115)\n\t");
printf("D: Datalog mode (0,1,(W)ipe memory, (D)ump memory) see wiki\n\t");
printf("B: Battery (low power) mode \n\t");
printf("F: Frequency output (antenna tuning mode)\n\n");


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
	gpio_for_onewire=ONEWIRE_bus_pin;
	}
	
    else                          //if using custom PCB 
    {	
    gpio_for_onewire=ONEWIRE_bus_pin_pcb;
    GPS_PPS_PIN = GPS_PPS_PIN_pcb;
    RFOUT_PIN = RFOUT_PIN_pcb;
    GPS_ENABLE_PIN = GPS_ENABLE_PIN_pcb;
    gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output (INVERSE LOGIC on custom PCB, so just initialize it, leave it at zero state)	

    gpio_init(6); gpio_set_dir(6, GPIO_OUT);gpio_put(6, 1); //these are required ONLY for v0.1 of custom PCB (ON/OFF and nReset of GPS module, which later are just left disconnected)
    gpio_init(5); gpio_set_dir(5, GPIO_OUT);gpio_put(5, 1); //these are required ONLY for v0.1 of custom PCB (ON/OFF and nReset of GPS module, which later are just left disconnected)
	}

    //*****************
    // kevin 11_1_24
    // vary the rf out pins drive strength. use special test callsigns to decide
    // from the pico sdk:
    // enum gpio_drive_strength { GPIO_DRIVE_STRENGTH_2MA = 0, GPIO_DRIVE_STRENGTH_4MA = 1, GPIO_DRIVE_STRENGTH_8MA = 2, GPIO_DRIVE_STRENGTH_12MA = 3 }
    enum gpio_drive_strength drive_strength;
    if (_callsign == "R2MAD")  
        drive_strength = GPIO_DRIVE_STRENGTH_2MA;
    else if (_callsign == "R4MAD")  
        drive_strength = GPIO_DRIVE_STRENGTH_4MA;
    else if (_callsign == "R8MAD")  
        drive_strength = GPIO_DRIVE_STRENGTH_8MA;
    else if (_callsign == "R12MAD")  
        drive_strength = GPIO_DRIVE_STRENGTH_12MA;
    // can have alphanumerc in first 3 chars, alpha in last 3?  space can be in last 3? (telemetry doesn't use spac)e
    else
        // FIX! just force to 2MA always? (except for test cases above)
        // drive_strength = GPIO_DRIVE_STRENGTH_2MA;
        // default drive strength for everything else
        drive_strength = GPIO_DRIVE_STRENGTH_8MA;

    gpio_set_drive_strength(RFOUT_PIN+0, drive_strength);
    gpio_set_drive_strength(RFOUT_PIN+1, drive_strength);
    gpio_set_drive_strength(RFOUT_PIN+2, drive_strength);
    gpio_set_drive_strength(RFOUT_PIN+3, drive_strength);
    //*****************

	dallas_setup();  //configures one-wire interface. Enabled pullup on one-wire gpio. must do this here, in case they want to use analog instead, because then pullup needs to be disabled below.

	for (int i=0;i < 4;i++)   //init ADC(s) as needed for TELEN
		{			
		   switch(_TELEN_config[i])
			{
				case '-':  break; //do nothing, telen chan is disabled
				case '0': gpio_init(26);gpio_set_dir(26, GPIO_IN);gpio_set_pulls(26,0,0);break;
				case '1': gpio_init(27);gpio_set_dir(27, GPIO_IN);gpio_set_pulls(27,0,0);break;
				case '2': gpio_init(28);gpio_set_dir(28, GPIO_IN);gpio_set_pulls(28,0,0);break; 
			}
			
		}
	
	gpio_init(PICO_VSYS_PIN);  		//Prepare ADC 3 to read Vsys
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

void I2C_init(void)   //this was used for testing HMC5883L compass module. keeping it here as a template for future I2C use
{
/*		
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(20, GPIO_FUNC_I2C);    //pins 20 and 21 for original Pi PIco  (20 Data, 21 Clk) , Custom PCB will use gpio 0,1 instead
    gpio_set_function(21, GPIO_FUNC_I2C);
    gpio_pull_up(20);
    gpio_pull_up(21);

	uint8_t i2c_buf[6];
    uint8_t config_buf[2];
	uint8_t write_config_buf[2];
	uint8_t reg;
	#define ADDR _u(0x1E)   //address of compass module

    config_buf[0] = 0x00; //config register A	
    config_buf[1] =0b00100;  //1.5Hz max update rate
    i2c_write_blocking(i2c_default, ADDR, config_buf, 2, false);
    config_buf[0] = 0x01; //config register B	
    config_buf[1] =0b00000000;  //max gain
    i2c_write_blocking(i2c_default, ADDR, config_buf, 2, false);
    config_buf[0] = 0x02; //Mode register
    config_buf[1] =0x00;  //normal mode
    i2c_write_blocking(i2c_default, ADDR, config_buf, 2, false);
    printf("Done I2C config \n");
*/
}
void I2C_read(void)  //this was used for testing HMC5883L compass module. keeping it here as a template for future I2C use
{
	/*
	write_config_buf[0]=0x3;  											//reg number to start reading at
	i2c_write_blocking(i2c_default, ADDR, write_config_buf , 1, true);  // send 3 to tell it we about to READ from register 3, and keep Bus control true
    i2c_read_blocking(i2c_default, ADDR, i2c_buf, 6, false);            //reads six bytes of registers, starting at address you used above
	int16_t x_result = (int16_t)((i2c_buf[0]<<8)|i2c_buf[1]);           //not bothering with Z axis, because assume sensor board is horizontal
	int16_t y_result = (int16_t)((i2c_buf[4]<<8)|i2c_buf[5]);
	printf("X: %d\n Y: %d\n",x_result,y_result);    //to make a useful "compass", you would need to keep track of max/min X,y values, scale them against those limits, take ratio of the two scaled values, and that corresponds to heading. direction (to direction)
	*/
}

void onewire_read()
{
                if ((ow_read(&one_wire_interface) != 0)&&(number_of_onewire_devs>0))   //if conversions ready, read it
				{
                // read the result from each device                   
                for (int i = 0; i < number_of_onewire_devs; i += 1) 
					{				
						ow_reset (&one_wire_interface);
						ow_send (&one_wire_interface, OW_MATCH_ROM);
							for (int b = 0; b < 64; b += 8) {
								ow_send (&one_wire_interface, OW_romcodes[i] >> b);
							   }
						ow_send (&one_wire_interface, DS18B20_READ_SCRATCHPAD);
						int16_t temp = 0;
						temp = ow_read (&one_wire_interface) | (ow_read (&one_wire_interface) << 8);
						if (temp!=-1)
						onewire_values[i]= 32.0 + ((temp / 16.0)*1.8);
						else printf("\nOneWire device read failure!! re-using previous value\n");
						//printf ("\t%d: %f", i,onewire_values[i]);
					}
					  // start temperature conversion in parallel on all devices so they will be ready for the next time i try to read them
					  // (see ds18b20 datasheet)
					  ow_reset (&one_wire_interface);
					  ow_send (&one_wire_interface, OW_SKIP_ROM);
					  ow_send (&one_wire_interface, DS18B20_CONVERT_T);
				}

}
//sets up OneWire interface
void dallas_setup() {  

    PIO pio = pio0;
    uint offset;
	gpio_init(gpio_for_onewire);
	gpio_pull_up(gpio_for_onewire);  //with this you dont need external pull up resistor on data line (phantom power still won't work though)

    // add the program to the PIO shared address space
    if (pio_can_add_program (pio, &onewire_program)) {
        offset = pio_add_program (pio, &onewire_program);
		
		if (ow_init (&one_wire_interface, pio, offset, gpio_for_onewire))  // claim a state machine and initialise a driver instance
		 {
            // find and display 64-bit device addresses

            number_of_onewire_devs = ow_romsearch (&one_wire_interface, OW_romcodes, maxdevs, OW_SEARCH_ROM);

            printf("Found %d devices\n", number_of_onewire_devs);      
            for (int i = 0; i < number_of_onewire_devs; i += 1) {
                printf("\t%d: 0x%llx\n", i, OW_romcodes[i]);
            }
            putchar ('\n');
         
		  // start temperature conversion in parallel on all devices right now so the values will be ready to read as soon as i try to
          // (see ds18b20 datasheet)
          ow_reset (&one_wire_interface);
          ow_send (&one_wire_interface, OW_SKIP_ROM);
          ow_send (&one_wire_interface, DS18B20_CONVERT_T);

		} else	puts ("could not initialise the onewire driver");
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
/////////////////////////////////////////////////////////////////////////////////////////////////
void datalog_special_functions()   //this called only from user-setup menu
{
/*		FLASH_TARGET_OFFSET(0x4 0000): a pointer to a safe place after the program memory  (
		xip_base offset (0x1000 0000) only needed when READING, not writing)
	    FLASH_TARGET_OFFSET = 040000x
		FLASH_SECTOR_SIZE,   4096
		FLASH_PAGE_SIZE       256 */		

uint8_t *pointer_to_byte;
char c;
uint32_t byte_counter;
uint32_t sector_count; //65- 321  //add xip_base offset ONLY when reading, each sector is 4096 bytes. this is 1MB of data in a safe place (could go close to 2MB theoreticallY). sector 64 is where NBRAM (user settings) are
	
if (_Datalog_mode[0]=='D') //Dumps memory to usb serial port
{
			printf("About to dump...\n");

			for (sector_count=65;sector_count<(321-1);sector_count+=1)   //sector 64 is  where user settings are, so start at 65			
			{
				for (byte_counter=0;byte_counter<(FLASH_SECTOR_SIZE-1);byte_counter+=1)   
				{
					pointer_to_byte=(char *)(XIP_BASE+byte_counter+(sector_count*FLASH_SECTOR_SIZE));
					c = *pointer_to_byte;
					if (c==255) break;    //255 is uninitialized or blank					
					printf("%c",c);
				//sleep_ms(5);                     //may or may not be needed for very large transfers?
				}  
				if (c==255) break;
			}
			printf("\nDone dumping memory, zero reached at %d bytes in sector %d\n",byte_counter,sector_count);
}

if(_Datalog_mode[0]=='W')   
{
	printf("WIPING EVERYTHING in 5 seconds! press a key to abort....\n");
	int cc=getchar_timeout_us(6000000);		

  if (cc==PICO_ERROR_TIMEOUT)
  {
	printf("wiping in process, please wait...\n");
	uint32_t ints = save_and_disable_interrupts();	
	flash_range_erase(FLASH_SECTOR_SIZE*65L,FLASH_SECTOR_SIZE*256L );  
	restore_interrupts (ints);
	printf("* * * Done Wiping! * * * \n");
  }
  else	printf("Wipe aborted. Phew!\n");  
}

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void write_to_next_avail_flash(char *text)   //text can be a literal, a pointer to char arracy, or a char array
{
uint32_t byte_counter;
uint32_t sector_count;	
uint32_t found_byte_location;
uint32_t found_sector;	
uint8_t current_sector_data[4096];		
uint8_t next_sector_data[4096];		
uint8_t *pointer_to_byte;
char c;
size_t length_of_input = strlen(text);

		for (sector_count=65;sector_count<(321-1);sector_count+=1)     //find next open spot	
			{
				for (byte_counter=0;byte_counter<(FLASH_SECTOR_SIZE-1);byte_counter+=1)   
				{
					pointer_to_byte=(char *)(XIP_BASE+byte_counter+(sector_count*FLASH_SECTOR_SIZE));
					c = *pointer_to_byte;
					if (c==255) break;    //255 is uninitialized or blank					
				}  
				if (c==255) break;
			}
	printf("found opening at byte # %d bytes in sector # 	%d\n",byte_counter,sector_count);
	found_sector=sector_count;
	found_byte_location=byte_counter;

//read the whole sector
	for (byte_counter=0;byte_counter<(FLASH_SECTOR_SIZE-1);byte_counter+=1)   
				{
					pointer_to_byte=(char *)(XIP_BASE+byte_counter+(found_sector*FLASH_SECTOR_SIZE));
					c = *pointer_to_byte;
					current_sector_data[byte_counter]=c;					
				}  

//read the entire NEXT sector (just in case wrapping is needed)
	for (byte_counter=0;byte_counter<(FLASH_SECTOR_SIZE-1);byte_counter+=1)   
				{
					pointer_to_byte=(char *)(XIP_BASE+byte_counter+((found_sector+1)*FLASH_SECTOR_SIZE));
					c = *pointer_to_byte;
					next_sector_data[byte_counter]=c;					
				}  
	
if ( (length_of_input + found_byte_location)>FLASH_SECTOR_SIZE)  //then need to wrap
				//need to span 2 sectors
			{
				for (byte_counter=found_byte_location;byte_counter<FLASH_SECTOR_SIZE;byte_counter+=1)   //first part
				{
					current_sector_data[byte_counter]= *((char *)(text+byte_counter-found_byte_location));					
				}  
				for (byte_counter=0;byte_counter<(length_of_input-(FLASH_SECTOR_SIZE-found_byte_location));byte_counter+=1)   //2nd part part
				{
					next_sector_data[byte_counter]= *((char *)(text+byte_counter + (FLASH_SECTOR_SIZE-found_byte_location)      ));					
				}
				
			}
			else
				//can fit new data in current sector
			{
				for (byte_counter=found_byte_location;byte_counter<(found_byte_location+length_of_input);byte_counter+=1)   
				{
					current_sector_data[byte_counter]= *((char *)(text+byte_counter-found_byte_location));					
				}
			}
	
	uint32_t ints = save_and_disable_interrupts();	
	flash_range_erase(FLASH_SECTOR_SIZE*found_sector,FLASH_SECTOR_SIZE);  	
	flash_range_program(FLASH_SECTOR_SIZE*found_sector, current_sector_data, FLASH_SECTOR_SIZE);  
	flash_range_erase(FLASH_SECTOR_SIZE*(1+found_sector),FLASH_SECTOR_SIZE);  
	flash_range_program(FLASH_SECTOR_SIZE*(1+found_sector), next_sector_data, FLASH_SECTOR_SIZE); 
	restore_interrupts (ints);
	
	printf("size of input string %s is: %d wrote it to byte %d in sector %d\n",text,length_of_input,found_byte_location,found_sector);

}
//////////////////////////
void datalog_loop()
{
	char string_to_log[400];
	absolute_time_t GPS_wait_start_time;
	uint64_t t;
	int elapsed_seconds;

				printf("Enterring DATA LOG LOOP. waiting for sat lock or 65 sec max\n");
				const float conversionFactor = 3.3f / (1 << 12);          //read temperature
				adc_select_input(4);	
				float adc = (float)adc_read() * conversionFactor;
				float tempf =32+(( 27.0f - (adc - 0.706f) / 0.001721f)*(9.0f/5.0f));						
				adc_select_input(3);  //if setup correctly, ADC3 reads Vsys   // read voltage
				float volts = 3*(float)adc_read() * conversionFactor;  

				GPS_wait_start_time = get_absolute_time();
	 
				do
					{
						t = absolute_time_diff_us(GPS_wait_start_time, get_absolute_time());	
										if (getchar_timeout_us(0)>0)   //looks for input on USB serial port only. Note: getchar_timeout_us(0) returns a -2 (as of sdk 2) if no keypress. But if you force it into a Char type, becomes something else
										{
											DCO._pGPStime->user_setup_menu_active=1;	
											user_interface();   
										}
					} 
				while (( t<450000000ULL )&&(DCO._pGPStime->_time_data.sat_count<4));               //wait for DCO._pGPStime->_time_data.sat_coun>4 with 65 second maximum time
					//set to 450 seconds !!!!!
				elapsed_seconds= t  / 1000000ULL;

				if (DCO._pGPStime->_time_data.sat_count>=4)
				{
				sleep_ms(3000); //even though sat count seen, wait a bit longer
				sprintf(string_to_log,"latitutde:,%lli,longitude:,%lli,altitude:,%f,sat count:,%d,time:,%s,temp:,%f,bat voltage:,%f,seconds to aquisition:,%d\n",DCO._pGPStime->_time_data._i64_lon_100k,DCO._pGPStime->_time_data._i64_lat_100k,DCO._pGPStime->_altitude,DCO._pGPStime->_time_data.sat_count,DCO._pGPStime->_time_data._full_time_string,tempf,volts,elapsed_seconds);
				write_to_next_avail_flash(string_to_log);
				printf("GPS data has been logged.\n");
				}
					else
				{
				sprintf(string_to_log,"no reading, time might be:,%s,temp:,%f,bat voltage:,%f\n",DCO._pGPStime->_time_data._full_time_string,tempf,volts);
				write_to_next_avail_flash(string_to_log);
				printf("NO GPS seen :-(\n");
				}

				printf("About to sleep!\n");
				gpio_set_dir(GPS_ENABLE_PIN, GPIO_IN);  //let the mosfet drive float
				gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN, GPIO_IN); 
				gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+1, GPIO_IN); 
				gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+2, GPIO_IN); 
				gpio_put(6, 0); //these are required ONLY for v0.1 of custom PCB (ON/OFF and nReset of GPS module, which later are just left disconnected)
				gpio_put(5, 0);

				go_to_sleep();

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void reboot_now()
{
printf("\n\nrebooting...");watchdog_enable(100, 1);for(;;)	{}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void go_to_sleep()
{
			datetime_t t = {.year  = 2020,.month = 01,.day= 01, .dotw= 1,.hour=1,.min= 1,.sec = 00};			
			rtc_init(); // Start the RTC
			rtc_set_datetime(&t);
			uart_default_tx_wait_blocking();
			datetime_t alarm_time = t;

			alarm_time.min += 20;	//sleep for 20 minutes.
			//alarm_time.sec += 15;

			gpio_set_irq_enabled(GPS_PPS_PIN, GPIO_IRQ_EDGE_RISE, false); //this is needed to disable IRQ callback on PPS
			multicore_reset_core1();  //this is needed, otherwise causes instant reboot
			sleep_run_from_dormant_source(DORMANT_SOURCE_ROSC);  //this reduces sleep draw to 2mA! (without this will still sleep, but only at 8mA)
			sleep_goto_sleep_until(&alarm_time, &sleep_callback);	//blocks here during sleep perfiod
			{watchdog_enable(100, 1);for(;;)	{} }  //recovering from sleep is messy, so this makes it reboot to get a fresh start
}
////////////////////////////////////
/*
kevin 10_30_24
From Hans G0UPL  on 06/27/23 post #11140 (this is not documented elsewhere). We had been using a table-driven mapping before Hans posted his algo .
 
The specification of U4B telemetry channels is as follows:
 
First callsign character:
Channels 0 - 199: '0'
Channels 200-399: '1'
Channels 400-599: 'Q'
 
Third callsign character:
(channel % 200) / 20
 
Frequency discrimination:
Frequency sector is
(channel % 20) / 5
 
That indicates into the array of transmit audio frequencies: {1420, 1460, 1540, 1580};
which are the target transmit frequencies, each in their 5 sectors.
Of course the actual transmit frequency is the standard WSPR USB dial frequency + the above mentioned audio frequency; USB dial frequencies:
{136000, 474200, 1836600, 3568600, 5364700, 7038600, 10138700, 14095600, 18104600, 21094600, 24924600, 28124600, 50293000, 70091000, 144489000};
 
Transmit slot:
The transmit slot (txSlot) is first calculated as (channel % 5).
Then the start time in minutes past the hour, repeated every 10 minutes, is given by:
2 * ((txSlot + 2 * txBand) % 5);
 
txBand is:
0: 2200m
1: 630m
2: 160m
3: 80m
4: 60m
5: 40m
6: 30m
7: 20m
8: 17m
9: 15m
10: 12m
11: 10m
12: 6m
13: 4m
14: 2m
*/
 
void process_chan_num()
{
	if ( (atoi(_U4B_chan)>=0) && (atoi(_U4B_chan)<600)) 
	{
		
		_id13[0]='1';
        // Channels 0 - 199: '0'
        // Channels 200-399: '1'
        // Channels 400-599: 'Q'
		if  (atoi(_U4B_chan)<200) _id13[0]='0';
		if  (atoi(_U4B_chan)>399) _id13[0]='Q';

        // (channel % 200) / 20
		int id3 = (atoi(_U4B_chan) % 200) / 20;
		_id13[1]=id3+'0';
		
        // Frequency discrimination:
        // Frequency sector is
        // (channel % 20) / 5
		int lane = (atoi(_U4B_chan) % 20) / 5;
		_lane[0]=lane+'1';

        // The transmit slot (txSlot) is first calculated as (channel % 5).
        // Then the start time in minutes past the hour, repeated every 10 minutes, is given by:
        // 2 * ((txSlot + 2 * txBand) % 5);
		int txSlot = atoi(_U4B_chan) % 5;
        int txBand;
        switch(atoi(_Band))
        {
            case 20: txBand = 7;  break; // 20m
            case 17: txBand = 8;  break; // 17m
            case 15: txBand = 9;  break; // 15m
            case 12: txBand = 10; break; // 12m
            case 10: txBand = 11; break; // 10m
            default: txBand = 7;  break; // default to 20M in case of error cases
        }
		_start_minute[0] = '0' + (2 * ((txSlot + (txBand*2)) % 5));

	}
}
