/////////////////////////////////////////////////////////////////////////////
//  Majority of code forked from work by
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//  PROJECT PAGE
//  https://github.com/RPiks/pico-WSPR-tx
///////////////////////////////////////////////////////////////////////////////
#include "WSPRbeacon.h"
#include <WSPRutility.h>
#include <maidenhead.h>
#include <math.h>

static int itx_trigger = 0;
static int itx_trigger2 = 0;		
static int forced_xmit_in_process = 0;
static int transmitter_status = 0;	
static absolute_time_t start_time;
static absolute_time_t time_of_last_serial_packet;
static int current_minute;
static int oneshots[10];
static int schedule[10];  //array index is minute, (odd minutes are unused) value is -1 for NONE or 1-4 for U4B 1st msg,U4B 2nd msg,Zachtek 1st, Zachtek 2nd, and #5 for extended TELEN
static int at_least_one_slot_has_elapsed;
static int at_least_one_GPS_fixed_has_been_obtained;
static uint8_t _callsign_with_suffix[12];
static 	uint8_t  altitude_as_power_fine;
static uint32_t previous_msg_count;
static int tikk;
static int tester;
const int8_t valid_dbm[19] =
    {0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40,
     43, 47, 50, 53, 57, 60};  

/// @brief Initializes a new WSPR beacon context.
/// @param pcallsign HAM radio callsign, 12 chr max.
/// @param pgridsquare Maidenhead locator, 7 chr max.
/// @param txpow_dbm TX power, db`mW.
/// @param pdco Ptr to working DCO.
/// @param dial_freq_hz The begin of working WSPR passband.
/// @param shift_freq_hz The shift of tx freq. relative to dial_freq_hz.
/// @param gpio Pico's GPIO pin of RF output.
/// @return Ptr to the new context.
/// @bug The function sets schedule hardcoded to minutes 1, 2 and 3. It may lead to 
/// @bug out of schedule transmits if the device is switched on around minute 0  
WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm,
                                  PioDco *pdco, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio,  uint8_t start_minute, uint8_t id13, uint8_t suffix)
{
    assert_(pcallsign);
    assert_(pgridsquare);
    assert_(pdco);
    WSPRbeaconContext *p = calloc(1, sizeof(WSPRbeaconContext));
    assert_(p);
    strncpy(p->_pu8_callsign, pcallsign, sizeof(p->_pu8_callsign));
    strncpy(p->_pu8_locator, pgridsquare, sizeof(p->_pu8_locator));
    p->_u8_txpower = txpow_dbm;
    p->_pTX = TxChannelInit(682667, 0, pdco);
    assert_(p->_pTX);
    p->_pTX->_u32_dialfreqhz = dial_freq_hz + shift_freq_hz;
    p->_pTX->_i_tx_gpio = gpio;
 	srand(3333);
	at_least_one_slot_has_elapsed=0;
	for (int i=0;i < 10;i++) schedule[i]=-1;
	tester=0;
	if (id13==253)              //if U4B protocol disabled,  we will ONLY do Type3 (zachtek) at the specified minute
	{
		schedule[start_minute]=3;           
		schedule[(start_minute+2)%10]=4;
	}
else
	{
		schedule[start_minute]=1;          //do 1st U4b packet at selected minute 
		schedule[(start_minute+2)%10]=2;   //do second U4B packet 2 minutes later
		schedule[(start_minute+4)%10]=5;  //for now, ALWAYS does #5 (TELEN 1) extended telemetry as part of U4B. eventially should make this optional
		schedule[(start_minute+6)%10]=6;  //for now, ALWAYS does #6 (TELEN 2) extended telemetry as part of U4B. eventially should make this optional (must NOT use zachtek protocol with Telen#2 for instance)

		if (suffix != 253)    // if Suffix enabled, Do zachtek messages 4 mins BEFORE (ie 6 minutes in future) of u4b (because minus (-) after char to decimal conversion is 253)
			{
				schedule[(start_minute+6)%10]=3;          
				schedule[(start_minute+8)%10]=4;
			}
	}
	at_least_one_GPS_fixed_has_been_obtained=0;
	transmitter_status=0;

 return p;
}

