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
static absolute_time_t start_time;
static int current_minute;
/// @brief Initializes a new WSPR beacon context.
/// @param pcallsign HAM radio callsign, 12 chr max.
/// @param pgridsquare Maidenhead locator, 7 chr max.
/// @param txpow_dbm TX power, db`mW.
/// @param pdco Ptr to working DCO.
/// @param dial_freq_hz The begin of working WSPR passband.
/// @param shift_freq_hz The shift of tx freq. relative to dial_freq_hz.
/// @param gpio Pico's GPIO pin of RF output.
/// @return Ptr to the new context.
WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm,
                                  PioDco *pdco, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio)
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
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int time_slot)
{
    assert_(pctx);
    pctx->_u8_txpower =0;

   if (time_slot==0)
   {
	printf("creating packet, modulo is %i\n",time_slot);
	char _4_char_version_of_locator[4];
	strncpy(_4_char_version_of_locator, pctx->_pu8_locator, 4);     //only take first 4 chars of locator
	_4_char_version_of_locator[4]=0; //null terminate end
	
	wspr_encode(pctx->_pu8_callsign, _4_char_version_of_locator, pctx->_u8_txpower, pctx->_pu8_outbuf);   // look in WSPRutility.c for wspr_encode
   }
   else  //if its not the first timeslot, do special encoding for U4B protocol
   {
	printf("creating packet, modulo is %i\n",time_slot);
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
		uint16_t altFracM =  round((double)pctx->_pTX->_p_oscillator->_pGPStime->_power_altitude / 20);     

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
        uint8_t speedKnotsNum = 0; // NOT USED FOR NOW, wuz:  round((double)speedKnots / 2.0);
        uint8_t gpsValidNum   = 1; // NOT USED FOR NOW, wuz: gpsValid ? 1 : 0;
        // shift inputs into a big number
        val = 0;

        val *= 90; val += tempCNum;
        val *= 40; val += voltageNum;
        val *= 42; val += speedKnotsNum;
        val *=  2; val += gpsValidNum;
        val *=  2; val += 1;                // standard telemetry
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
	
		switch( powerVal)
		{
		case 0: power_U4B=0;
				break;
		case 1: power_U4B=3;
				break;
		case 2: power_U4B=7;
				break;
		case 3: power_U4B=10;
				break;
		case 4: power_U4B=13;
				break;
		case 5: power_U4B=17;
				break;
		case 6: power_U4B=20;
				break;
		case 7: power_U4B=23;
				break;
		case 8: power_U4B=27;
				break;
		case 9: power_U4B=30;
				break;
		case 10: power_U4B=33;
				break;
		case 11: power_U4B=37;
				break;
		case 12: power_U4B=40;
				break;
		case 13: power_U4B=43;
				break;
		case 14: power_U4B=47;
				break;
		case 15: power_U4B=50;
				break;
		case 16: power_U4B=53;
				break;
		case 17: power_U4B=57;
				break;
		case 18: power_U4B=60;
				break;
		}

	wspr_encode(CallsignU4B, Grid_U4B, power_U4B, pctx->_pu8_outbuf); 
   }
     
    return 0;
}
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
    pctx->_pTX->_ix_input = WSPR_SYMBOL_COUNT;
    return 0;
}
//******************************************************************************************************************************
/// @brief Arranges WSPR sending in accordance with pre-defined schedule.
/// @brief It works only if GPS receiver available (for now).
/// @param pctx Ptr to Context.
/// @param verbose Whether stdio output is needed.
/// @return 0 if OK, -1 if NO GPS received available
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose)   // called every second from Main.c
{
	assert_(pctx);                 	
    const uint32_t is_GPS_available = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_nmea_gprmc_count;
    const uint32_t is_GPS_active = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;

		 if (pctx->_txSched.force_xmit_for_testing) {             // && is_GPS_active 
								StampPrintf("> FORCING XMISSION! for debugging   <"); pctx->_txSched.led_mode = 2;
								PioDCOStart(pctx->_pTX->_p_oscillator);
								WSPRbeaconCreatePacket(pctx,0);
								sleep_ms(100);
								WSPRbeaconSendPacket(pctx);
								return -1;
		 }
 
		 if(!is_GPS_available)
		{
			StampPrintf(" Waiting for GPS receiver...");pctx->_txSched.led_mode = 0;  //waiting for GPS
			return -1;
		}
	 
		if(!is_GPS_active && pctx->_txSched.Xmission_In_Process==0) 
			{   StampPrintf("Gps was available, but wasnt active yet. ledmode %d XMIT status %d",pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);
				return -1;
			}
		else 
		{
			pctx->_txSched.led_mode = 1;
			StampPrintf("gps is ACTIVE amd AVAILABLE. ledmode %d XMIT status %d",pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);				  
		}
		
		
		current_minute = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_last_digit_minutes - 48;  //convert from char to int

		if( (pctx->_txSched.Xmission_In_Process==FALSE) && (pctx->_txSched.start_minute==current_minute) ) 
		{                                              //if not yet transmitting, and current minute == starting_minute, then enable xmission
			printf("XMIT about to Start! start minute and current minute: >%i< >%i<\n\n",pctx->_txSched.start_minute,current_minute);
			pctx->_txSched.Xmission_In_Process=1;
			start_time = get_absolute_time();
		}			
		
		if(pctx->_txSched.Xmission_In_Process==TRUE) 
		{	
				if(absolute_time_diff_us( start_time, get_absolute_time()) < 120000000ULL)     //first timeslot
				{
					if(!itx_trigger)   //oneshot right at beginning of slot
					{
						//we get here once, and only once, at top of modulo 0												
						pctx->_txSched.Xmission_In_Process=1;   
						itx_trigger = 1;
						if(verbose) StampPrintf(">      >    >     Start TX.  <   <       <");
						PioDCOStart(pctx->_pTX->_p_oscillator); printf("Pio START called by modulo 0\n");						
						WSPRbeaconCreatePacket(pctx,0);
						sleep_ms(100);
						WSPRbeaconSendPacket(pctx); 
						pctx->_txSched.led_mode = 2;  //xmitting						
					}
				}				
				else if(absolute_time_diff_us( start_time, get_absolute_time()) > 120000000ULL)  //time for 2nd (telemetry) message
				{
					if(!itx_trigger2)   //oneshot right at beginning of slot 
					{                                  
						PioDCOStop(pctx->_pTX->_p_oscillator); printf("Pio-Stop called by modulo 1 \n");//make sure to kill the previous message
						itx_trigger2 = 1;
						if(verbose) StampPrintf(">>>>> >                   Start SECOND TX.         < <<<<<<<<<<");
						PioDCOStart(pctx->_pTX->_p_oscillator);        printf("Pio START called by modulo 1 \n");
						WSPRbeaconCreatePacket(pctx,1);
						sleep_ms(100);
						WSPRbeaconSendPacket(pctx);
						pctx->_txSched.led_mode = 2;  //xmitting
					}
				}				
				
				if (absolute_time_diff_us( start_time, get_absolute_time()) > 240000000ULL) //4 minutes past, time to killl it
				{
					itx_trigger = 0;	itx_trigger2 = 0;   //reset oneshots					
					pctx->_txSched.led_mode = 1;  //not xmitting, waiting for SLOT
					if(verbose) StampPrintf("..   WAITING  for right time slot to start xmit..modulo and led-mode and XMIT %d %d %d",current_minute,pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);
					PioDCOStop(pctx->_pTX->_p_oscillator); printf("Pio *STOP*  called by else. modulo: %d\n",current_minute);
					pctx->_txSched.Xmission_In_Process=0; // back to normal
				}
		}
    return 0;
}

