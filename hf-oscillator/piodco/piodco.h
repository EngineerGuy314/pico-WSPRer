///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  piodco.h - Digital controlled radio freq oscillator based on PIO.
// 
//
//  DESCRIPTION
//
//      The oscillator provides precise generation of any frequency ranging
//  from 1 Hz to 33.333 MHz with tenth's of millihertz resolution (please note that
//  this is relative resolution owing to the fact that the absolute accuracy of 
//  onboard crystal of pi pico is limited; the absoulte accuracy can be provided
//  when using GPS reference option included).
//      The DCO uses phase locked loop principle programmed in C and PIO asm.
//      The DCO does *NOT* use any floating point operations - all time-critical
//  instructions run in 1 CPU cycle.
//      Currently the upper freq. limit is about 33.333 MHz and it is achieved only
//  using pi pico overclocking to 270 MHz.
//      Owing to the meager frequency step, it is possible to use 3, 5, or 7th
//  harmonics of generated frequency. Such solution completely cover all HF and
//  a portion of VHF band up to about 233 MHz.
//      Unfortunately due to pure digital freq.synthesis principle the jitter may
//  be a problem on higher frequencies. You should assess the quality of generated
//  signal if you want to emit a noticeable power.
//      This is an experimental project of amateur radio class and it is devised
//  by me on the free will base in order to experiment with QRP narrowband
//  digital modes.
//      I appreciate any thoughts or comments on that matter.
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
// 
//      Rev 0.1   05 Nov 2023   Initial release
//      Rev 0.2   18 Nov 2023
//      Rev 1.0   10 Dec 2023   Improved frequency range (to ~33.333 MHz).
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
#ifndef PIODCO_H_
#define PIODCO_H_

#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include "defines.h"

#include "../gpstime/GPStime.h"

enum PioDcoMode
{
    eDCOMODE_IDLE = 0,          /* No output. */
    eDCOMODE_GPS_COMPENSATED= 2 /* Internally compensated, if GPS available. */
};

typedef struct
{
    enum PioDcoMode _mode;      /* Running mode. */

    PIO _pio;                   /* Worker PIO on this DCO. */
    int _gpio;                  /* Pico' GPIO for DCO output. */

    pio_sm_config _pio_sm;      /* Worker PIO parameter. */
    int _ism;                   /* Index of state maschine. */
    int _offset;                /* Worker PIO u-program offset. */

    int32_t _frq_cycles_per_pi; /* CPU CLK cycles per PI. */

    uint32_t _ui32_pioreg[8];   /* Shift register to PIO. */

    uint32_t _clkfreq_hz;       /* CPU CLK freq, Hz. */

    GPStimeContext *_pGPStime;  /* Ptr to GPS time context. */

    uint32_t _ui32_frq_hz;      /* Working freq, Hz. */
    int32_t _ui32_frq_millihz;  /* Working freq additive shift, mHz. */
    int _is_enabled;

} PioDco;

int PioDCOInit(PioDco *pdco, int gpio, int cpuclkhz);
int PioDCOSetFreq(PioDco *pdco, uint32_t u32_frq_hz, int32_t u32_frq_millihz);
int32_t PioDCOGetFreqShiftMilliHertz(const PioDco *pdco, uint64_t u64_desired_frq_millihz);

void PioDCOStart(PioDco *pdco);
void PioDCOStop(PioDco *pdco);

void PioDCOSetMode(PioDco *pdco, enum PioDcoMode emode);

void RAM (PioDCOWorker)(PioDco *pDCO);
void RAM (PioDCOWorker2)(PioDco *pDCO);

#endif