/// @brief Sets dial (baseband minima) freq.
/// @param pctx Context.
/// @param freq_hz the freq., Hz.
//******************************************************************************************************************************
void WSPRbeaconSetDialFreq(WSPRbeaconContext *pctx, uint32_t freq_hz)
{
    assert_(pctx);
    pctx->_pTX->_u32_dialfreqhz = freq_hz;
}

/// @brief Constructs a new WSPR packet using the data available.
/// @param pctx Context
/// @return 0 if OK.
//******************************************************************************************************************************
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int packet_type)  //1-6.  1: U4B 1st msg,U4B 2: 2nd msg, 3: Zachtek 1st, 4: Zachtek 2nd 5:U4B telen 1, 6:U4B telen 2
{
   /*     if(0 == ++tikk % 2)    //turns a fan on via GPIO 18 every other packet. this forces temperature swings for testing TCXO stability   
		gpio_put(18, 1);
       if(0 == (tikk+1) % 2)
		gpio_put(18, 0);  */

   assert_(pctx);

   if (packet_type==1)   //U4B first msg
   {
	pctx->_u8_txpower =10;               //hardcoded at 10dbM when doing u4b MSG 1
	if (pctx->_txSched.verbosity>=3) printf("creating U4B packet 1\n");
	char _4_char_version_of_locator[5];
	strncpy(_4_char_version_of_locator, pctx->_pu8_locator, 4);     //only take first 4 chars of locator
	_4_char_version_of_locator[4]=0;  //add null terminator
	wspr_encode(pctx->_pu8_callsign, _4_char_version_of_locator, pctx->_u8_txpower, pctx->_pu8_outbuf, pctx->_txSched.verbosity);   // look in WSPRutility.c for wspr_encode
   }
 if (packet_type==2)   // special encoding for 2nd packet of U4B protocol
   {
	if (pctx->_txSched.verbosity>=3) printf("creating U4B packet 2 \n");
	char CallsignU4B[7]; 
	char Grid_U4B[7]; 
	uint8_t  power_U4B;

/* inputs:  pctx->_pu8_locator (6 char grid)
			pctx->_txSched->temp_in_Celsius
			pctx->_txSched->id13
			pctx->_txSched->voltage
*/	
	  // pick apart inputs
        char grid5 = pctx->_pu8_locator[4];
        char grid6 = pctx->_pu8_locator[5];
	        // convert inputs into components of a big number
        uint8_t grid5Val = grid5 - 'A';
        uint8_t grid6Val = grid6 - 'A';
		uint16_t altFracM =  round((double)pctx->_pTX->_p_oscillator->_pGPStime->_altitude / 20);     

	 // convert inputs into a big number
        uint32_t val = 0;
        val *=   24; val += grid5Val;
        val *=   24; val += grid6Val;
        val *= 1068; val += altFracM;
		        // extract into altered dynamic base
        uint8_t id6Val = val % 26; val = val / 26;
        uint8_t id5Val = val % 26; val = val / 26;
        uint8_t id4Val = val % 26; val = val / 26;
        uint8_t id2Val = val % 36; val = val / 36;
        // convert to encoded CallsignU4B
        char id2 = EncodeBase36(id2Val);
        char id4 = 'A' + id4Val;
        char id5 = 'A' + id5Val;
        char id6 = 'A' + id6Val;
        CallsignU4B[0] =  pctx->_txSched.id13[0];   //string{ id13[0], id2, id13[1], id4, id5, id6 };
		CallsignU4B[1] =  id2;
		CallsignU4B[2] =  pctx->_txSched.id13[1];
		CallsignU4B[3] =  id4;
		CallsignU4B[4] =  id5;
		CallsignU4B[5] =  id6;
		CallsignU4B[6] =  0;

/* inputs:  pctx->_pu8_locator (6 char grid)
			pctx->_txSched->temp_in_Celsius
			pctx->_txSched->id13
			pctx->_txSched->voltage
*/	
/* outputs :	char CallsignU4B[6]; 
				char Grid_U4B[7]; 
				uint8_t  power_U4B;
				*/
        // parse input presentations
        double tempC   = pctx->_txSched.temp_in_Celsius;
        double voltage = pctx->_txSched.voltage;
       // map input presentations onto input radix (numbers within their stated range of possibilities)
        uint8_t tempCNum      = tempC - -50;
        uint8_t voltageNum    = ((uint8_t)round(((voltage * 100) - 300) / 5) + 20) % 40;
 
		uint8_t speedKnotsNum = pctx->_pTX->_p_oscillator->_pGPStime->_time_data.sat_count;   //encoding # of sattelites into knots
        uint8_t gpsValidNum   = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;
        // shift inputs into a big number
        val = 0;
        val *= 90; val += tempCNum;
        val *= 40; val += voltageNum;
        val *= 42; val += speedKnotsNum;
        val *=  2; val += gpsValidNum;
        val *=  2; val += 1;                // standard telemetry (1 means normal standard, 0 is special TELEN?)
        // unshift big number into output radix values
        uint8_t powerVal = val % 19; val = val / 19;
        uint8_t g4Val    = val % 10; val = val / 10;
        uint8_t g3Val    = val % 10; val = val / 10;
        uint8_t g2Val    = val % 18; val = val / 18;
        uint8_t g1Val    = val % 18; val = val / 18;
        // map output radix to presentation
        char g1 = 'A' + g1Val;
        char g2 = 'A' + g2Val;
        char g3 = '0' + g3Val;
        char g4 = '0' + g4Val;
 	
		Grid_U4B[0] = g1; // = string{ g1, g2, g3, g4 };
		Grid_U4B[1] = g2;
		Grid_U4B[2] = g3;
		Grid_U4B[3] = g4;
		Grid_U4B[4] = 0;

	power_U4B=valid_dbm[powerVal];

	wspr_encode(CallsignU4B, Grid_U4B, power_U4B, pctx->_pu8_outbuf,pctx->_txSched.verbosity); 
   }
	
if (packet_type==3)   //1st Zachtek (WSPR type 1 message)
   {
	if (pctx->_txSched.verbosity>=3) printf("creating Zachtek packet 1 (WSPR type 1)\n");
	 strcpy(_callsign_with_suffix,pctx->_pu8_callsign);
	 strcat(_callsign_with_suffix,"/"); 
 	 uint8_t suffix_as_string[2];
	 suffix_as_string[0]=pctx->_txSched.suffix+48;
	 suffix_as_string[1]=0;
	 strcat(_callsign_with_suffix,suffix_as_string);

	uint8_t  altitude_as_power_rough;

			altitude_as_power_rough=0;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>900) altitude_as_power_rough=3;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>2100) altitude_as_power_rough=7;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>3000) altitude_as_power_rough=10;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>3900) altitude_as_power_rough=13;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>5100) altitude_as_power_rough=17;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>6000) altitude_as_power_rough=20;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>6900) altitude_as_power_rough=23;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>8100) altitude_as_power_rough=27;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>9000) altitude_as_power_rough=30;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>9900) altitude_as_power_rough=33;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>11100) altitude_as_power_rough=37;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>12000) altitude_as_power_rough=40;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>12900) altitude_as_power_rough=43;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>14100) altitude_as_power_rough=47;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>15000) altitude_as_power_rough=50;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>15900) altitude_as_power_rough=53;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>17100) altitude_as_power_rough=57;
			if (pctx->_pTX->_p_oscillator->_pGPStime->_altitude>18000) altitude_as_power_rough=60;

			float fine_altitude = pctx->_pTX->_p_oscillator->_pGPStime->_altitude - (altitude_as_power_rough*300.0f);

			altitude_as_power_fine=0;
			if (fine_altitude>60) altitude_as_power_fine=3;
			if (fine_altitude>140) altitude_as_power_fine=7;
			if (fine_altitude>200) altitude_as_power_fine=10;
			if (fine_altitude>260) altitude_as_power_fine=13;
			if (fine_altitude>340) altitude_as_power_fine=17;
			if (fine_altitude>400) altitude_as_power_fine=20;
			if (fine_altitude>460) altitude_as_power_fine=23;
			if (fine_altitude>540) altitude_as_power_fine=27;
			if (fine_altitude>600) altitude_as_power_fine=30;
			if (fine_altitude>660) altitude_as_power_fine=33;
			if (fine_altitude>740) altitude_as_power_fine=37;
			if (fine_altitude>800) altitude_as_power_fine=40;
			if (fine_altitude>860) altitude_as_power_fine=43;
			if (fine_altitude>940) altitude_as_power_fine=47;
			if (fine_altitude>1000) altitude_as_power_fine=50;
			if (fine_altitude>1060) altitude_as_power_fine=53;
			if (fine_altitude>1140) altitude_as_power_fine=57;
			if (fine_altitude>1200) altitude_as_power_fine=60;

				if (pctx->_txSched.verbosity>=3) printf("Raw altitude: %0.3f rough: %d fine: %d\n",pctx->_pTX->_p_oscillator->_pGPStime->_altitude,altitude_as_power_rough,altitude_as_power_fine);

	wspr_encode(_callsign_with_suffix, pctx->_pu8_locator, altitude_as_power_rough, pctx->_pu8_outbuf,pctx->_txSched.verbosity);  
   }

