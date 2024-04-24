///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY, PhD
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  gpstime.h - GPS time reference utilities for digital controlled radio freq
//              oscillator based on Raspberry Pi Pico.
//
//  DESCRIPTION
//
//      GPS time utilities for pico-hf-oscillator calculate a precise frequency
//  shift between the local Pico oscillator and reference oscill. of GPS system.
//  The value of the shift is used to correct generated frequency. The practical 
//  precision of this solution depends on GPS receiver's time pulse stability, 
//  as well as on quality of navigation solution of GPS receiver. 
//  This quality can be estimated by GDOP and TDOP parameters received 
//  in NMEA-0183 message packet from GPS receiver.
//      Owing to the meager frequency step in millihertz range, we obtain
//  a quasi-analog precision frequency source (if the GPS navigation works OK).
//      This is an experimental project of amateur radio class and it is devised
//  by me on the free will base in order to experiment with QRP narrowband
//  digital modes including extremely ones such as QRSS.
//      I appreciate any thoughts or comments on that matter.
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
// 
//      Rev 0.1   25 Nov 2023   Initial release
//
//  PROJECT PAGE
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
#ifndef GPSTIME_H_
#define GPSTIME_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "../defines.h"
#include "../lib/assert.h"
#include "../lib/utility.h"
#include "../lib/thirdparty/strnstr.h"

#define ASSERT_(x) assert_(x)

enum
{
    eDtUpscale = 1000000,
    eSlidingLen = 32,			   //size of buffer used to compensated
    eCLKperTimeMark = 1000000,
    eMaxCLKdevPPM = 250           // max error it will try to compensate for
};

typedef struct
{
    uint8_t _u8_is_solution_active;             /* A navigation solution is valid. */
    char _u8_last_digit_minutes;             // First digit of the minutes. Really, this is the only thing i care about. 
    uint32_t _u32_utime_nmea_last;              /* The last unix time received from GPS. */
    uint64_t _u64_sysclk_nmea_last;             /* The sysclk of the last unix time received. */
    int64_t _i64_lat_100k, _i64_lon_100k;       /* The lat, lon, degrees, multiplied by 1e5. */
    uint32_t _u32_nmea_gprmc_count;             /* The count of $GPRMC sentences received */

    uint64_t _u64_sysclk_pps_last;              /* The sysclk of the last rising edge of PPS. */
    uint64_t _u64_pps_period_1M;                /* The PPS avg. period *1e6, filtered. */

    uint64_t _pu64_sliding_pps_tm[eSlidingLen]; /* A sliding window to store PPS periods. */
    uint8_t _ix_last;                           /* An index of last write to sliding window. */

    int64_t _i32_freq_shift_ppb;                /* Calcd frequency shift, parts per billion. */

} GPStimeData;

typedef struct
{
    int _uart_id;
    int _uart_baudrate;
    int _pps_gpio;

    GPStimeData _time_data;

    uint8_t _pbytebuff[256];  
    uint8_t _u8_ixw;
    uint8_t _is_sentence_ready;
    int32_t _i32_error_count;
    float _power_altitude;   //altitude in metesr
	uint8_t enable_debug_messages;
	uint8_t forced_XMIT_on;
	int8_t temp_in_Celsius;

} GPStimeContext;

GPStimeContext *GPStimeInit(int uart_id, int uart_baud, int pps_gpio);
void GPStimeDestroy(GPStimeContext **pp);

int GPStimeProcNMEAsentence(GPStimeContext *pg);
void extract_altitude(GPStimeContext *pg);

void RAM (GPStimePPScallback)(uint gpio, uint32_t events);
void RAM (GPStimeUartRxIsr)();

int GPStimeGetTime(const GPStimeContext *pg, uint32_t *u32_tmdst);
uint32_t GPStime2UNIX(const char *pdate, const char *ptime);

void GPStimeDump(const GPStimeData *pd);

#endif
