///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  WSPRbeacon.c - WSPR beacon - related functions.
// 
//  DESCRIPTION
//      The pico-WSPR-tx project provides WSPR beacon function using only
//  Pi Pico board. *NO* additional hardware such as freq.synth required.
//
//  HOWTOSTART
//  .
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
// 
//      Rev 0.1   18 Nov 2023
//  Initial release.
//
//  PROJECT PAGE
//      https://github.com/RPiks/pico-WSPR-tx
//
//  LICENCE
//      MIT License (http://www.opensource.org/licenses/mit-license.php)
//
//  Copyright (c) 2023 by Roman Piksaykin
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
#include "WSPRbeacon.h"
#include <WSPRutility.h>
#include <maidenhead.h>


static absolute_time_t start_time;
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
void WSPRbeaconSetDialFreq(WSPRbeaconContext *pctx, uint32_t freq_hz)
{
    assert_(pctx);
    pctx->_pTX->_u32_dialfreqhz = freq_hz;
}

/// @brief Constructs a new WSPR packet using the data available.
/// @param pctx Context
/// @return 0 if OK.
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int time_slot)
{
    assert_(pctx);

    pctx->_u8_txpower = pctx->_pTX->_p_oscillator->_pGPStime->_power_altitude; // this moves the PWR value (which has encoded altitude)
 
   if (time_slot==0)
   {
	wspr_encode(pctx->_pu8_callsign, pctx->_pu8_locator, pctx->_u8_txpower, pctx->_pu8_outbuf);   // look in WSPRutility.c
   }
   else
   {
	wspr_encode(add_brackets(pctx->_pu8_callsign), pctx->_pu8_locator, pctx->_u8_txpower, pctx->_pu8_outbuf); // add < and > around callsign for the 2nd part of a Type 3 message
   }
     
    return 0;
}
///////////////////////////////////////////////////////////

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


/// @brief Sends a prepared WSPR packet using TxChannel.
/// @param pctx Context.
/// @return 0, if OK.
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

