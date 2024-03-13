///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY, PhD
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  hfconsole.h - Serial console of Raspberry Pi Pico.
//
//  DESCRIPTION
//      -
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
//      Rev 0.1   16 Dec 2023   Initial revision.
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
#ifndef HFTERM_H_
#define HFTERM_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "../lib/assert.h"
#include "../lib/utility.h"

typedef struct
{
    int _uart_id;           // UART id (-1 when use Pico USB port)
    int _uart_baudrate;     // UART baud rate (isn't used when uaer id is 0)

    void (*_pfwrapper)(char *, int, char *);

    char buffer[256];
    uint8_t ix;

} HFconsoleContext;

HFconsoleContext *HFconsoleInit(int uart_id, int baud);
void HFconsoleDestroy(HFconsoleContext **pp);
int HFconsoleProcess(HFconsoleContext *p, int ms);
int HFconsoleEmitCommand(HFconsoleContext *pc);
void HFconsoleSetWrapper(HFconsoleContext *pc, void *pfwrapper);
void HFconsoleClear(HFconsoleContext *pc);

#endif
