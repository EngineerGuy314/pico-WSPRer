///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY, PhD
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  hfconsole.c - Serial console of Raspberry Pi Pico.
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
#include "hfconsole.h"

HFconsoleContext *HFconsoleInit(int uart_id, int baud)
{
    assert_(uart_id < 2);
    HFconsoleContext *pctx = calloc(1, sizeof(HFconsoleContext));
    assert_(pctx);

    memset(pctx->buffer, 0, sizeof(pctx->buffer));

    pctx->_uart_id = uart_id;
    pctx->_uart_baudrate = baud;

    return pctx;
}

void HFconsoleDestroy(HFconsoleContext **pp)
{
    if(pp && *pp)
    {
        free(*pp);
        *pp = NULL;
    }
}

void HFconsoleClear(HFconsoleContext *pc)
{
    if(pc)
    {
        memset(pc->buffer, 0, sizeof(pc->buffer));
        pc->ix = 0;
    }
}

int HFconsoleProcess(HFconsoleContext *p, int ms)
{
    const int ichr = getchar_timeout_us(ms);
    if(ichr < 0)
    {
        return -1;
    }

    switch(ichr)
    {
        case 13:
        HFconsoleEmitCommand(p);
        HFconsoleClear(p);
        printf("\n=> ");
        break;

        case 8:
        if(p->ix)
        {
            --p->ix;
        }
        printf("%c", ichr);
        break;

        default:
        p->buffer[p->ix++] = (char)ichr;
        printf("%c", ichr);
        break;
    }
  
    return 0;
}

int HFconsoleEmitCommand(HFconsoleContext *pc)
{
    char *p = strchr(pc->buffer, ' ');
    if(p)
    {
        if(strlen(p) < 1)
        {
            return -1;
        }
        int narg = 0;
        const char* delimiters = ",. ";
        char* token = strtok(pc->buffer, delimiters);
        while (token)
        {
            ++narg;
            token = strtok(NULL, delimiters);
        }

        if(pc->_pfwrapper)
        {
            *p = 0;
            (*pc->_pfwrapper)(pc->buffer, narg, p+1);
        }

        return 0;
    }
    else
    {
        if(pc->_pfwrapper)
        {
            (*pc->_pfwrapper)(pc->buffer, 0, pc->buffer);
        }
    }

    return 0;
}

void HFconsoleSetWrapper(HFconsoleContext *pc, void *pfwrapper)
{
    assert_(pc);
    
    pc->_pfwrapper = pfwrapper;
}
