///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY, PhD
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  utility.h - Utilities for RPiks' projects.
//
//  DESCRIPTION
//      -
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
#ifndef UTILITY_H_
#define UTILITY_H_

#include <stdint.h>
#include "pico/stdlib.h"

inline uint64_t GetUptime64(void)
{
    const uint32_t lo = timer_hw->timelr;
    const uint32_t hi = timer_hw->timehr;

    return ((uint64_t)hi << 32U) | lo;
}

inline uint32_t GetTime32(void)
{
    return timer_hw->timelr;
}

inline uint32_t PicoU64timeToSeconds(uint64_t u64tm)
{
    return u64tm / 1000000U;    // No rounding deliberately!
}

inline uint32_t DecimalStr2ToNumber(const char *p)
{
    return 10U * (p[0] - '0') + (p[1] - '0');
}

inline void PRN32(uint32_t *val)
{ 
    *val ^= *val << 13;
    *val ^= *val >> 17;
    *val ^= *val << 5;
}

#endif
