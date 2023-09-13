// Watchdog 0 functions
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "nvic.h"
#include "wd0.h"

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initWatchdog0(uint32_t timeoutUs, uint32_t fcyc)
{
    // Enable clock
    SYSCTL_RCGCWD_R |= SYSCTL_RCGCWD_R0;
    _delay_cycles(10);

    // Configure WDT0 which is driven by the system clock
    WATCHDOG0_LOCK_R = 0x1ACCE551;                       // unlock
    WATCHDOG0_LOAD_R = timeoutUs * (fcyc / 1e6);         // convert into fcyc units
    WATCHDOG0_CTL_R |= WDT_CTL_RESEN;                    // enable reset if timeout
    WATCHDOG0_CTL_R |= WDT_CTL_INTEN;                    // enable interrupts
    WATCHDOG0_LOCK_R = 0;                                // lock-out further changes
    WATCHDOG0_ICR_R = 0;                                 // clear any pending interrupt
    enableNvicInterrupt(INT_WATCHDOG);                   // turn-on interrupt 34 (WATCHDOG)
}

void resetWatchdog0()
{
    WATCHDOG0_ICR_R = 0;                                 // clear any pending interrupt
}

