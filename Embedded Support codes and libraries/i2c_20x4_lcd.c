// I2C 20x4 LCD Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// HD44780-based 20x4 LCD display
// Display driven by PCF8574 I2C 8-bit I/O expander at address 0x27
// I2C devices on I2C bus 0 with 2kohm pullups on SDA and SCL
// Display RS, R/W, E, backlight enable, and D4-7 connected to PCF8574 P0-7

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "i2c0_lcd.h"
#include "wait.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Initialize hardware
    initHw();
    initLcd();

    while(true)
    {
        putsLcd(0, 0, "Line 1 | | | | | | |");
        putsLcd(1, 0, "Line 2 | | | | | | |");
        putsLcd(2, 0, "Line 3 | | | | | | |");
        putsLcd(3, 0, "Line 4 | | | | | | |");

        waitMicrosecond(2000000);

        putsLcd(0, 0, "abcdefghijklmnopqrst");
        putsLcd(1, 0, "uvwxyzABCDEFGHIJKLMN");
        putsLcd(2, 0, "OPQRSTUVWXYZ12345678");
        putsLcd(3, 0, "90!@#$%^&*()-_+={}[]");

        waitMicrosecond(2000000);
    }
}
