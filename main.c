//./////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  main.c - The project entry point.
// 
//  DESCRIPTION
//      The pico-WSPR-tx project provides WSPR beacon function using only
//  Pi Pico board. *NO* additional hardware such as freq.synth required.
//  External GPS receiver is optional and serves a purpose of holding
//  WSPR time window order and accurate frequancy drift compensation.
//
//  HOWTOSTART
//      ./build.sh; cp ./build/*.uf2 /media/Pico_Board/
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
//      Rev 0.1   18 Nov 2023
//      Rev 0.5   02 Dec 2023
//
//  PROJECT PAGE
//      https://github.com/RPiks/pico-WSPR-tx
//
//  SUBMODULE PAGE
//      https://github.com/RPiks/pico-hf-oscillator
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

#define CONFIG_GPS_SOLUTION_IS_MANDATORY YES
#define CONFIG_GPS_RELY_ON_PAST_SOLUTION NO
#define CONFIG_SCHEDULE_SKIP_SLOT_COUNT 5  //a zero *should* xmit each slot. 5 does every 10 mins
#define CONFIG_WSPR_DIAL_FREQUENCY 14097100UL //the real "dial" freq for 20m wspr is 14.0956 Mhz. But you must add 1500Hz because of reasons
#define CONFIG_CALLSIGN "your-callsign/3"   //if doing a slash, doesnt xmit 4 char grid (only 6 char on 2nd one)- that is normal! wsprnet fills it in anyway
#define CONFIG_LOCATOR4 "AA00"       //gets overwritten by gps data anyway

WSPRbeaconContext *pWSPR;

int main()
{
    StampPrintf("\n");
    sleep_ms(5000);
    StampPrintf("pico WSPR v4");

    InitPicoHW();

    PioDco DCO = {0};

    StampPrintf("WSPR beacon init...");

    WSPRbeaconContext *pWB = WSPRbeaconInit(
        CONFIG_CALLSIGN,/* the Callsign. */
        CONFIG_LOCATOR4,/* the default QTH locator if GPS isn't used. */
        12,             /* Tx power, dbm. */
        &DCO,           /* the PioDCO object. */
        CONFIG_WSPR_DIAL_FREQUENCY,
        55UL,           /* the carrier freq. shift relative to dial freq. */
        RFOUT_PIN       /* RF output GPIO pin. */
        );
    assert_(pWB);
    pWSPR = pWB;
    
    pWB->_txSched._u8_tx_GPS_mandatory  = CONFIG_GPS_SOLUTION_IS_MANDATORY;
    pWB->_txSched._u8_tx_GPS_past_time  = CONFIG_GPS_RELY_ON_PAST_SOLUTION;
    pWB->_txSched._u8_tx_slot_skip      = CONFIG_SCHEDULE_SKIP_SLOT_COUNT;

    multicore_launch_core1(Core1Entry);
    StampPrintf("RF oscillator started.");

    DCO._pGPStime = GPStimeInit(0, 9600, GPS_PPS_PIN); //GPIO 1 (pin 2 on pico) will be the serial input from GPS
    assert_(DCO._pGPStime);

    int tick = 0;
    for(;;)   //loop every second
    {
        //GET MAIDENHEAD       - this code in original fork wasnt working due to error in WSPRbeacon.c
        if(WSPRbeaconIsGPSsolutionActive(pWB))
        {
            const char *pgps_qth = WSPRbeaconGetLastQTHLocator(pWB);
            if(pgps_qth)
            {
                strncpy(pWB->_pu8_locator, pgps_qth, 6);     //trying full maidenhead - 
                pWB->_pu8_locator[7] = 0x00;                 //null terminates
            }
        }
        
        WSPRbeaconTxScheduler(pWB, YES);   
                
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        sleep_ms(100);
        gpio_put(PICO_DEFAULT_LED_PIN, 0);

#ifdef DEBUG
        if(0 == ++tick % 20)                //every ~20 secs dumps context
            WSPRbeaconDumpContext(pWB);
#endif
        sleep_ms(900);
    }
}