if (packet_type==4)   //2nd Zachtek (WSPR type 3 message)
   {
	if (pctx->_txSched.verbosity>=3) printf("creating Zachtek packet 2 (WSPR type 3)\n");
	wspr_encode(add_brackets(_callsign_with_suffix), pctx->_pu8_locator, altitude_as_power_fine, pctx->_pu8_outbuf,pctx->_txSched.verbosity);  			
   }

if ((packet_type==5)||(packet_type==6))   //TELEN #1 or #2 extended telemetry, gets sent right after the two U4B packets
   {	
	if (pctx->_txSched.verbosity>=3) printf("creating TELEN packet 1\n");
	char _4_char_version_of_locator[5];
	char _callsign[7];
	uint32_t telen_val1;
	uint32_t telen_val2;
	
	if (packet_type==5)    //TELEN #1
		{
			telen_val1=pctx->_txSched.TELEN1_val1;  //gets values for TELEN from global vars
			telen_val2=pctx->_txSched.TELEN1_val2;
			printf("encoding TELEN #1\n");
		}
	  else		//TELEN #2
		{
			telen_val1=pctx->_txSched.TELEN2_val1;  //gets values for TELEN from global vars
			telen_val2=pctx->_txSched.TELEN2_val2;
			printf("encoding TELEN #2\n");
		}
				
	char telen_chars[8];
	uint8_t telen_power;

	encode_telen(telen_val1,telen_val2,telen_chars, &telen_power); //converts two 32bit ints into 8 characters and one byte to be transmitted
	
        _callsign[0] =  pctx->_txSched.id13[0];   //callsign: id13[0], telen char0, id13[1], telen char1, telen char2, telen char3
		_callsign[1] =  telen_chars[0];
		_callsign[2] =  pctx->_txSched.id13[1];	
		_callsign[3] =  telen_chars[1];
		_callsign[4] =  telen_chars[2];
		_callsign[5] =  telen_chars[3];
	_4_char_version_of_locator[0]=telen_chars[4];
	_4_char_version_of_locator[1]=telen_chars[5];
	_4_char_version_of_locator[2]=telen_chars[6];
	_4_char_version_of_locator[3]=telen_chars[7];
	_4_char_version_of_locator[4]=0;  //add null terminator

	wspr_encode(_callsign, _4_char_version_of_locator, telen_power, pctx->_pu8_outbuf, pctx->_txSched.verbosity);   // look in WSPRutility.c for wspr_encode
   }
	return 0;
}
////////////////////////////////////////////////////////////////////
//******************************************************************************************************************************
/// @brief encodes data for extended telemetry (TELEN)
/// @param telen_val1,telen_val2: the values to encode, telen_chars: the output characters, telen_power: output power (in dbm)
//******************************************************************************************************************************
void encode_telen(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power)  
{
	// TELEN packet  which has value 1 and value 2 (this same routine used for both telen#1 and telen#2). 
	// first value  gets encoded into the callsign (1st char is alphannumeric, and last three chars are alpha). Full callsign will be ID1, telen_char[0], ID3, telen_CHar[1],  telen_CHar[2], telen_CHar[3]. 
	// 2nd value gets encoded into GRID and power. grid = telen_CHar[4,5,6,7]. power = telen_power
	// max val of 1st one ~= 632k (per dave) [19 bits] i had originally thought 651,013 (if first char Z, which ~=35, times 17565(26^3). base 26 used, not base 36, because other chars must be only alpha, not alphanumeric because of Ham callsign conventions)
	// max val of 2nd one ~= 153k (per dave) [17 bits] i had thought over 200k....

	uint32_t tem=telen_val1;						
	double temf=telen_val2;
	telen_chars[0]= '0'+floor(tem / 17576); tem-=(telen_chars[0]-'0')*17576; if (telen_chars[0] > '9') telen_chars[0]+=7; //shift up from numeric to alpha (there are 7 ascii codes between 9 and A)
	telen_chars[1]= 'A'+floor(tem / 676); tem-=(telen_chars[1]-'A')*676;
	telen_chars[2]= 'A'+floor(tem / 26); tem-=(telen_chars[2]-'A')*26;
	telen_chars[3]= 'A'+tem;
	telen_chars[4]= 'A'+floor(temf / 8550); temf-=(telen_chars[4]-'A')*8550;   
	telen_chars[5]= 'A'+floor(temf / 475); temf-=(telen_chars[5]-'A')*475;  
	telen_chars[6]= '0'+floor(temf / 47.5); temf-=(telen_chars[6]-'0')*47.5;
	telen_chars[7]= '0'+floor(temf / 4.75); temf-=((telen_chars[7]-'0')*4.75);  
	int i=round(temf/0.25);  // there are 19 possible dbm values. And 4.75/19=0.25
	*telen_power = valid_dbm[i]; 

    printf(" val1: %d val2: %d the chars: %s and the power:(as dmb: %d)\n",telen_val1,telen_val2,telen_chars,*telen_power);
	telen_chars[8]=0; //null terminate
}
////////////////////////////////////////////////////////////////////
//******************************************************************************************************************************
/// @brief Sends a prepared WSPR packet using TxChannel.
/// @param pctx Context.
/// @return 0, if OK.
//******************************************************************************************************************************
int WSPRbeaconSendPacket(const WSPRbeaconContext *pctx)
{
    assert_(pctx);
    assert_(pctx->_pTX);
    assert_(pctx->_pTX->_u32_dialfreqhz > 500 * kHz);
    TxChannelClear(pctx->_pTX); 
    memcpy(pctx->_pTX->_pbyte_buffer, pctx->_pu8_outbuf, WSPR_SYMBOL_COUNT);  //162
    pctx->_pTX->_ix_input = WSPR_SYMBOL_COUNT;  //set count of bytes to send
    return 0;
}
//******************************************************************************************************************************
/// @brief Arranges WSPR sending in accordance with pre-defined schedule.
/// @brief It works only if GPS receiver available (for now).
/// @param pctx Ptr to Context.
/// @return 0 if OK, -1 if NO GPS received available
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose)   // called every half second from Main.c
{
	assert_(pctx);                 	
	uint32_t is_GPS_available = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_nmea_gprmc_count;  //on if there ever were any serial data received from a GPS unit
    const uint32_t is_GPS_active = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;  //on if valid 3d fix

		 if(is_GPS_active) at_least_one_GPS_fixed_has_been_obtained=1;
		 
		 if (pctx->_txSched.force_xmit_for_testing) {            
							if(forced_xmit_in_process==0)
							{
								StampPrintf("> FORCING XMISSION! for debugging   <"); pctx->_txSched.led_mode = 4; 
								PioDCOStart(pctx->_pTX->_p_oscillator);
								//WSPRbeaconCreatePacket(pctx,0);    If this is disabled, the packet is all zeroes, and it xmits an unmodulated steady frequency. but if you didnt power cycle since enabling Force_xmition there will still be data stuck in the buffer...
								sleep_ms(100);
								WSPRbeaconSendPacket(pctx);
								start_time = get_absolute_time();       
								forced_xmit_in_process=1;
							}
								else if(absolute_time_diff_us( start_time, get_absolute_time()) > 120000000ULL) 
								{
									forced_xmit_in_process=0; //restart after 2 mins
									PioDCOStop(pctx->_pTX->_p_oscillator); 
									printf("Pio *STOP*  called by end of forced xmit. small pause before restart\n");
									sleep_ms(2000);
								}								
				return -1;
		 }
  
		 if(!is_GPS_available)
		{
			if (pctx->_txSched.verbosity>=1) StampPrintf(" Waiting for GPS receiver to start communicating, or, serial comms interrupted");
			pctx->_txSched.led_mode = 0;  //waiting for GPS
			return -1;
		}
	 
		if(!is_GPS_active){
			if (pctx->_txSched.verbosity>=1) StampPrintf("Gps was available, but no valid 3d Fix. ledmode %d XMIT status %d",pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);
		}

		current_minute = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_last_digit_minutes - '0';  //convert from char to int
	
	if (schedule[current_minute]==-1)        //if the current minute is an odd minute or a non-scheduled minute
	{
		for (int i=0;i < 10;i++) oneshots[i]=0;
		at_least_one_slot_has_elapsed=1;  
		if (pctx->_txSched.oscillatorOff && schedule[(current_minute+9)%10]==-1)    // if we want to switch oscillator off and are in non sheduled interval 
		{
			transmitter_status=0;
			PioDCOStop(pctx->_pTX->_p_oscillator);	// Stop the oscilator
		}
	}
	
	else if (is_GPS_available && at_least_one_slot_has_elapsed 
			&& schedule[current_minute]>0
			&& oneshots[current_minute]==0
			&& (at_least_one_GPS_fixed_has_been_obtained!=0) )       //prevent transmission if a location has never been received
		{
			oneshots[current_minute]=1;	
			if (pctx->_txSched.verbosity>=3) printf("\nStarting TX. current minute: %i Schedule Value (packet type): %i\n",current_minute,schedule[current_minute]);
			PioDCOStart(pctx->_pTX->_p_oscillator); 
			transmitter_status=1;
			WSPRbeaconCreatePacket(pctx, schedule[current_minute] ); //the schedule determines packet type (1-4 for U4B 1st msg,U4B 2nd msg,Zachtek 1st, Zachtek 2nd)
			sleep_ms(50);
			WSPRbeaconSendPacket(pctx); 
		}

/*				1 - No valid GPS, not transmitting
				2 - Valid GPS, waiting for time to transmitt
				3 - Valid GPS, transmitting
				4 - no valid GPS, but (still) transmitting anyway */
			if (!is_GPS_active && transmitter_status) pctx->_txSched.led_mode = 4; else
			pctx->_txSched.led_mode = 1 + is_GPS_active + transmitter_status;

			if (previous_msg_count!=is_GPS_available)
			{
			previous_msg_count=is_GPS_available;
			time_of_last_serial_packet= get_absolute_time();
			}

			 if(absolute_time_diff_us(time_of_last_serial_packet, get_absolute_time()) > 3000000ULL) //if more than one or two serial packets are missed something is wrong
			 {
				pctx->_txSched.led_mode = 0;  //no GPS serial Comms
			 }

   return 0;
}
///////////////////////////////////////////////////////////
/// @brief Dumps the beacon context to stdio.
/// @param pctx Ptr to Context.
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx)  //called ~ every 20 secs from main.c
{
    assert_(pctx);
    assert_(pctx->_pTX);

    const uint64_t u64tmnow = GetUptime64();

    StampPrintf("__________________");
   /* StampPrintf("=TxChannelContext=");
    StampPrintf("ftc:%llu", pctx->_pTX->_tm_future_call);
    StampPrintf("ixi:%u", pctx->_pTX->_ix_input);
    StampPrintf("dfq:%lu", pctx->_pTX->_u32_dialfreqhz);
    StampPrintf("gpo:%u", pctx->_pTX->_i_tx_gpio);   */
    GPStimeContext *pGPS = pctx->_pTX->_p_oscillator->_pGPStime;
    /*const uint32_t u32_unixtime_now = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_utime_nmea_last + u64_GPS_last_age_sec;
    assert_(pGPS);
    StampPrintf("=GPStimeContext=");
    StampPrintf("err:%ld", pGPS->_i32_error_count);
    StampPrintf("ixw:%lu", pGPS->_u8_ixw);
    StampPrintf("sol:%u", pGPS->_time_data._u8_is_solution_active);
    StampPrintf("unl:%lu", pGPS->_time_data._u32_utime_nmea_last);
    StampPrintf("snl:%llu", pGPS->_time_data._u64_sysclk_nmea_last);
    StampPrintf("age:%llu", u64_GPS_last_age_sec);
    StampPrintf("utm:%lu", u32_unixtime_now);      
    StampPrintf("rmc:%lu", pGPS->_time_data._u32_nmea_gprmc_count); */ 
    StampPrintf("ppb:%lld", pGPS->_time_data._i32_freq_shift_ppb); 

	StampPrintf("LED Mode: %d",pctx->_txSched.led_mode);
	StampPrintf("Grid: %s",(char *)WSPRbeaconGetLastQTHLocator(pctx));
	StampPrintf("lat: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k);
	StampPrintf("lon: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k);
	StampPrintf("altitude: %f",pctx->_pTX->_p_oscillator->_pGPStime->_altitude);	   
	StampPrintf("current minute: %i",current_minute);	   
}
///////////////////////////////////////////////////////////
/// @brief Extracts maidenhead type QTH locator (such as KO85) using GPS coords.
/// @param pctx Ptr to WSPR beacon context.
/// @return ptr to string of QTH locator (static duration object inside get_mh).
/// @remark It uses third-party project https://github.com/sp6q/maidenhead .
char *WSPRbeaconGetLastQTHLocator(const WSPRbeaconContext *pctx)
{
    assert_(pctx);
    assert_(pctx->_pTX);
    assert_(pctx->_pTX->_p_oscillator);
    assert_(pctx->_pTX->_p_oscillator->_pGPStime);
    
    const double lat = 1e-7 * (double)pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k;  //Roman's original code used 1e-5 instead (bug)
    const double lon = 1e-7 * (double)pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k;  //Roman's original code used 1e-5 instead (bug)
    return get_mh(lat, lon, 6);
}

uint8_t WSPRbeaconIsGPSsolutionActive(const WSPRbeaconContext *pctx)
{
    assert_(pctx);
    assert_(pctx->_pTX);
    assert_(pctx->_pTX->_p_oscillator);
    assert_(pctx->_pTX->_p_oscillator->_pGPStime);

    return YES == pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;
}
///////////////////////////////////////////////////////////
          // used for Zachtek style
char* add_brackets(const char * call)   //adds <> around the callsign. this is what triggers a type 3 message. 
{
	static char temp_holder[20];
	temp_holder[0]=0;
	char first_brack[2]= "<";
	char second_brack[2]= ">";
    strcat(temp_holder, first_brack);  
    strcat(temp_holder, call);         
    strcat(temp_holder, second_brack);
	return temp_holder;
}	
char EncodeBase36(uint8_t val)
    {
        char retVal;

        if (val < 10)
        {
            retVal = '0' + val;
        }
        else
        {
            retVal = 'A' + (val - 10);
        }

        return retVal;
    }
