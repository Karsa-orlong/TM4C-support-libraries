// PID Controller Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// Red Backlight LED:
//   M0PWM3 (PB5) drives an NPN transistor that powers the red LED
// Green Backlight LED:
//   M0PWM5 (PE5) drives an NPN transistor that powers the green LED
// Blue Backlight LED:
//   M0PWM4 (PE4) drives an NPN transistor that powers the blue LED
// ST7565R Graphics LCD Display Interface:
//   MOSI on PD3 (SSI1Tx)
//   SCLK on PD0 (SSI1Clk)
//   ~CS on PD1 (SSI1Fss)
//   A0 connected to PD2
// Servo motor drive:
//   PWM output on M1PWM6 (PF2) - blue on-board LED
//   DIR output on PF3 - green on-board LED
// Analog feedback:
//   Analog inputs on AIN3 (PE0)
// Digital feedback:
//   Quadrature encoder inputs on PhA0 (PD6) and PhB0 (PD7)
// 4x4 Keyboard
//   Column 0-3 open drain outputs on PB0, PB1, PB4, PA6
//   Rows 0-3 inputs with pull-ups on PE1, PE2, PE3, PA7
//   To locate a key (r, c), the column c is driven low so the row r reads low
//   4 Menu Pages:
//                                   Pg1    Pg 2   Pg3    Pg4    Pg5
//   1=show Y   2=show U   3=show S  A=Kp   K      Yset   FB     Tcap
//   4=step     5          6         B=Ki   Imax   Yst1   Ydiv   Ymax
//   7=man_ccw  8=man_stop 9=man_cw  C=Kd   Dead   Yst2          Umax
//   *=run/stop 0=zero_qe  #=ent/pg  D=Ko

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "clock.h"
#include "wait.h"
#include "graphics_lcd.h"
#include "backlight.h"
#include "kb.h"
#include "motor_control.h"
#include "adc0.h"
#include "qei0.h"
#include "tm4c123gh6pm.h"

// Feedback mode
#define FB_ANALOG 0
#define FB_QE 1

// Capture count
#define CAPTURE_SIZE 100

// Menu
#define MENU_COUNT 5
#define MENU_ITEMS 4

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

    // plant output (y) varies from 0 to 1023 (10 bit range), so place in center
    int32_t ySetPoint = 512;

    // set step response positions at 1/4 and 3/4 of 10 bit full scale range
    int32_t yStep1 = 1024;
    int32_t yStep2 = 3072;

    // pid calculation of u
    int32_t coeffKp = 100;
    int32_t coeffKi = 0;
    int32_t coeffKd = 0;
    int32_t coeffKo = 0;
    int32_t coeffK = 100; // denominator used to scale Kp, Ki, and Kd
    int32_t integral = 0;
    int32_t iMax = 100;
    int32_t diff;
    int32_t error;
    int32_t u = 0;
    int32_t deadBand = 0;

    // Feedback variables
    int8_t fbMode = FB_ANALOG;
    int32_t y = 0;
    uint32_t yDiv = 1;

    // Mode variables
    bool runMode = false;
    bool stepMode = false;
    uint16_t stepTime = 2000;
    bool manualMode = false;
    uint16_t manualPwm = 0;

    // Capture variables
    // disable data capture request and done flags
    bool captureRequest = false;
    bool captureDone = false;
    // default capture at 1ms rate
    int16_t tCap = 1;
    int8_t capturePhase = 0;
    // size buffers for plant input and output capture
    int8_t captureBufferIndex = 0;
    int16_t captureBufferU[CAPTURE_SIZE] = {0};
    int16_t captureBufferY[CAPTURE_SIZE] = {0};

    // Display variables
    uint8_t displayPage = 0;
    bool displayY = true;
    bool displayU = false;
    bool displaySensors = false;
    int32_t displayUMax = 4095;
    int32_t displayYMax = 4095;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initPidController()
{
    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2;
    _delay_cycles(3);

    // Configure Timer 2 for PID controller
    TIMER2_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER2_TAILR_R = 40000;                          // set load value to 40000 for 1000 Hz interrupt rate
    TIMER2_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    NVIC_EN0_R = 1 << (INT_TIMER2A-16);              // turn-on interrupt 37 (TIMER1A)
    TIMER2_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
}

