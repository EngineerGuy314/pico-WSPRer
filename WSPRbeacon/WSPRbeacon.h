///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  WSPRbeacon.h - WSPR beacon - related functions.
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
#ifndef WSPRBEACON_H_
#define WSPRBEACON_H_

#include <stdint.h>
#include <string.h>
#include <TxChannel.h>
#include "../../debug/logutils.h"

typedef struct
{
 
	uint8_t force_xmit_for_testing;
    uint8_t led_mode;
	uint8_t suffix;
    char id13[3];
    int8_t temp_in_Celsius;
	int8_t verbosity;
    double voltage;
    double voltage_at_idle;
    double voltage_at_xmit;
    int8_t oscillatorOff;
	uint32_t TELEN1_val1;
	uint32_t TELEN1_val2;
	uint32_t TELEN2_val1;
	uint32_t TELEN2_val2;
	uint32_t minutes_since_boot;
	uint32_t minutes_since_GPS_aquisition;
	uint8_t low_power_mode;

} WSPRbeaconSchedule;


typedef struct
{
	uint32_t value;
	uint32_t range;
} v_and_r;

typedef struct
{
    uint8_t _pu8_callsign[12];
    uint8_t _pu8_locator[7];
    uint8_t _u8_txpower;
    uint8_t _pu8_outbuf[256];
    TxChannelContext *_pTX;
    WSPRbeaconSchedule _txSched;
	char telem_callsign[7];
	char telem_4_char_loc[5];
	uint8_t telem_power;	
	char telem_chars[8];
	v_and_r telem_vals_and_ranges[5][10];    //slot and param number
	uint64_t Big64;
	uint8_t grid7;
	uint8_t grid8;
	uint8_t grid9;
	uint8_t grid10;
	
} WSPRbeaconContext;


WSPRbeaconContext *WSPRbeaconInit(const char *pcallsign, const char *pgridsquare, int txpow_dbm,
                                  PioDco *pdco, uint32_t dial_freq_hz, uint32_t shift_freq_hz,
                                  int gpio,  uint8_t start_minute,  uint8_t id13 ,  uint8_t suffix,const char *DEXT_config);
void WSPRbeaconSetDialFreq(WSPRbeaconContext *pctx, uint32_t freq_hz);
int WSPRbeaconCreatePacket(WSPRbeaconContext *pctx,int packet_type);
char* add_brackets(const char * call);
int WSPRbeaconSendPacket(const WSPRbeaconContext *pctx);
char EncodeBase36(uint8_t val);
int WSPRbeaconTxScheduler(WSPRbeaconContext *pctx, int verbose, int GPS_PPS_PIN);
void WSPRbeaconDumpContext(const WSPRbeaconContext *pctx);
char *WSPRbeaconGetLastQTHLocator(WSPRbeaconContext *pctx);
uint8_t WSPRbeaconIsGPSsolutionActive(const WSPRbeaconContext *pctx);
void encode_telen(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void encode_telen2(uint32_t telen_val1,uint32_t telen_val2,char * telen_chars,uint8_t * telen_power, uint8_t packet_type);  
void telem_add_values_to_Big64(int slot, WSPRbeaconContext *c); 
void telem_add_header(int slot, WSPRbeaconContext *c);
void telem_convert_Big64_to_GridLocPower(WSPRbeaconContext *c);
int calc_solar_angle(int hour, int min, int64_t int_lat, int64_t int_lon);
#endif
