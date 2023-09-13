// GPIO Interrupt Handling Example
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

// Before make:
// sudo apt update
// sudo apt upgrade
// sudo apt install raspberrypi-kernel-headers

//-----------------------------------------------------------------------------

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/io.h>           // iowrite, ioread (platform specific)
#include <linux/gpio.h>       // gpio_to_isr

#define RED_LED 27
#define GREEN_LED 22
#define PUSH_BUTTON 26

#define GPIO_OFFSET 0x3F200000
#define GPIO_SIZE 0xB4

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

uint32_t *gpio = NULL;
int irq;

//-----------------------------------------------------------------------------
// Kernel module information
//-----------------------------------------------------------------------------

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jason Losh");
MODULE_DESCRIPTION("GPIO Interrupt Handler");

//-----------------------------------------------------------------------------
// GPIO functions0
//-----------------------------------------------------------------------------

void gpioOutput(int pin)
{
    uint32_t regOfs = pin/10;
    uint32_t bitOfs = (pin%10) * 3;
    uint32_t value;

    value = ioread32(gpio+regOfs);
    value &= ~(7<<bitOfs);
    value |= (1<<bitOfs);
    iowrite32(value, gpio+regOfs);
}

void gpioInput(int pin)
{
    uint32_t regOfs = pin/10;
    uint32_t bitOfs = (pin%10) * 3;
    uint32_t value;

    value = ioread32(gpio+regOfs);
    value &= ~(7<<bitOfs);
    iowrite32(value, gpio+regOfs);
}

void gpioWrite(int pin, int value)
{
    uint32_t regOfs = (pin >> 5) + 10 - value*3;
    uint32_t mask = 1 << (pin & 31);
    iowrite32(mask, gpio+regOfs);
}

int gpioRead(int pin)
{
    uint32_t regOfs = (pin >> 5) + 13;
    uint32_t bitOfs = pin & 31;

    return (ioread32(gpio+regOfs) >> bitOfs) & 1;
}

void gpioClearInterrupt(int pin)
{
    uint32_t regOfs = (pin >> 5) + 16;
    uint32_t mask = 1 << (pin & 31);

    iowrite32(mask, gpio+regOfs);
}

void gpioDisableInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value &= ~mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value &= ~mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value &= ~mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value &= ~mask;
    iowrite32(value, gpio+regOfsLow);
}

void gpioSelectRisingEdgeInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value |= mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value &= ~mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value &= ~mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value &= ~mask;
    iowrite32(value, gpio+regOfsLow);
}

void gpioSelectFallingEdgeInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value &= ~mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value |= mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value &= ~mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value &= ~mask;
    iowrite32(value, gpio+regOfsLow);
}

void gpioSelectBothEdgesInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value |= mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value |= mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value &= ~mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value &= ~mask;
    iowrite32(value, gpio+regOfsLow);
}

void gpioSelectHighLevelInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value &= ~mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value &= ~mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value |= mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value &= ~mask;
    iowrite32(value, gpio+regOfsLow);
}

void gpioSelectLowLevelInterrupt(int pin)
{
    uint32_t regOfsRising = (pin >> 5) + 19;
    uint32_t regOfsFalling = (pin >> 5) + 22;
    uint32_t regOfsHigh = (pin >> 5) + 25;
    uint32_t regOfsLow = (pin >> 5) + 28;
    uint32_t mask = 1 << (pin & 31);
    uint32_t value;

    value = ioread32(gpio+regOfsRising);
    value &= ~mask;
    iowrite32(value, gpio+regOfsRising);
    value = ioread32(gpio+regOfsFalling);
    value &= ~mask;
    iowrite32(value, gpio+regOfsFalling);
    value = ioread32(gpio+regOfsHigh);
    value &= ~mask;
    iowrite32(value, gpio+regOfsHigh);
    value = ioread32(gpio+regOfsLow);
    value |= mask;
    iowrite32(value, gpio+regOfsLow);
}

//-----------------------------------------------------------------------------
// ISR
//-----------------------------------------------------------------------------

irq_handler_t isr(int irq, void *dev_id, struct pt_regs *regs)
{
    // Turn on GREEN LED
    gpioWrite(GREEN_LED, 1);

    printk(KERN_INFO "GPIO 22 Falling Edge occured\n");

    return (irq_handler_t)IRQ_HANDLED;
}

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------

static int __init initialize_module(void)
{
    int result;

    // Physical to virtual memory map to access gpio registers
    gpio = (uint32_t*)ioremap_nocache(GPIO_OFFSET, GPIO_SIZE);
    printk(KERN_INFO "GPIO ISR gpio@%p\n", gpio);

    // Setup pins
    gpioOutput(RED_LED);
    gpioOutput(GREEN_LED);
    gpioInput(PUSH_BUTTON);

    gpioWrite(RED_LED, 1);
    gpioWrite(GREEN_LED, 0);
    gpioSelectFallingEdgeInterrupt(PUSH_BUTTON);
    gpioClearInterrupt(PUSH_BUTTON);

    // Request GPIO
    irq = gpio_to_irq(PUSH_BUTTON);

    // Register ISR
    result = request_irq(irq, (irq_handler_t)isr, IRQF_SHARED,
                         "GPIO ISR", (irq_handler_t)isr);

    if (result >= 0)
        printk(KERN_INFO "GPIO ISR initialization complete\n");

    return result;
}

static void __exit exit_module(void)
{
    gpioWrite(RED_LED, 0);
    gpioWrite(GREEN_LED, 0);
    free_irq(irq, (irq_handler_t)isr);
    printk(KERN_INFO "GPIO ISR exiting\n");
}

module_init(initialize_module);
module_exit(exit_module);