void drawMenu()
{
    const char menu[MENU_ITEMS][MENU_COUNT][5] =
    {
        {"Kp  ", "K   ", "Yset", "FB  ", "Tcap"},
        {"Ki  ", "Imax", "Yst1", "Ydiv", "Ymax"},
        {"Kd  ", "Dead", "Yst2", "    ", "Umax"},
        {"Ko  ", "    ", "    ", "    ", "    "}
    };
    uint8_t i;
    for (i = 0; i < 4; i++)
    {
        setGraphicsLcdTextPosition(104,2*i);
        putsGraphicsLcd((char*)menu[i][displayPage]);
    }
}

void drawVariables()
{
    char str[5];
    switch(displayPage)
    {
        case 0:
            setGraphicsLcdTextPosition(104,1);
            snprintf(str, sizeof(str), "%04"PRId32, coeffKp);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,3);
            snprintf(str, sizeof(str), "%04"PRId32, coeffKi);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,5);
            snprintf(str, sizeof(str), "%04"PRId32, coeffKd);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,7);
            snprintf(str, sizeof(str), "%04"PRId32, coeffKo);
            putsGraphicsLcd(str);
            break;
        case 1:
            setGraphicsLcdTextPosition(104,1);
            snprintf(str, sizeof(str), "%04"PRId32, coeffK);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,3);
            snprintf(str, sizeof(str), "%04"PRId32, iMax);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,5);
            snprintf(str, sizeof(str), "%04"PRId32, deadBand);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,7);
            putsGraphicsLcd("    ");
            break;
        case 2:
            setGraphicsLcdTextPosition(104,1);
            snprintf(str, sizeof(str), "%04"PRId32, ySetPoint);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,3);
            snprintf(str, sizeof(str), "%04"PRId32, yStep1);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,5);
            snprintf(str, sizeof(str), "%04"PRId32, yStep2);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,7);
            putsGraphicsLcd("    ");
            break;
        case 3:
            setGraphicsLcdTextPosition(104,1);
            if (fbMode == FB_ANALOG)
                putsGraphicsLcd("AIN ");
            if (fbMode == FB_QE)
                putsGraphicsLcd("QE  ");
            setGraphicsLcdTextPosition(104,3);
            snprintf(str, sizeof(str), "%04"PRId32, yDiv);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,5);
            putsGraphicsLcd("    ");
            setGraphicsLcdTextPosition(104,7);
            putsGraphicsLcd("    ");
            break;
        case 4:
            setGraphicsLcdTextPosition(104,1);
            snprintf(str, sizeof(str), "%04"PRId32, tCap);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,3);
            snprintf(str, sizeof(str), "%04"PRId32, displayYMax);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,5);
            snprintf(str, sizeof(str), "%04"PRId32, displayUMax);
            putsGraphicsLcd(str);
            setGraphicsLcdTextPosition(104,7);
            putsGraphicsLcd("    ");
            break;
    }
}

void drawPlot()
{
    uint8_t x;
    uint32_t y, yScaled;
    int32_t uScaled;
    // clear graphics area
    drawGraphicsLcdRectangle(0, 0, 104, 64, CLEAR);

    // draw plant output (y)
    if (displayY)
    {
        // dashed line if in step mode
        if (stepMode)
        {
            y = ((uint32_t)yStep1 * 64) / displayYMax;
            for (x = 0; x < CAPTURE_SIZE; x++)
            if (x & 4)
                drawGraphicsLcdPixel(x, y, SET);
            y = ((long)yStep2 * 64) / displayYMax;
            for (x = 0; x < CAPTURE_SIZE; x++)
            if (x & 4)
                drawGraphicsLcdPixel(x, y, SET);
        }
        setGraphicsLcdTextPosition(90, 7);
        putcGraphicsLcd('y');
        for (x = 0; x < CAPTURE_SIZE; x++)
        {
            yScaled = captureBufferY[x];
            yScaled = (yScaled * 64) / displayYMax;
            if (yScaled > 63) yScaled = 63;
                drawGraphicsLcdPixel(x, 63 - yScaled, SET);
        }
    }

    // draw plant input (u)
    if (displayU)
    {
        setGraphicsLcdTextPosition(84, 7);
        putcGraphicsLcd('u');
        for (x = 0; x < CAPTURE_SIZE; x++)
        {
            uScaled = captureBufferU[x];
            uScaled = (uScaled * 32) / displayUMax;
            if (uScaled > 32)
                uScaled = 32;
            if (uScaled < -31)
                uScaled = -31;
            drawGraphicsLcdPixel(x, 32 - uScaled, SET);
        }
    }
}

