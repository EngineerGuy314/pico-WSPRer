/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico/multicore.h"
#include "hf-oscillator/lib/assert.h"
#include "hf-oscillator/defines.h"
#include <defines.h>
#include <piodco.h>
#include <WSPRbeacon.h>
#include <logutils.h>
#include <protos.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   //dont forget to add hardware_adc to the target-link-libraries in CMakeLists.txt
#define d_force_xmit_for_testing NO
#define CONFIG_GPS_SOLUTION_IS_MANDATORY YES
#define CONFIG_GPS_RELY_ON_PAST_SOLUTION NO
#define CONFIG_SCHEDULE_SKIP_SLOT_COUNT 5      //not used anymore
#define CONFIG_LOCATOR4 "AA22ab"       	       //gets overwritten by gps data anyway
// Serial RX from GPS module wired to GPIO 1 (pin 2 on pico).
#define GPS_PPS_PIN 2          /* GPS time mark PIN. (labeled PPS on GPS module)*/ //its not actually PIN 2, its GPIO 2, which is physical pin 4 on pico
#define RFOUT_PIN 6            /* RF output PIN. */                                 //its not actually PIN 6, its GPIO 6, which is physical pin 9 on pico
#define GPS_ENABLE_PIN 3       /* GPS_ENABLE pin - high to enable GPS (needs a MOSFET ie 2N7000 on low side drive */    //its not actually PIN 3, its GPIO 3, which is physical pin 5 on pico

#define CONFIG_CALLSIGN "KC3LBR"  //your callsign (no suffix or prefixes)
/* Go to https://traquito.github.io/channelmap/ and choose a clear channel. Make note of id13 (blue column header), slot (min) and lane  */
#define CONFIG_id13 "Q8" 		  //two character alphanumeric channel specifier. will be the 1st and 3rd char of callsign in second xmission. see column headers on channelmap chart
#define CONFIG_start_minute 2             //0, 2, 4, 6 or 8. defines which MINUTE the first of the two xmissions begins on 
//#define CONFIG_WSPR_DIAL_FREQUENCY 14097020UL  // Lane 1
//#define CONFIG_WSPR_DIAL_FREQUENCY 14097060UL  // Lane 2
//#define CONFIG_WSPR_DIAL_FREQUENCY 14097140UL  // Lane 3
#define CONFIG_WSPR_DIAL_FREQUENCY 14097180UL  // Lane 4


WSPRbeaconContext *pWSPR;

int main()
{
    gpio_init(PICO_DEFAULT_LED_PIN); 
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT); //initialize LED output
    /* the next 6 lines  are needed to allow ADC3 to read VSYS */
	gpio_init(25);  //needed to allow ADC3 to read Vsys
	gpio_set_dir(25, GPIO_OUT); 
    gpio_put(25, 1);	
    gpio_init(29);  //needed to allow ADC3 to read Vsys
	gpio_set_dir(29, GPIO_IN); 
	gpio_set_pulls(29,0,0);	
    adc_init();
    adc_set_temp_sensor_enabled(true);
	StampPrintf("\n");
	
	for (int i=0;i < 20;i++)     //do some blinkey on startup, allows time for power supply to stabilize before GPS unit enabled
	{
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(100);
	}

    StampPrintf("pico-WSPRer v3");      //messages are sent to USB serial port, 115200 baud
    InitPicoHW();
    PioDco DCO = {0};
	StampPrintf("WSPR beacon init...");

    WSPRbeaconContext *pWB = WSPRbeaconInit(
        CONFIG_CALLSIGN,/* the Callsign. */
        CONFIG_LOCATOR4,/* the default QTH locator if GPS isn't used. */
        10,             /* Tx power, dbm. */  //in reality probably closer to 6 (3mW)
        &DCO,           /* the PioDCO object. */
        CONFIG_WSPR_DIAL_FREQUENCY,
        0,           /* the carrier freq. shift relative to dial freq. */ //not used
        RFOUT_PIN       /* RF output GPIO pin. */
        );
    assert_(pWB);
    pWSPR = pWB;
    pWB->_txSched._u8_tx_GPS_mandatory  = CONFIG_GPS_SOLUTION_IS_MANDATORY;
    pWB->_txSched._u8_tx_GPS_past_time  = CONFIG_GPS_RELY_ON_PAST_SOLUTION;
    pWB->_txSched._u8_tx_slot_skip      = CONFIG_SCHEDULE_SKIP_SLOT_COUNT;
    pWB->_txSched.force_xmit_for_testing = d_force_xmit_for_testing;
	pWB->_txSched.led_mode = 0;  //waiting for GPS
	pWB->_txSched.Xmission_In_Process = 0;  
	pWB->_txSched.output_number_toEnable_GPS = GPS_ENABLE_PIN;
	pWB->_txSched.start_minute = CONFIG_start_minute;
	strcpy(pWB->_txSched.id13,CONFIG_id13);
    multicore_launch_core1(Core1Entry);
    StampPrintf("RF oscillator started.");

	gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output output
    gpio_put(GPS_ENABLE_PIN, 1); 									   // to power up GPS unit

    DCO._pGPStime = GPStimeInit(0, 9600, GPS_PPS_PIN); //the 0 defines uart0, so the RX is GPIO 1 (pin 2 on pico). TX to GPS module not needed
    assert_(DCO._pGPStime);

    int tick = 0;
    for(;;)   //loop every ~ second
    {
        //GET MAIDENHEAD       - this code in original fork wasnt working due to error in WSPRbeacon.c
        if(WSPRbeaconIsGPSsolutionActive(pWB))
        {
            const char *pgps_qth = WSPRbeaconGetLastQTHLocator(pWB);
            if(pgps_qth)
            {
                strncpy(pWB->_pu8_locator, pgps_qth, 6);     //does full 6 char maidenhead 				
		        pWB->_pu8_locator[7] = 0x00;                 //null terminates  (needed?)
            }
        }        
        WSPRbeaconTxScheduler(pWB, YES);   
                
#ifdef DEBUG
        if(0 == ++tick % 20)      //every ~20 secs dumps context.  This is a great example of writing clever and consise code that is the opposite of "clear and easy to follow"
        WSPRbeaconDumpContext(pWB);
#endif
		//orig code had a 900mS pause here 
	    
    const float conversionFactor = 3.3f / (1 << 12);          //read temperature
    adc_select_input(4);	
	float adc = (float)adc_read() * conversionFactor;
	float tempC = 27.0f - (adc - 0.706f) / 0.001721f;
	pWB->_txSched.temp_in_Celsius=tempC;
	//printf("as i: %i\n", pWB->_txSched.temp_in_Celsius);

	adc_select_input(3);  //if setup correctly, ADC3 reads Vsys   // read voltage
	float volts = 3*(float)adc_read() * conversionFactor;         //times 3 because thats what it said to do on the internet
	    if (volts < 3.00) { volts += 1.95; }			          //wrap around for overflow
        if (volts > 4.95) { volts -= 1.95; }
	pWB->_txSched.voltage=volts;
			
		gpio_put(PICO_DEFAULT_LED_PIN, 1); //LED on. how long it stays on depends on "mode"0,1,2 ~= no gps, waiting for slot, xmitting
		if (pWB->_txSched.led_mode==0)
		{
		sleep_ms(50);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(850);
		}
		if (pWB->_txSched.led_mode==1)
		{
		sleep_ms(450);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(450);
		}
		if (pWB->_txSched.led_mode==2)
		{
		sleep_ms(850);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(50);
		}

    }
}

