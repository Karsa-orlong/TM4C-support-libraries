// Motor Control Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// Servo motor drive:
//   PWM output on M1PWM6 (PF2) - blue on-board LED
//   DIR output on PF3 - green on-board LED

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "motor_control.h"
#include "gpio.h"

// Bitband aliases
#define DIRECTION    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))

// Pins
#define PWM PORTF,2
#define DIR PORTF,3

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize motor control
void initMotorControl()
{
    // Enable clocks
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R1;
    _delay_cycles(3);
    enablePort(PORTF);

    // Configure PWM and DIRECTION outputs
    selectPinPushPullOutput(DIR);
    selectPinPushPullOutput(PWM);
    setPinAuxFunction(PWM, GPIO_PCTL_PF2_M1PWM6);

    // Configure PWM module 1 to drive servo
    // PWM on M1PWM6 (PF2), M0PWM3a
    SYSCTL_SRPWM_R |= SYSCTL_SRPWM_R1;               // reset PWM1 module
    SYSCTL_SRPWM_R &= ~SYSCTL_SRPWM_R1;              // leave reset state
    _delay_cycles(3);                                // wait 3 clocks
    PWM1_3_CTL_R = 0;                                // turn-off PWM1 generator 3
    PWM1_3_GENA_R = PWM_1_GENA_ACTCMPAD_ONE | PWM_1_GENA_ACTLOAD_ZERO;
    PWM1_3_LOAD_R = 1024;                            // set period to 40 MHz sys clock / 2 / 1024 = 19.53125 kHz
    PWM1_3_CMPA_R = 0;                               // PWM off (0=always low, 1023=always high)
    PWM1_3_CTL_R = PWM_1_CTL_ENABLE;                 // turn-on PWM1 generator 3
    PWM1_ENABLE_R = PWM_ENABLE_PWM6EN;               // enable PWM output
}

void setMotorPwm(unsigned int dutyCycle)
{
    PWM1_3_CMPA_R = dutyCycle;
}

void setMotorDirection(bool dir)
{
    DIRECTION = dir;
}