void getsKb(char str[], int maxSize, char enterCharacter)
{
    int count = 0;
    char c = 0;

    // fill string with characters up to max length or enter pressed
    while ((count < maxSize) && (c != enterCharacter))
    {
        c = getKey();
        if (c != enterCharacter) str[count++] = c;
    }

    // add null
    str[count] = 0;
}

void processKeyboardData()
{
    char c;
    char str[5];
    if (kbhit())
    {
        c = getKey();
        switch(c)
        {
            case 'A':
                switch(displayPage)
                {
                    case 0:
                        drawGraphicsLcdRectangle(104, 8, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &coeffKp);
                        break;
                    case 1:
                        drawGraphicsLcdRectangle(104, 8, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &coeffK);
                        break;
                    case 2:
                        drawGraphicsLcdRectangle(104, 8, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &ySetPoint);
                        captureRequest = true;
                        stepMode = false;
                        break;
                    case 3:
                        fbMode = !fbMode;
                        break;
                    case 4:
                        drawGraphicsLcdRectangle(104, 8, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd16, &tCap);
                }
                drawVariables();
                break;
            case 'B':
                drawGraphicsLcdRectangle(104, 24, 24, 8, INVERT);
                getsKb(str, 4, '#');
                switch(displayPage)
                {
                    case 0:
                        sscanf(str, "%"SCNd32, &coeffKi);
                        break;
                    case 1:
                        sscanf(str, "%"SCNd32, &iMax);
                        break;
                    case 2:
                        sscanf(str, "%"SCNd32, &yStep1);
                        break;
                    case 3:
                        sscanf(str, "%"SCNd32, &yDiv);
                        break;
                    case 4:
                        sscanf(str, "%"SCNd32, &displayYMax);
                        if (displayYMax < 64) displayYMax = 64;
                        drawPlot();
                }
                drawVariables();
                break;
            case 'C':
                switch(displayPage)
                {
                    case 0:
                        drawGraphicsLcdRectangle(104, 40, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &coeffKd);
                        break;
                    case 1:
                        drawGraphicsLcdRectangle(104, 40, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &deadBand);
                        break;
                    case 2:
                        break;
                    case 3:
                        break;
                    case 4:
                        drawGraphicsLcdRectangle(104, 40, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &displayYMax);
                        if (displayYMax < 64) displayYMax = 64;
                        drawPlot();
                }
                drawVariables();
                break;
            case 'D':
                switch(displayPage)
                {
                    case 0:
                        drawGraphicsLcdRectangle(104, 56, 24, 8, INVERT);
                        getsKb(str, 4, '#');
                        sscanf(str, "%"SCNd32, &coeffKo);
                        break;
                    case 1:
                        break;
                    case 2:
                        break;
                    case 3:
                        break;
                    case 4:
                        break;
                }
                drawVariables();
                break;
            case '*':
                runMode = !runMode;
                if (runMode)
                    setBacklightRgbColor(64, 1023, 64);
                else
                    setBacklightRgbColor(1023, 64, 64);
                break;
            case '#':
                displayPage++;
                if (displayPage == MENU_COUNT) displayPage = 0;
                drawMenu();
                drawVariables();
                break;
            case '1':
                displayY = !displayY;
                displaySensors = false;
                drawPlot();
                break;
            case '2':
                displayU = !displayU;
                displaySensors = false;
                drawPlot();
                break;
            case '3':
                displaySensors = !displaySensors;
                displayY = false;
                displayU = false;
                drawPlot();
                break;
            case '4':
                ySetPoint = yStep1;
                stepMode = true;
                drawVariables();
                waitMicrosecond(stepTime*1000);
                captureRequest = true;
                ySetPoint = yStep2;
                drawVariables();
                break;
            case '7':
                setBacklightRgbColor(1023, 1023, 64);
                runMode = false;
                manualMode = true;
                u -= 10;
                break;
            case '8':
                u = 0;
                manualMode = false;
                break;
            case '9':
                setBacklightRgbColor(1023, 1023, 8);
                runMode = false;
                manualMode = true;
                u += 10;
                break;
            case '0':
                setQei0Position(0);
        }
    }
}

