// QE Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL with QE
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz
// Stack:           4096 bytes

// Hardware configuration:
// Quadrature Encoder:
//   PhA0 (PD6) and PhB0 (PD7) are connected to 256 cpr optical encoder
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "wait.h"
#include "uart0.h"
#include "qei0.h"

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
    int32_t pos;
    char str[80];

    // Initialize hardware
    initHw();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    initQei0();

    while(true)
    {
        // Read sensor
        pos = getQei0Position();
        snprintf(str, sizeof(str), "Count: %"PRId32"\nAngle: %"PRId32"\n\n", pos, pos*360/512);
        putsUart0(str);

        waitMicrosecond(250000);
    }
}
