// Stop Go C Example (/dev/gpiomem)
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: RPi 3b+
// Target uC:       BCM2837
// System Clock:    -

// Hardware configuration:
// Red LED:
//   BCM27 drives the red LED through a 470 ohm current limiting resistor
// Green LED:
//   BCM22 drives the green LED through a 470 ohm current limiting resistor
// Pushbutton:
//   SW1 pulls BCM26 low, with external pull-up resistor

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdlib.h>          // EXIT_ codes
#include <stdio.h>           // printf
#include <stdint.h>          // C99 integer types -- uint32_t
#include <stdbool.h>         // bool
#include <fcntl.h>           // open
#include <sys/mman.h>        // mmap
#include <unistd.h>          // close

#define RED_LED 27
#define GREEN_LED 22
#define PUSH_BUTTON 26

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

volatile uint32_t *gpio;

bool gpioOpen()
{
    // Open /dev/gpiomem 
    // Use instead of /dev/mem since location does not change and no root access required
    int file = open("/dev/gpiomem", O_RDWR | O_SYNC);
    bool bOK = (file >= 0);
    if (bOK)
    {
        // Create map to any location in the local memory space
        // to the physical memory location of /dev/gpiomem with aperature size of B4h
        // memory should be r/w and shared
        gpio = mmap(NULL, 0xB4, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
        bOK = (gpio != MAP_FAILED);

        // Close /dev/gpiomem 
        close(file);
    }
    return bOK;
}
 
void gpioOutput(int pin)
{
    uint32_t regOfs = pin/10;
    uint32_t bitOfs = (pin%10) * 3;

    // Set mode to output
    (*(gpio+regOfs)) &= ~(7<<bitOfs);
    (*(gpio+regOfs)) |=  (1<<bitOfs);
}

void gpioInput(int pin)
{
    uint32_t regOfs = pin/10;
    uint32_t bitOfs = (pin%10) * 3;

    // Set mode to input
    (*(gpio+regOfs)) &= ~(7<<bitOfs);
}

void gpioWrite(int pin, int value)
{
    uint32_t regOfs = (pin >> 5) + 10 - value*3;
    uint32_t bitOfs = pin & 31;
   
    // Set or clear bit
    (*(gpio+regOfs)) = (1<<bitOfs);
}

int gpioRead(int pin)
{
    uint32_t regOfs = (pin >> 5) + 13;
    uint32_t bitOfs = pin & 31;
   
    // Get bit
    return ((*(gpio+regOfs)) >> bitOfs) & 1;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    if (!gpioOpen())
        return EXIT_FAILURE;
    gpioOutput(RED_LED);    
    while (true)
	gpioWrite(RED_LED, !gpioRead(RED_LED));

    return EXIT_SUCCESS;
}

