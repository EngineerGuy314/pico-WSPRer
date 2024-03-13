///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY, PhD
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  conswrapper.c - Serial console commands processing manager.
//
//  DESCRIPTION
//      -
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
//      Rev 0.1   23 Dec 2023   Initial revision.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/uart.h"
#include "./lib/assert.h"
#include "piodco/piodco.h"
#include "protos.h"

extern PioDco DCO;

/// @brief Console commands manager. Currently available:
/// @brief HELP             - Usage.
/// @brief SETFREQ f        - Set oscillator output frequency in Hz.
/// @brief SWITCH ON/OFF    - Switch output to ON or OFF state.
/// @param cmd Ptr to command.
/// @param narg Argument count.
/// @param params Command params, full string.
void ConsoleCommandsWrapper(char *cmd, int narg, char *params)
{
    assert_(cmd);

    if(strstr(cmd, "HELP"))
    {
        printf("\n");
        printf("Pico-hf-oscillator project HELP page\n");
        printf("Copyright (c) 2023 by Roman Piksaykin\n");
        printf("Build date: %s %s\n",__DATE__, __TIME__);
        printf("Project official page: github.com/RPiks/pico-hf-oscillator\n");
        printf("----------------------------------------------------------\n");
        printf("-\n");
        printf("  HELP - this page.\n");
        printf("-\n");
        printf("  STATUS - print system status.\n");
        printf("-\n");
        printf("  SETFREQ f - set output frequency f in Hz.\n");
        printf("  example: SETFREQ 14074010 - set output frequency to 14.074145 MHz.\n");
        printf("-\n");
        printf("  SWITCH s - enable/disable generation.\n");
        printf("  example: SWITCH ON - enable generation.\n");
        printf("-\n");
        printf("  GPSREC OFF/uart_id,pps_pin,baud - enable/disable GPS receiver connection.\n");
        printf("  example: GPSREC 0,3,9600 - enable GPS receiver connection with UART0 & PPS on gpio3, 9600 baud port speed.\n");
        printf("  example: GPSREC OFF - disable GPS receiver connection.\n");
        return;
    } else if(strstr(cmd, "SETFREQ"))
    {
        if(2 != narg)
        {
            PushErrorMessage(-1);
            return;
        }

        const uint32_t ui32frq = atol(params);
        if(ui32frq < 1000000L || ui32frq > 32333333)
        {
            PushErrorMessage(-11);
            return;
        }

        PioDCOSetFreq(&DCO, ui32frq, 0U);
        printf("\nFrequency is set to %lu+.000 Hz", ui32frq);
        return;

    } else if(strstr(cmd, "STATUS"))
    {
        PushStatusMessage();
        return;
    } else if(strstr(cmd, "SWITCH"))
    {
        if(2 != narg)
        {
            PushErrorMessage(-1);
            return;
        }
        if(strstr(params, "ON"))
        {
            PioDCOStart(&DCO);
            printf("\nOutput is enabled");
            return;
        } else if(strstr(params, "OFF"))
        {
            PioDCOStop(&DCO);
            printf("\nOutput is disabled");
            return;
        }
    } else if(strstr(cmd, "GPSREC"))
    {
        if(4 == narg)
        {
            char *p = params;
            const uint32_t ui32uart = atol(p);
            if(0 != ui32uart && 1 != ui32uart)
            {
                PushErrorMessage(-12);
                return;
            }

            p += strlen(p) + 1;
            if(p)
            {
                if(strlen(p))
                {
                    const uint32_t ui32pps = atol(p);
                    p += strlen(p) + 1;
                    const uint32_t ui32baud = atol(p);
                    //printf("\nbaud=%s\n", p);
                    sleep_ms(5);
                    if(DCO._pGPStime)
                    {
                        GPStimeDestroy(&DCO._pGPStime);
                        printf("\nGPS subsystem is disabled.");
                    }
                    GPStimeContext *pGPS = GPStimeInit(ui32uart, ui32baud, ui32pps);
                    assert_(pGPS);
                    DCO._pGPStime = pGPS;
                    printf("\nGPS subsystem is set to UART%lu (%lu baud) & PPS pin%lu", ui32uart, ui32baud, ui32pps);
                    return;
                }
            }

            PushErrorMessage(-1);

            return;

        } else if(2 == narg)
        {
            if(DCO._pGPStime)
            {
                GPStimeDestroy(&DCO._pGPStime);
                printf("\nGPS subsystem is disabled.");
            }

            return;
        }
    }

    PushErrorMessage(-13);
}

void PushErrorMessage(int id)
{
    switch(id)
    {
        case -1:
        printf("\nInvalid argument");
        break;

        case -11:
        printf("\nInvalid frequency");
        break;

        case -12:
        printf("\nInvalid UART id, should be 0 OR 1");
        break;

        case -13:
        printf("\nInvalid command");
        break;

        default:
        printf("\nUnknown error");
        break;
    }
}

void PushStatusMessage(void)
{
    printf("\nPico-hf-oscillator system status\n");
    
    printf("Working freq: %lu Hz + %ld milliHz\n", DCO._ui32_frq_hz, DCO._ui32_frq_millihz);
    
    printf("Output is ");
    if(DCO._is_enabled)
    {
        printf("ENABLED");
    }
    else
    {
        printf("DISABLED");
    }

    if(DCO._pGPStime)
    {
        printf("\nGPS UART id %d", DCO._pGPStime->_uart_id);
        printf("\nGPS UART baud %d", DCO._pGPStime->_uart_baudrate);
        printf("\nGPS PPS GPIO pin %d", DCO._pGPStime->_pps_gpio);
        printf("\nGPS error count %ld", DCO._pGPStime->_i32_error_count);
        printf("\nGPS NAV solution flag %u", DCO._pGPStime->_time_data._u8_is_solution_active);
        printf("\nGPS GPRMC receive count %u", DCO._pGPStime->_time_data._u32_nmea_gprmc_count);
        printf("\nGPS PPS period %llu", DCO._pGPStime->_time_data._u64_pps_period_1M);
        printf("\nGPS frequency correction %lld ppb", DCO._pGPStime->_time_data._i32_freq_shift_ppb);
        printf("\nGPS lat %lld deg1e5", DCO._pGPStime->_time_data._i64_lat_100k);
        printf("\nGPS lon %lld deg1e5", DCO._pGPStime->_time_data._i64_lon_100k);
    }
    else
    {
        printf("\nGPS subsystem hasn't been initialized.");
    }
}