/// @brief Dumps the beacon context to stdio.
/// @param pctx Ptr to Context.
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx)  //called ~ every 20 secs from main.c
{
    assert_(pctx);
    assert_(pctx->_pTX);

    const uint64_t u64tmnow = GetUptime64();
    const uint64_t u64_GPS_last_age_sec 
        = (u64tmnow - pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u64_sysclk_nmea_last) / 1000000ULL;

    StampPrintf("__________________");
   /* StampPrintf("=TxChannelContext=");
    StampPrintf("ftc:%llu", pctx->_pTX->_tm_future_call);
    StampPrintf("ixi:%u", pctx->_pTX->_ix_input);
    StampPrintf("dfq:%lu", pctx->_pTX->_u32_dialfreqhz);
    StampPrintf("gpo:%u", pctx->_pTX->_i_tx_gpio);   */
    /*GPStimeContext *pGPS = pctx->_pTX->_p_oscillator->_pGPStime;
    const uint32_t u32_unixtime_now = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_utime_nmea_last + u64_GPS_last_age_sec;
    assert_(pGPS);
    StampPrintf("=GPStimeContext=");
    StampPrintf("err:%ld", pGPS->_i32_error_count);
    StampPrintf("ixw:%lu", pGPS->_u8_ixw);
    StampPrintf("sol:%u", pGPS->_time_data._u8_is_solution_active);
    StampPrintf("unl:%lu", pGPS->_time_data._u32_utime_nmea_last);
    StampPrintf("snl:%llu", pGPS->_time_data._u64_sysclk_nmea_last);
    StampPrintf("age:%llu", u64_GPS_last_age_sec);
    StampPrintf("utm:%lu", u32_unixtime_now);      
    StampPrintf("rmc:%lu", pGPS->_time_data._u32_nmea_gprmc_count); 
    StampPrintf("pps:%llu", pGPS->_time_data._u64_sysclk_pps_last);
    StampPrintf("ppb:%lld", pGPS->_time_data._i32_freq_shift_ppb); */

	StampPrintf("LED Mode: %d",pctx->_txSched.led_mode);
	StampPrintf("Grid: %s",(char *)WSPRbeaconGetLastQTHLocator(pctx));
	StampPrintf("lat: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k);
	StampPrintf("lon: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k);
	StampPrintf("altitude: %f",pctx->_pTX->_p_oscillator->_pGPStime->_power_altitude);	   
	StampPrintf("current minute: %i",current_minute);	   
}

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
          //no longer used. was used for Zachtek style in v1,v2
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