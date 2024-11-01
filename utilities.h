/////////////////////////////////////////////////////////////////////////////
//  Other functions used in pico-WSPRer
//  Jakub Serych serych@panska.cz
// 
//  PROJECT PAGE
//  https://github.com/EngineerGuy314/pico-WSPRer
/////////////////////////////////////////////////////////////////////////////
#ifndef UTILITIES_H_
#define UTILITIES_H_

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <defines.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   

void get_user_input(const char *prompt, char *input_variable, int max_length);
int InitPicoClock(int PLL_SYS_MHZ);

#endif
