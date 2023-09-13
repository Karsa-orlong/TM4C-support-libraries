// Stop Go C Example (sysfs)
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
#include <stdbool.h>         // bool
#include <stdio.h>           // fopen, fclose, fscanf, printf

#define RED_LED 27
#define GREEN_LED 22
#define PUSH_BUTTON 26

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void gpioOutput(int pin)
{
    FILE* file;
    char str[35];
    file = fopen("/sys/class/gpio/export", "w");
    fprintf(file, "%d", pin);
    fclose(file);
    sprintf(str, "/sys/class/gpio/gpio%d/direction", pin);
    file = fopen(str, "w");
    fprintf(file, "out");
    fclose(file);
}

void gpioInput(int pin)
{
    FILE* file;
    char str[35];
    file = fopen("/sys/class/gpio/export", "w");
    fprintf(file, "%d", pin);
    fclose(file);
    sprintf(str, "/sys/class/gpio/gpio%d/direction", pin);
    file = fopen(str, "w");
    fprintf(file, "in");
    fclose(file);
}

void gpioWrite(int pin, int value)
{
    FILE* file;
    char str[35];
    sprintf(str, "/sys/class/gpio/gpio%d/value", pin);
    file = fopen(str, "w");
    fprintf(file, "%d", value);
    fclose(file);
}
  
int gpioRead(int pin)
{
    FILE* file;
    int result;
    char str[24];
    sprintf(str, "/sys/class/gpio/gpio%d/value", pin);
    file = fopen(str, "rw");
    fscanf(file, "%d", &result);
    fclose(file);
    return result;
}
 
//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    // Initialize GPIO pins
    gpioOutput(RED_LED);
    while (true)
	gpioWrite(RED_LED, !gpioRead(RED_LED));

    return EXIT_SUCCESS;
}

