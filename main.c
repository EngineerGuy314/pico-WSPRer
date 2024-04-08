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
#include <ctype.h>
#include "pico/multicore.h"
#include "hf-oscillator/lib/assert.h"
#include "hf-oscillator/defines.h"
#include "hardware/flash.h"
#include <defines.h>
#include <piodco.h>
#include <WSPRbeacon.h>
#include <logutils.h>
#include <protos.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   //dont forget to add hardware_adc to the target-link-libraries in CMakeLists.txt
#include "hardware/watchdog.h"
#define d_force_xmit_for_testing NO
#define CONFIG_GPS_SOLUTION_IS_MANDATORY YES   //not used anymore
#define CONFIG_GPS_RELY_ON_PAST_SOLUTION NO     //not used anymore
#define CONFIG_SCHEDULE_SKIP_SLOT_COUNT 5      //not used anymore
#define CONFIG_LOCATOR4 "AA22ab"       	       //gets overwritten by gps data anyway

// Serial RX from GPS module wired to GPIO 1 (pin 2 on pico).
#define GPS_PPS_PIN 2          /* GPS time mark PIN. (labeled PPS on GPS module)*/ //its not actually PIN 2, its GPIO 2, which is physical pin 4 on pico
#define RFOUT_PIN 6            /* RF output PIN. */                                 //its not actually PIN 6, its GPIO 6, which is physical pin 9 on pico
#define GPS_ENABLE_PIN 3       /* GPS_ENABLE pin - high to enable GPS (needs a MOSFET ie 2N7000 on low side drive */    //its not actually PIN 3, its GPIO 3, which is physical pin 5 on pico
#define FLASH_TARGET_OFFSET (256 * 1024) //leaves 256k of space for the program

WSPRbeaconContext *pWSPR;

char _callsign[7];        //these get set via terminal, and then from NVRAM on boot
char _id13[3];
char _start_minute[2];
char _lane[2];

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

	read_NVRAM();
    StampPrintf("pico-WSPRer version: %s %s\n",__DATE__ ,__TIME__);//messages are sent to USB serial port, 115200 baud
    InitPicoHW();
    PioDco DCO = {0};
	StampPrintf("WSPR beacon init...");
	uint32_t XMIT_FREQUENCY;
	switch(_lane[0])
		{
			case '1':XMIT_FREQUENCY=14097020UL;break;
			case '2':XMIT_FREQUENCY=14097060UL;break;
			case '3':XMIT_FREQUENCY=14097140UL;break;
			case '4':XMIT_FREQUENCY=14097180UL;break;
		}	
    
	WSPRbeaconContext *pWB = WSPRbeaconInit(
        _callsign,/* the Callsign. */
        CONFIG_LOCATOR4,/* the default QTH locator if GPS isn't used. */
        10,             /* Tx power, dbm. */  //in reality probably closer to 6 (3mW)
        &DCO,           /* the PioDCO object. */
        XMIT_FREQUENCY,
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
	pWB->_txSched.start_minute,_start_minute[0]-48;
	strcpy(pWB->_txSched.id13,_id13);

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
	
	if (getchar_timeout_us(0)>0) user_interface();

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
		sleep_ms(20);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(880);
		}
		if (pWB->_txSched.led_mode==1)
		{
		sleep_ms(450);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(450);
		}
		if (pWB->_txSched.led_mode==2)
		{
		sleep_ms(880);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
		sleep_ms(20);
		}

    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

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
	void read_NVRAM(void)
	{
	const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET); //a pointer to a safe place after the program memory
	
	print_buf(flash_target_contents, FLASH_PAGE_SIZE); //256

	strncpy(_callsign, flash_target_contents, 6);
	strncpy(_id13, flash_target_contents+6, 2);
	strncpy(_start_minute, flash_target_contents+8, 1);
	strncpy(_lane, flash_target_contents+9, 1);

	}
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void user_interface(void)
{
    char c;
	char str[10];
	
    gpio_put(GPS_ENABLE_PIN, 0); //shutoff gps to prevent serial input

printf("\n\n\n\n\n\n\n\n\n\n\n\n");
printf("Pico-WSPRer (pico whisper-er) by KC3LBR,  version: %s %s\n",__DATE__ ,__TIME__);
printf("https://github.com/EngineerGuy314/pico-WSPRer\n");
printf("forked from: https://github.com/RPiks/pico-WSPR-tx\n\n");
printf("consult https://traquito.github.io/channelmap/ to find an open channel \nand make note of id13 (column headers), minute and lane (frequency)\n");
show_values();
    for(;;)
	{		
		c=getchar();
		if (c>90) c-=32; //make it capital either way
		switch(c)
		{
			case 'X': printf("\n\nGOODBYE");watchdog_enable(100, 1);for(;;)	{}
			case 'C':printf("Enter callsign: ");		 scanf(" %s", _callsign); _callsign[6]=0;convertToUpperCase(_callsign); write_NVRAM(); show_values();break;
			case 'I':printf("Enter id13: ");			 scanf(" %s", _id13);_id13[2]=0; convertToUpperCase(_id13); write_NVRAM(); show_values();break;
			case 'M':printf("Enter starting Minute: ");  scanf(" %s", _start_minute); _start_minute[1]=0; write_NVRAM(); show_values();break;
			case 'L':printf("Enter Lane (1,2,3,4): ");   scanf(" %s", _lane);_lane[1]=0;  write_NVRAM(); show_values();break;
			case 13:  break;
			case 10:  break;
			default: printf("\n\n\n\n\n\n\nyou pressed %c, invalid choice",c);show_values();break;
		}
	}

}
//
void show_values(void)
{
printf("\n\ncurrent values:\n\tCallsign:%s\n\tId13:%s\n\tMinute:%s\n\tLane:%s\n\n",_callsign,_id13,_start_minute,_lane);
printf("VALID commands: \n\n\tx: eXit configuraiton and reboot\n\tc: change Callsign (6 char max)\n\tI: change Id13 (two alpha numeric chars, ie Q8)\n\tM: change starting Minute (0,2,4,6,8)\n\tL: Lane (1,2,3,4) corresponding to 4 frequencies in 20M band\n\n");

}
//
void write_NVRAM(void)
{
    uint8_t data_chunk[FLASH_PAGE_SIZE];

	strncpy(data_chunk,_callsign, 6);
	strncpy(data_chunk+6,_id13,  2);
	strncpy(data_chunk+8,_start_minute, 1);
	strncpy(data_chunk+9,_lane, 1);

	uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
	flash_range_program(FLASH_TARGET_OFFSET, data_chunk, FLASH_PAGE_SIZE);
	restore_interrupts (ints);

}
////////////////
void convertToUpperCase(char *str) {
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}