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

/// @brief Initializes Pi pico clock.
void InitPicoClock(void)
{

    const uint32_t clkhz = PLL_SYS_MHZ * 1000000L;
    set_sys_clock_khz(clkhz / kHz, true);

    clock_configure(clk_peri, 0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
                    PLL_SYS_MHZ * MHZ,
                    PLL_SYS_MHZ * MHZ);
}

/**
 * @brief Initializes Pico pins
 * 
 */
void InitPicoPins(void)
{
    gpio_init(LED_PIN); 
	gpio_set_dir(LED_PIN, GPIO_OUT); //initialize LED output

    gpio_init(PICO_VSYS_PIN);  		//Prepare ADC to read Vsys
	gpio_set_dir(PICO_VSYS_PIN, GPIO_IN);
	gpio_set_pulls(PICO_VSYS_PIN,0,0);
    adc_init();
    adc_set_temp_sensor_enabled(true); 	//Enable the onboard temperature sensor

    // RF pins are initialised in /hf-oscillator/dco2.pio. Here is only pads setting
    // trying to set the power of RF pads to maximum and slew rate to fast (Chapter 2.19.6.3. Pad Control - User Bank in the RP2040 datasheet)
    // possible values: PADS_BANK0_GPIO0_DRIVE_VALUE_12MA, ..._8MA, ..._4MA, ..._2MA
    // values of constants are the same for all the pins, so doesn't matter if we use PADS_BANK0_GPIO6_DRIVE_VALUE_12MA or ..._GPIO0_DRIVE...
    /*  Measurements have shown that the drive value and slew rate settings do not affect the output power. Therefore, the lines are commented out.
    hw_write_masked(&padsbank0_hw->io[RFOUT_PIN],
                (PADS_BANK0_GPIO0_DRIVE_VALUE_12MA << PADS_BANK0_GPIO0_DRIVE_LSB) || PADS_BANK0_GPIO0_SLEWFAST_FAST,
                PADS_BANK0_GPIO0_DRIVE_BITS || PADS_BANK0_GPIO0_SLEWFAST_BITS);           // first RF pin 
    hw_write_masked(&padsbank0_hw->io[RFOUT_PIN+1],
                (PADS_BANK0_GPIO0_DRIVE_VALUE_12MA << PADS_BANK0_GPIO0_DRIVE_LSB) || PADS_BANK0_GPIO0_SLEWFAST_FAST,
                PADS_BANK0_GPIO0_DRIVE_BITS || PADS_BANK0_GPIO0_SLEWFAST_BITS);           // second RF pin
    */            

	gpio_init(GPS_ENABLE_PIN); gpio_set_dir(GPS_ENABLE_PIN, GPIO_OUT); //initialize GPS enable output output
	gpio_put(GPS_ENABLE_PIN, 1); 									   // to power up GPS unit
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN, GPIO_OUT); //alternate way to enable the GPS is to pull down its ground (aka low-side drive) using 3 GPIO in parallel (no mosfet needed). 2 do: make these non-hardcoded
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+1); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+1, GPIO_OUT); //no need to actually write a value to these outputs. Just enabling them as outputs is fine, they default to the off state when this is done. perhaps thats a dangerous assumption? 
	gpio_init(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+2); gpio_set_dir(GPS_ALT_ENABLE_LOW_SIDE_DRIVE_BASE_IO_PIN+2, GPIO_OUT);

}