void processControlData()
{
    char str[20];
    if (captureDone)
    {
        captureDone = false;
        drawPlot();
    }
    if (displaySensors)
    {
        setGraphicsLcdTextPosition(0, 0);
        snprintf(str, sizeof(str), "Y = %8"PRId32, y);
        putsGraphicsLcd(str);
        setGraphicsLcdTextPosition(0, 1);
        snprintf(str, sizeof(str), "E = %8"PRId32, error);
        putsGraphicsLcd(str);
        setGraphicsLcdTextPosition(0, 2);
        snprintf(str, sizeof(str), "I = %8"PRId32, integral);
        putsGraphicsLcd(str);
        setGraphicsLcdTextPosition(0, 3);
        snprintf(str, sizeof(str), "U = %8"PRId32, u);
        putsGraphicsLcd(str);
        waitMicrosecond(250000);
    }
}

void pidIsr()
{
    static int32_t errorLast = 0;

    // get feedback (process value, PV)
    switch (fbMode)
    {
        case FB_ANALOG:
            y = readAdc0Ss3() / yDiv;
            break;
        case FB_QE:
            y = getQei0Position() / yDiv;
    }

    if (runMode)
    {
        // determine error (set point - process value, SP-PV)
        error = ySetPoint - y;

        // calculate integral and prevent windup
        integral += error;

        int32_t iLimit = iMax * coeffKi;
        if (integral > iLimit)
            integral = iLimit;
        if (integral < -iLimit)
            integral = -iLimit;

        // calculate differential
        diff = error - errorLast;
        errorLast = error;

        // calculate plant input, saturating 10 bit output if needed
        u = (coeffKp * error) + (coeffKi * integral) + (coeffKd * diff);
        u /= coeffK;
        if (u > 1023) u = 1023;
        if (u < -1023) u = -1023;

        // set direction and speed based on mode
        setMotorDirection(u >= 0);
        if (abs(error) > deadBand)
            setMotorPwm(abs(u) + coeffKo);
        else
            setMotorPwm(0);
    }
    else
    {
        if (manualMode)
        {
            setMotorDirection(u > 0);
            setMotorPwm(abs(u));
        }
        else
            setMotorPwm(0);
    }

    // capture outputs
    if (captureRequest)
    {
        capturePhase++;
        if (capturePhase == tCap)
        {
            capturePhase = 0;
            captureBufferY[captureBufferIndex] = y;
            captureBufferU[captureBufferIndex++] = u;
            if (captureBufferIndex == CAPTURE_SIZE)
            {
                captureBufferIndex = 0;
                captureRequest = false;
                captureDone = true;
            }
        }
    }
    TIMER2_ICR_R = TIMER_ICR_TATOCINT;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
	// Initialize hardware
    initSystemClockTo40Mhz();
    initGraphicsLcd();
    initBacklight();
    initKb();
    initMotorControl();
    initAdc0Ss3();
    setAdc0Ss3Mux(3);
    setAdc0Ss3Log2AverageCount(6);
    initQei0();
    setQei0Position(0);
    initPidController();

    // Two background tasks are now running:
    // KB is handling user input
    // PID controller is controlling the plant

    // Draw initial screen
    setBacklightRgbColor(1023, 1023, 1023);
    drawMenu();
    drawVariables();
    drawPlot();

    // User interface endless loop
    while (true)
    {
        processKeyboardData();
        processControlData();
	}
}
