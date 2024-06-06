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