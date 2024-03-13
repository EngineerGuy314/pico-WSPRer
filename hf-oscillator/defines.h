///////////////////////////////////////////////////////////////////////////////
//
//  Roman Piksaykin [piksaykin@gmail.com], R2BDY
//  https://www.qrz.com/db/r2bdy
//
///////////////////////////////////////////////////////////////////////////////
//
//
//  defines.h - Macro definitions of the project.
// 
//
//  DESCRIPTION
//
//      .
//
//  PLATFORM
//      Raspberry Pi pico.
//
//  REVISION HISTORY
// 
//      Rev 0.1   05 Nov 2023   Initial release
//      Rev 0.2   18 Nov 2023
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
#ifndef DEFINES_H
#define DEFINES_H

#define DEBUGLOG 1

#define FALSE 0                                     /* Something is false. */
#define TRUE 1                                       /* Something is true. */
#define BAD 0                                         /* Something is bad. */
#define GOOD 1                                       /* Something is good. */
#define INVALID 0                                 /* Something is invalid. */
#define VALID 1                                     /* Something is valid. */
#define NO 0                                          /* The answer is no. */
#define YES 1                                        /* The answer is yes. */
#define OFF 0                                       /* Turn something off. */
#define ON 1                                         /* Turn something on. */

#define RAM __not_in_flash_func         /* Place time-critical func in RAM */
#define RAM_A __not_in_flash("A")        /* Place time-critical var in RAM */

     /* A macro for arithmetic right shifts, with casting of the argument. */
#define iSAR32(arg, rcount) (((int32_t)(arg)) >> (rcount))
#define iSAR64(arg, rcount) (((int64_t)(arg)) >> (rcount))

  /* A macro of multiplication guarantees of doing so using 1 ASM command. */
#define iMUL32ASM(a, b) __mul_instruction((int32_t)(a), (int32_t)(b))

                                          /* Performing the square by ASM. */
#define iSquare32ASM(x) (iMUL32ASM((x), (x)))

#define ABS(x) ((x) > 0 ? (x) : -(x))

#define INVERSE(x) ((x) = -(x))

#define asizeof(a) (sizeof (a) / sizeof ((a)[0]))

#endif
