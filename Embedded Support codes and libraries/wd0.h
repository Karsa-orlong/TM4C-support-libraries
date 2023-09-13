// Watchdog 0 functions
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef WD0_H_
#define WD0_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initWatchdog0(uint32_t timeoutUs, uint32_t fcyc);
void resetWatchdog0();

#endif
