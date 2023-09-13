/*
 * Lab3.c
 *
 *  Created on: Feb 22, 2023
 *      Author: kalki
 */


#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "wait.h"
#include "gpio.h"

#define BLUE_LED_MASK 4
//#define BLUE_LED (*((volatile uint32_t*)(0X42000000 + (0X400253FC - 0X40000000) + 2*4)))



void initHw(void){
  initSystemClockTo40Mhz();
 SYSCTL_RCGCGPIO_R |=   SYSCTL_RCGCGPIO_R5;
 _delay_cycles(3);

 GPIO_PORTF_DIR_R |= BLUE_LED_MASK;
 GPIO_PORTF_DEN_R |= BLUE_LED_MASK;
}
//ASSEMBLY
void toggleBit(){

    __asm("GPIO_BLUE_LED: .field 0X400253FC");
    __asm("toggle: LDR R1, GPIO_BLUE_LED");
    __asm(" LDR R0, [R1]");
    __asm(" EOR R0, R0, #4");
    __asm(" STR R0, [R1]");
    __asm(" B toggle");


}
void gpioIsr(){}

int main(){

    initHw();
    toggleBit();

}

