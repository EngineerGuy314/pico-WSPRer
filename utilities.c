/////////////////////////////////////////////////////////////////////////////
//  Other functions used in pico-WSPRer
//  Jakub Serych serych@panska.cz
// 
//  PROJECT PAGE
//  https://github.com/EngineerGuy314/pico-WSPRer
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <defines.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"   
#include "TxChannel.h"


static TxChannelContext *spOsc = NULL;

/**
 * @brief Improoved user input function
 * 
 * @param prompt Prompt to display to user 
 * @param input_variable Variable to which we want to read input
 * @param max_length Maximum length of input string
 */
void get_user_input(const char *prompt, char *input_variable, int max_length) {
    int index = 0;
    int ch;
    
    printf("%s", prompt);  // Display the prompt to the user
    fflush(stdout);

    while (1) {
        ch = getchar();
        if (ch == '\n' || ch == '\r') {  // Enter key pressed
            break;
        } else if (ch == 127 || ch == 8) {  // Backspace key pressed (127 for most Unix, 8 for Windows)
            if (index > 0) {
                index--;
                printf("\b \b");  // Move back, print space, move back again
            }
        } else if (isprint(ch)) {
            if (index < max_length - 1) {  // Ensure room for null terminator
                input_variable[index++] = ch;
                printf("%c", ch);  // Echo character
            }
        }
        fflush(stdout);
    }

    input_variable[index] = '\0';  // Null-terminate the string
    printf("\n");
}

/*                                              -- hardcoded at 48Mhz for Kazu's PLL
void InitPicoClock(int PLL_SYS_MHZ)
{
    const uint32_t clkhz = PLL_SYS_MHZ * 1000000L;
	 printf("\n ABOUT TO SET SYSTEM KLOCK TO %dMhz\n", PLL_SYS_MHZ); 
	
    set_sys_clock_khz(clkhz / kHz, true);

    clock_configure(clk_peri, 0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    PLL_SYS_MHZ * MHZ,
                    PLL_SYS_MHZ * MHZ);
					
}
*/


