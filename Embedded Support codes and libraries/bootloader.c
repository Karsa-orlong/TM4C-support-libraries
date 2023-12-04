// Bootloader Example
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Red LED:
//   PF1 drives an NPN transistor that powers the red LED
// Blue LED:
//   PF2 drives an NPN transistor that powers the blue LED
// Green LED:
//   PF3 drives an NPN transistor that powers the green LED
// Pushbutton:
//   SW1 pulls pin PF4 low (internal pull-up is used)
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1
//
// To invoke bootloader, power cycle the board with PB1 pressed
// and then execute the bootloader program

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tm4c123gh6pm.h"

// Bitband aliases
#define RED_LED      (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 1*4)))
#define BLUE_LED     (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 2*4)))
#define GREEN_LED    (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 3*4)))
#define PUSH_BUTTON  (*((volatile uint32_t *)(0x42000000 + (0x400253FC-0x40000000)*32 + 4*4)))

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

// PortF masks
#define GREEN_LED_MASK 8
#define BLUE_LED_MASK 4
#define RED_LED_MASK 2
#define PUSH_BUTTON_MASK 16

// Bootloader
#define FLASH_BASE_ADDRESS 0
#define FLASH_SIZE 262144
#define RAM_BASE_ADDRESS 0x20000000
#define RAM_SIZE 32768

#define PAGE_SIZE 1024
#define BLOCKS_PER_PAGE 8
#define WORDS_PER_BLOCK 32

#define RELOCATED_IVT_ADD 4096
#define SP_INIT_OFFSET 0
#define PC_INIT_OFFSET 4

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

extern void setSp(uint32_t sp);

typedef void (*_fn)();
uint32_t pageBuffer[256];
uint32_t sp, resetAdd;

// Blocking function that returns only when SW1 is pressed
bool isBootloadRequested()
{
	return (PUSH_BUTTON == 0);
}

// Initialize Hardware
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, sysdivider of 5, creating system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Enable clocks
    SYSCTL_RCGCUART_R = SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R = SYSCTL_RCGCGPIO_R0 | SYSCTL_RCGCGPIO_R5;
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    GPIO_PORTF_DIR_R = GREEN_LED_MASK | BLUE_LED_MASK | RED_LED_MASK;
                                                       // bits 1, 2, and 3 are outputs, other pins are inputs
    GPIO_PORTF_DEN_R = PUSH_BUTTON_MASK | BLUE_LED_MASK | GREEN_LED_MASK | RED_LED_MASK;
                                                       // enable LEDs and pushbuttons
    GPIO_PORTF_PUR_R = PUSH_BUTTON_MASK;               // enable internal pull-up for push button

    // Configure UART0 pins
    GPIO_PORTA_DEN_R = UART_TX_MASK | UART_RX_MASK;    // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R = UART_TX_MASK | UART_RX_MASK;  // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R = GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
                                                        // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R &= ~UART_CTL_UARTEN;                    // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                     // use system clock (40 MHz)
    UART0_IBRD_R = 21;                                  // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // enable TX, RX, and module

    // Turn-off status LEDs
    GREEN_LED = 0;
    RED_LED = 0;
    BLUE_LED = 0;
}

// Un-initialize most hardware changes when leaving bootloader
void unInitHw()
{
    SYSCTL_SRGPIO_R |= SYSCTL_SRGPIO_R0 | SYSCTL_SRGPIO_R5;
    SYSCTL_SRGPIO_R &= ~(SYSCTL_SRGPIO_R0 | SYSCTL_SRGPIO_R5);
    SYSCTL_SRUART_R |= SYSCTL_SRUART_R0;
    SYSCTL_SRUART_R &= ~SYSCTL_SRUART_R0;
}

void showBootloadRequested()
{
    BLUE_LED = 1;
    GREEN_LED = 0;
    RED_LED = 0;
}


void showConnection()
{
    BLUE_LED = 0;
    GREEN_LED = 1;
    RED_LED = 0;
}

void showError()
{
    BLUE_LED = 0;
    GREEN_LED = 0;
    RED_LED = 1;
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
	while (UART0_FR_R & UART_FR_TXFF);
	UART0_DR_R = c;
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
	while (UART0_FR_R & UART_FR_RXFE);
	return UART0_DR_R & 0xFF;
}