/// @brief Arranges WSPR sending in accordance with pre-defined schedule.
/// @brief It works only if GPS receiver available (for now).
/// @param pctx Ptr to Context.
/// @param verbose Whether stdio output is needed.
/// @return 0 if OK, -1 if NO GPS received available
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose)   // called every second from Main.c
{
	assert_(pctx);                 	

    const uint64_t u64tmnow = GetUptime64();
    const uint32_t is_GPS_available = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_nmea_gprmc_count;
    const uint32_t is_GPS_active = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u8_is_solution_active;
    const uint32_t is_GPS_override = pctx->_txSched._u8_tx_GPS_past_time == YES;

    const uint64_t u64_GPS_last_age_sec 
        = (u64tmnow - pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u64_sysclk_nmea_last) / 1000000ULL;



 if (pctx->_txSched.force_xmit_for_testing) {             // && is_GPS_active 
						StampPrintf("> FORCING XMISSION!   <"); pctx->_txSched.led_mode = 2;
						PioDCOStart(pctx->_pTX->_p_oscillator);
						WSPRbeaconCreatePacket(pctx,0);
						sleep_ms(100);
						WSPRbeaconSendPacket(pctx);
 }
 
     if(!is_GPS_available && pctx->_txSched.GPS_is_OFF_running_blind==0)
    {
        StampPrintf(" Waiting for GPS receiver...");pctx->_txSched.led_mode = 0;  //waiting for GPS
        return -1;
    }
 
 
	if((pctx->_txSched.GPS_is_OFF_running_blind>0)
		||  ((is_GPS_active || (pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_utime_nmea_last && is_GPS_override && u64_GPS_last_age_sec < WSPR_MAX_GPS_DISCONNECT_TM)) 
	     && (pctx->_txSched.force_xmit_for_testing==0))		 
       )
    {
		if(!is_GPS_active && pctx->_txSched.GPS_is_OFF_running_blind==0) StampPrintf("Gps was available, but wasnt active yet. led-mode XMIT %d %d ",pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);
					  else StampPrintf("gps is ACTIVE amd AVAILABLE. ledmode XMIT %d %d",pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);				  
		
		const uint32_t u32_unixtime_now 
            = pctx->_pTX->_p_oscillator->_pGPStime->_time_data._u32_utime_nmea_last + u64_GPS_last_age_sec;        
        const int isec_of_hour = u32_unixtime_now % HOUR;
        const int islot_number = isec_of_hour / (2 * MINUTE);
              int islot_modulo = islot_number % pctx->_txSched._u8_tx_slot_skip;
        const int islot_modulo2 = islot_number % (pctx->_txSched._u8_tx_slot_skip+1);

		static int itx_trigger = 0;
        static int itx_trigger2 = 0;		
	
		///if running_blind  on, set modulo 0, then fter 2 mins force modulo to 1, after 2 more minutes make it 99 (which will restart normal operation)]   ULL	
		if(pctx->_txSched.GPS_is_OFF_running_blind>0)
		{
			islot_modulo =99;
			if (absolute_time_diff_us( start_time, get_absolute_time()) < 240000000ULL) islot_modulo =1;
			if (absolute_time_diff_us( start_time, get_absolute_time()) < 120000000ULL) islot_modulo =0;			
		}
		
				if(islot_modulo == ZERO )  //top of the ten minute period
				{
					if(!itx_trigger)   //oneshot right at beginning of slot
					{
						gpio_put(GPS_ENABLE_PIN, 0); // Kill GPS to Save Power
						start_time = get_absolute_time();
						pctx->_txSched.GPS_is_OFF_running_blind=1;   //for the next 4 minutes no GPS time updates are coming
						
						//PioDCOStop(pctx->_pTX->_p_oscillator); printf("Pio-Stop called by modulo 0\n");//make sure to kill the previous message, just in case
						itx_trigger = 1;
						if(verbose) StampPrintf(">           Start TX.            <");
						PioDCOStart(pctx->_pTX->_p_oscillator); printf("Pio START called by modulo 0\n");
						
						WSPRbeaconCreatePacket(pctx,islot_modulo);
						sleep_ms(100);
						WSPRbeaconSendPacket(pctx); 
						pctx->_txSched.led_mode = 2;  //xmitting
						
					}
				}
				
				else if(islot_modulo == 1 )  //time for 2nd half of Type 3 msg
				{
					if(!itx_trigger2)   //oneshot right at beginning of slot 
					{                                  
						PioDCOStop(pctx->_pTX->_p_oscillator); printf("Pio-Stop called by modulo 1 \n");//make sure to kill the previous message
						itx_trigger2 = 1;
						if(verbose) StampPrintf(">>>>>>                 Start SECOND TX.   <<<<<<<<<<<");
						PioDCOStart(pctx->_pTX->_p_oscillator);   printf("Pio START called by modulo 1 \n");
						WSPRbeaconCreatePacket(pctx,islot_modulo);
						sleep_ms(100);
						WSPRbeaconSendPacket(pctx);
						pctx->_txSched.led_mode = 2;  //xmitting
					}
				}
				
				else //its not the zero or the 2 minute slot
				{
					itx_trigger = 0;	itx_trigger2 = 0;   //reset oneshots					
					pctx->_txSched.led_mode = 1;  //not xmitting, waiting for SLOT
					if(verbose) StampPrintf("..   WAITING  for right time slot to start xmit..modulo and led-mode and XMIT %d %d %d\n",islot_modulo,pctx->_txSched.led_mode,pctx->_pTX->_p_oscillator->_is_enabled);
					PioDCOStop(pctx->_pTX->_p_oscillator); printf("Pio *STOP*  called by else. modulo: %d\n",islot_modulo);
					gpio_put(GPS_ENABLE_PIN, 1); // re-enable GPS 
					pctx->_txSched.GPS_is_OFF_running_blind=0; // back to normal
				}

    }
	else
	{
	   StampPrintf("GPS was available, but not active. led mode 0, XMIT status: %d",pctx->_pTX->_p_oscillator->_is_enabled);  //hmm, if this happens during a xmission, the oneshots wont clear and oscillatror will remain active and rf output will be CW. not a big deal i guess, it will self recover eventually
		pctx->_txSched.led_mode = 0;
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
    StampPrintf("pps:%llu", pGPS->_time_data._u64_sysclk_pps_last);
    StampPrintf("ppb:%lld", pGPS->_time_data._i32_freq_shift_ppb);

	StampPrintf("LED Mode: %d",pctx->_txSched.led_mode);
	StampPrintf("Grid: %s",(char *)WSPRbeaconGetLastQTHLocator(pctx));
	StampPrintf("lat: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lat_100k);
	StampPrintf("lon: %lli",pctx->_pTX->_p_oscillator->_pGPStime->_time_data._i64_lon_100k);
	StampPrintf("PWR (encoded altitude): %i",pctx->_pTX->_p_oscillator->_pGPStime->_power_altitude);	   
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
