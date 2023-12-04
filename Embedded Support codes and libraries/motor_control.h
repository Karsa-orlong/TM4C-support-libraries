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

#ifndef MOTOR_CONTROL_H_
#define MOTOR_CONTROL_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initMotorControl();
void setMotorPwm(unsigned int dutyCycle);
void setMotorDirection(bool dir);

#endif