// Blocking function that writes a uint32_t when the UART buffer is not full
void putlUart0(uint32_t data)
{
	uint8_t i;
    for (i = 0; i < sizeof(uint32_t); i++)
    {
        while (UART0_FR_R & UART_FR_TXFF);
        UART0_DR_R = data & 0xFF;
        data >>= 8;
    }
}

// Blocking function that writes a uint32_t when the UART buffer is not full
uint32_t getlUart0()
{
	uint8_t i;
	uint32_t data8;
	uint32_t data = 0;
    for (i = 0; i < sizeof(uint32_t); i++)
    {
    	while (UART0_FR_R & UART_FR_RXFE);
    	data8 = UART0_DR_R & 0xFF;
    	data >>= 8;
    	data += data8 << 24;
    }
    return data;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
	// Initialize hardware
	initHw();

	// Check for bootload request
	if (isBootloadRequested())
    {
	    showBootloadRequested();

	    // Receive "M4F Unlock" code
        uint32_t phase = 0;
        char str[11] = "M4F_Unlock";
        char c;
        while (phase < 10)
        {
		  c = getcUart0();
		  if (c == str[phase])
			  phase++;
		  else
			  phase = 0;
        }

        // Send acknowledge, indicate in programming mode
        putcUart0('k');
        showConnection();

        // Receive number of blocks and handle checksum
        uint32_t nPages = getlUart0();
        putlUart0(~nPages);
        uint32_t nChecksum = getlUart0();

        // Handle error condition
        if (~nPages != nChecksum)
            showError();

        // Continue with all program blocks
        else
        {
        	uint32_t add;
        	uint32_t checksum;
        	uint16_t i, j, block;
        	uint16_t page = 0;
        	bool error = false;
        	while (!error && (page < nPages))
        	{
        		add = getlUart0();
        		checksum = add;
        		for (i = 0; i < PAGE_SIZE/sizeof(uint32_t); i++)
        		{
        			pageBuffer[i] = getlUart0();
        			checksum += pageBuffer[i];
        		}

    			putlUart0(~checksum);
    			error = (~checksum != getlUart0());
    			if (error)
    			    showError();
    			else
    			{
    				// Erase 1k page
    				FLASH_FMA_R = add;
    				FLASH_FMC_R = FLASH_FMC_WRKEY | FLASH_FMC_ERASE;
    				while (FLASH_FMC_R & FLASH_FMC_ERASE);

    				// Program 8 blocks of 32 words (128 bytes)
    				uint32_t* buffer = (uint32_t*)&FLASH_FWBN_R;
    				uint32_t k = 0;
    				for (block = 0; block < BLOCKS_PER_PAGE; block++)
    				{
    					FLASH_FMA_R = add;
    					for (j = 0; j < WORDS_PER_BLOCK; j++)
    					{
    						buffer[j] = pageBuffer[k++];
    					}
        				FLASH_FMC2_R = FLASH_FMC_WRKEY | FLASH_FMC2_WRBUF;
        				while (FLASH_FMC2_R & FLASH_FMC2_WRBUF);
        				add += WORDS_PER_BLOCK*sizeof(uint32_t);
    				}

                    // Send write done flag
                    putcUart0('d');
          		    page++;
    			}
        	}
        }

        // Ensure last done byte transmits
        while (UART0_FR_R & UART_FR_BUSY);
    }

    // Start normal program if relocated IVT is valid
	// note: sp and resetAdd are global since stack is changing
    sp = *(uint32_t*)(RELOCATED_IVT_ADD + SP_INIT_OFFSET);
    resetAdd = *(uint32_t*)(RELOCATED_IVT_ADD + PC_INIT_OFFSET);
	if (resetAdd < (FLASH_BASE_ADDRESS + FLASH_SIZE)
	    && sp >= RAM_BASE_ADDRESS && sp < (RAM_BASE_ADDRESS + RAM_SIZE))
	{
	    // Back out changes to HW
	    unInitHw();

	    // setup VTABLE
	    NVIC_VTABLE_R = RELOCATED_IVT_ADD;

	    // setup stack
	    setSp(sp);

	    // start program
	    _fn fn = (_fn)resetAdd;
	    fn();
	}
	else
	{
	    showError();
	    while(true);
	}
}

