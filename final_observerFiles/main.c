//Ernest Wang(919883842)
//Wahad Latif(918459497)


//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution. 
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
//
//*****************************************************************************


//*****************************************************************************
//
// Application Name     -   SSL Demo
// Application Overview -   This is a sample application demonstrating the
//                          use of secure sockets on a CC3200 device.The
//                          application connects to an AP and
//                          tries to establish a secure connection to the
//                          Google server.
// Application Details  -
// docs\examples\CC32xx_SSL_Demo_Application.pdf
// or
// http://processors.wiki.ti.com/index.php/CC32xx_SSL_Demo_Application
//
//*****************************************************************************


//*****************************************************************************
//
//! \addtogroup ssl
//! @{
//
//*****************************************************************************

#include <stdio.h>

// Simplelink includes
#include "simplelink.h"

//Driverlib includes
#include "hw_types.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "prcm.h"
#include "utils.h"
#include "uart.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"

//Common interface includes
#include "pin_mux_config.h"
#include "gpio_if.h"
#include "common.h"
#include "uart_if.h"

// Custom includes
#include "utils/network_utils.h"

// Other includes
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "gpio.h"
#include "systick.h"
#include "hw_nvic.h"

#include "Adafruit_GFX.h"
#include "oled_test.h"
#include "Adafruit_SSD1351.h"
#include "glcdfont.h"
#include "spi.h"


//*****************************************************************************
//                          MACROS
//*****************************************************************************
#define APPLICATION_VERSION  "1.4.0"
#define APP_NAME             "UART Echo"
#define CONSOLE              UARTA0_BASE
#define CONSOLE1             UARTA1_BASE
#define UartGetChar()        MAP_UARTCharGet(CONSOLE1)
#define UartPutChar(c)       MAP_UARTCharPut(CONSOLE1,c)
#define MAX_STRING_LENGTH    80


// some helpful macros for systick
#define SYSCLKFREQ 80000000ULL
#define TICKS_TO_US(ticks) ((((ticks) / SYSCLKFREQ) * 1000000ULL) + ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))
#define US_TO_TICKS(us) ((SYSCLKFREQ / 1000000ULL) * (us))
// systick reload value set to 40ms period
// (PERIOD_SEC) * (SYSCLKFREQ) = PERIOD_TICKS
//#define SYSTICK_RELOAD_VAL 3200000UL
#define SYSTICK_RELOAD_VAL 16000000ULL
// pin 50 info
#define IR_GPIO_PORT GPIOA0_BASE
#define IR_GPIO_PIN 0x1
//OLED STUFF
#define SPI_IF_BIT_RATE  100000



//NEED TO UPDATE THIS FOR IT TO WORK!
#define DATE                3    /* Current Date */
#define MONTH               6     /* Month 1-12 */
#define YEAR                2024  /* Current year */
#define HOUR                10    /* Time - hours */
#define MINUTE              10    /* Time - minutes */
#define SECOND              0     /* Time - seconds */


#define APPLICATION_NAME      "SSL"
#define APPLICATION_VERSION   "SQ24"
#define SERVER_NAME           "a2mlniuqin2pa7-ats.iot.us-east-1.amazonaws.com" // CHANGE MEX
#define GOOGLE_DST_PORT       8443


#define POSTHEADER "POST /things/Wahad_CC3200_Board/shadow HTTP/1.1\r\n"
#define GETHEADER "GET /things/Wahad_CC3200_Board/shadow HTTP/1.1\r\n"// CHANGE MEX
#define HOSTHEADER "Host: a2mlniuqin2pa7-ats.iot.us-east-1.amazonaws.com\r\n"  // CHANGE MEX
#define CHEADER "Connection: Keep-Alive\r\n"
#define CTHEADER "Content-Type: application/json; charset=utf-8\r\n"
#define CLHEADER1 "Content-Length: "
#define CLHEADER2 "\r\n\r\n"

//cut Data1 for concatnation of new messages
char DATA1BEFORE[] =  "{" \
            "\"state\": {\r\n"                                              \
                "\"desired\" : {\r\n"                                       \
                    "\"var\" :\"";

char DATA1AFTER[] ="\"\r\n"                                            \
                "}"                                                         \
            "}"                                                             \
        "}\r\n\r\n";

char DATA1[256];



//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
volatile uint8_t counter = 0;
//MESSAGE FROM GET REQUESt
volatile char getreqmsg[1460];
volatile int gotmsg = 0;


// track systick counter periods elapsed
// if it is not 0, we know the transmission ended
volatile int systick_expired = 0;
//pulse width in microseconds
volatile uint64_t ulsystick_delta_us = 0;
//PREVIOUS STATE
//volatile bool prev_state = 1;
//BUFFER
volatile uint64_t buffer = 0;
//BUTTON PRESSING DELAY in microseconds
volatile uint64_t button_delay_us = 0;
//RECEIVED MESSAGE
volatile char get_message[MAX_STRING_LENGTH];
volatile int received = 0;
//BUTTON CHARS
int button;
int last4Buttons[4];
volatile int nextChar;


long lRetVal = -1;

#if defined(ccs) || defined(gcc)
extern void (* const g_pfnVectors[])(void);
#endif
#if defined(ewarm)
extern uVectorEntry __vector_table;
#endif

//*****************************************************************************
//                 GLOBAL VARIABLES -- End: df
//*****************************************************************************


//****************************************************************************
//                      LOCAL FUNCTION PROTOTYPES
//****************************************************************************
static int set_time();
static void BoardInit(void);
static int http_post(int);

//*****************************************************************************
//
//! Board Initialization & Configuration
//!
//! \param  None
//!
//! \return None
//
//*****************************************************************************
static void BoardInit(void) {
/* In case of TI-RTOS vector table is initialize by OS itself */
#ifndef USE_TIRTOS
  //
  // Set vector table base
  //
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#if defined(ewarm)
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
#endif
#endif
    //
    // Enable Processor
    //
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);

    PRCMCC3200MCUInit();
}




//*****************************************************************************
//
//! This function updates the date and time of CC3200.
//!
//! \param None
//!
//! \return
//!     0 for success, negative otherwise
//!
//*****************************************************************************

static int set_time() {
    long retVal;

    g_time.tm_day = DATE;
    g_time.tm_mon = MONTH;
    g_time.tm_year = YEAR;
    g_time.tm_sec = HOUR;
    g_time.tm_hour = MINUTE;
    g_time.tm_min = SECOND;

    retVal = sl_DevSet(SL_DEVICE_GENERAL_CONFIGURATION,
                          SL_DEVICE_GENERAL_CONFIGURATION_DATE_TIME,
                          sizeof(SlDateTime),(unsigned char *)(&g_time));

    ASSERT_ON_ERROR(retVal);
    return SUCCESS;
}




//*****************************************************************************
//
//! Application startup display on UART
//!
//! \param  none
//!
//! \return none
//!
//*****************************************************************************
static void
DisplayBanner(char * AppName)
{

    Report("\n\n\n\r");
    Report("\t\t *************************************************\n\r");
    Report("\t\t        CC3200 %s Application       \n\r", AppName);
    Report("\t\t *************************************************\n\r");
    Report("\n\n\n\r");
}


/**
 * Function to reset SysTick Counter
 */
static inline void SysTickReset(void) {
    HWREG(NVIC_ST_CURRENT) = 1;
    systick_expired = 0;
}

/**
 * SysTick Interrupt Handler
 *
 * Keep track of whether the systick counter expired
 */
static void SysTickHandler(void) {
    systick_expired = 1;
}

/**
 * Interrupt handler for GPIOA2 port
 *
 * Only used here for decoding IR transmissions
 *
 *
 * Fill buffer with bits, once its filled copy it into another buffer/variable
 */
static void GPIOA0IntHandler(void) {
    //static bool prev_state = 1;

    // get and clear status
    unsigned long ulStatus;
    ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, true);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus);

    if ((ulStatus & IR_GPIO_PIN) && !systick_expired) {
        // if systick expired, the pulse was longer than 40ms
        // don't measure it in that case

        // calculate the pulse width
        ulsystick_delta_us = TICKS_TO_US(SYSTICK_RELOAD_VAL - MAP_SysTickValueGet());

        // determine whether 1 or 0
        if(ulsystick_delta_us < 1000){
            buffer = buffer << 1;
            buffer =  buffer | 0;
        }else if(ulsystick_delta_us > 1000){
            buffer = buffer << 1;
            buffer = buffer | 1;
        }

        counter++;
    }

    //expiration logic
    if(systick_expired){
        ulsystick_delta_us = 0;
        counter = 0;
        buffer = 0;
    }

    //default behavior
    SysTickReset();

    return;
}

//function that takes the long number and converts it to a button
//given the identiy of the binary sequence casted to an unsigned long long
int buttonDecode(unsigned long long* temp_buffer){
    int toReturn;
    switch (*temp_buffer){
        case 351860917536777:
            toReturn = 1;
            break;
        case 351860917569673:
            toReturn = 2;
            break;
        case 351860917553225:
            toReturn = 3;
            break;
        case 351860917586121:
            toReturn = 4;
            break;
        case 351860917545001:
            toReturn = 5;
            break;
        case 351860917577897:
            toReturn = 6;
            break;
        case 351860917561449:
            toReturn = 7;
            break;
        case 351860917594345:
            toReturn = 8;
            break;
        case 351860917540889:
            toReturn = 9;
            break;
        case 351860917573785:
            toReturn = 0;
            break;
        case 351860917554253:
            toReturn = 10;
            break;
        case 351860917595373:
            toReturn = 11;
            break;
        default:
            toReturn = -1;
            break;
    }
    *temp_buffer = 0;
    return toReturn;
}

//Uart handler to GET CHARACTERS
static void UARTIntHandler(){
    // get and clear status
    unsigned long ulStatus2;
    ulStatus2 = MAP_UARTIntStatus(CONSOLE1, true);
    MAP_UARTIntClear(CONSOLE1, ulStatus2);

    int i = 0;
    while(MAP_UARTCharsAvail(CONSOLE1)){
        get_message[i] = UartGetChar();
        i++;
    }
    received = 1;
}

//wrapper function to cycle through the different characters using the button1 as a select
char selectChar(int button, int prevButton, int* numPressed1, char* send_message, char prev)
{
    char toReturn;
    if(prevButton == button)
    {
        *numPressed1++;
    }
    int numPressed = *numPressed1 -1;
    if(numPressed<0)
    {
        numPressed = 0;
    }
    int i;
    switch (button){
        case 2:
            if(numPressed == 0)
            {
                toReturn = 'a';
            }
            else if(numPressed == 1)
            {
                toReturn = 'b';
            }
            else
            {
                toReturn = 'c';
            }
            break;
        case 3:
            if(numPressed == 0)
            {
                toReturn = 'd';
            }
            else if(numPressed == 1)
            {
                toReturn = 'e';
            }
            else
            {
                toReturn = 'f';
            }
            break;
        case 4:
            if(numPressed == 0)
            {
                toReturn = 'g';
            }
            else if(numPressed == 1)
            {
                toReturn = 'h';
            }
            else
            {
                toReturn = 'i';
            }
            break;
        case 5:
            if(numPressed == 0)
            {
                toReturn = 'j';
            }
            else if(numPressed == 1)
            {
                toReturn = 'k';
            }
            else
            {
                toReturn = 'l';
            }
            break;
        case 6:
            if(numPressed == 0)
            {
                toReturn = 'm';
            }
            else if(numPressed == 1)
            {
                toReturn = 'n';
            }
            else
            {
                toReturn = 'o';
            }
            break;
        case 7:
            if(numPressed == 0)
            {
                toReturn = 'p';
            }
            else if(numPressed == 1)
            {
                toReturn = 'q';
            }
            else if(numPressed == 2)
            {
                toReturn = 'r';
            }
            else
            {
                toReturn = 's';
            }
            break;
        case 8:
            if(numPressed == 0)
            {
                toReturn = 't';
            }
            else if(numPressed == 1)
            {
                toReturn = 'u';
            }
            else
            {
                toReturn = 'v';
            }
            break;
        case 9:
            if(numPressed == 0)
            {
                toReturn = 'w';
            }
            else if(numPressed == 1)
            {
                toReturn = 'x';
            }
            else if(numPressed == 2)
            {
                toReturn = 'y';
            }
            else
            {
                toReturn = 'z';
            }
            break;
        case 0:
            toReturn = ' ';
            break;
        case 10:
            if(strlen(send_message)){
                toReturn = '\0';
            }
            break;
        case 11:
            for(i = 0; i < strlen(send_message); i++){
                UartPutChar(send_message[i]);
            }
            //HTTP POST
            strncat(DATA1, send_message, strlen(send_message));
            strncat(DATA1, DATA1AFTER, strlen(DATA1AFTER));
            Message("Posted this:\n");
            Report(DATA1);
            http_post(lRetVal);
            break;
        default:
            toReturn = prev;
            break;
    }


    return toReturn;
}




//*****************************************************************************
//
//! Main 
//!
//! \param  none
//!
//! \return None
//!
//*****************************************************************************
void main() {
    //
    // Initailizing the board
    //
    BoardInit();
    //
    // Muxing for Enabling UART_TX and UART_RX.
    //
    PinMuxConfig();
    //
    // Initialising the Terminal.
    //
    InitTerm();



    //
    // Initialising the Terminal.
    //
    //CHANGED CONSOLE_PERIPH TO PCRM FOR UART1!!!!!!!!!!!!!!!!!!!!!
    MAP_UARTConfigSetExpClk(CONSOLE1,MAP_PRCMPeripheralClockGet(PRCM_UARTA1),//CONSOLE_PERIPH),
                            UART_BAUD_RATE,
                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                            UART_CONFIG_PAR_NONE));



    //
    // Register interrupt handler for UART
    //
    MAP_UARTIntRegister(CONSOLE1,UARTIntHandler);
    uint64_t ulStatus2 = MAP_UARTIntStatus(CONSOLE1, false);
    MAP_UARTIntClear(CONSOLE1, ulStatus2); // clear interrupts on GPIOA2
    //
    // Enable interrupts for uart1 on receive mask
    //
    MAP_UARTIntEnable(CONSOLE1,UART_INT_RX);


    //OLED STUFF
    //
    // Enable the SPI module clock
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI,PRCM_RUN_MODE_CLK);

    MAP_PRCMPeripheralReset(PRCM_GSPI);

    MAP_SPIReset(GSPI_BASE);

    //
    // Configure SPI interface
    //
    MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_OFF |
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8));

    //
    // Enable SPI for communication
    //
    MAP_SPIEnable(GSPI_BASE);

    //
    // Print mode on uart
    //
    //MAP_UtilsDelay(800000);


    //
    // Enable Chip select
    //
    //MAP_SPICSEnable(GSPI_BASE);

    Adafruit_Init();
    fillScreen(BLACK);





    //
    // Clearing the Terminal.
    //
    ClearTerm();
    DisplayBanner(APP_NAME);
    Message("\t\t****************************************************\n\r");
    Message("\t\t\t        CC3200 UART Echo Usage        \n\r");
    Message("\t\t Type in a string of alphanumeric characters and  \n\r");
    Message("\t\t pressenter, the string will be echoed. \n\r") ;
    Message("\t\t Note: if string length reaches 80 character it will \n\r");
    Message("\t\t echo the string without waiting for enter command \n\r");
    Message("\t\t ****************************************************\n\r");
    Message("\n\n\n\r");
    Message("cmd#");

    // main.c
    // ... other board initializations ...
    //systick_wrapped = 1;
    systick_expired = 1;
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);
    MAP_SysTickIntRegister(SysTickHandler);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();

    // Register the interrupt handlers
    MAP_GPIOIntRegister(IR_GPIO_PORT, GPIOA0IntHandler);

    // Configure interrupts on rising and falling edges JUST FALLING
    MAP_GPIOIntTypeSet(IR_GPIO_PORT, IR_GPIO_PIN, GPIO_FALLING_EDGE); // read ir_output
    uint64_t ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, false);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus); // clear interrupts on GPIOA2

    // Enable interrupts on ir_output
    MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);

    uint64_t temp_buffer;
    char send_message[] = "";
    int counter1 = 0;
    int prevButton = -1;
    char prevChar;



    //HTTP SETUP TO AWS
    //MADE GLOBALlong lRetVal = -1;
    UART_PRINT("My terminal works!\n\r");

    // initialize global default app configuration
    g_app_config.host = SERVER_NAME;
    g_app_config.port = GOOGLE_DST_PORT;

    //Connect the CC3200 to the local access point
    lRetVal = connectToAccessPoint();
    //Set time so that encryption can be used
    lRetVal = set_time();
    if(lRetVal < 0) {
        UART_PRINT("Unable to set time in the device");
        LOOP_FOREVER();
    }
    //Connect to the website with TLS encryption
    lRetVal = tls_connect();
    if(lRetVal < 0) {
        ERR_PRINT(lRetVal);
    }
    //http_post(lRetVal);

    //sl_Stop(SL_STOP_TIMEOUT);

    while(1) {
        while(gotmsg == 0){
            http_get(lRetVal);
            if(gotmsg == 1){
                fillScreen(RED);
                UART_PRINT("PARSED MESSAGE\n");
                UART_PRINT(getreqmsg);
            }
        }

        //DISPLAY Received message ON OLED

        if(counter == 49){
            temp_buffer = buffer;
            button = buttonDecode(&temp_buffer);
            Report("Measured Waveform: %d", button);
            counter = 0;
            char ch;
            ch = selectChar(button, prevButton, &counter1, send_message, prevChar);
            if (button == 1)
            {
                if(ch == 'd'){
                    char msg[5] = "safe";
                    //strncat(send_message, &msg, 4);
                    //HTTP POST
                    DATA1[0] = '\0';
                    strncat(DATA1, DATA1BEFORE, strlen(DATA1BEFORE));
                    strncat(DATA1, msg, strlen(msg));
                    strncat(DATA1, DATA1AFTER, strlen(DATA1AFTER));
                    Message("Posted safe:\n");
                    Report(DATA1);
                    http_post(lRetVal);
                    //fillRect(0,0,width(),height(),GREEN);
                    fillScreen(GREEN);
                    setCursor(0,height()/2);
                    setTextSize(1);
                    Outstr("SAFE");
                    gotmsg = 0;
                }else if(ch == 'a'){
                    char msg[4] = "ack";
                    //strncat(send_message, &msg, 3);
                    //HTTP POST
                    DATA1[0] = '\0';
                    strncat(DATA1, DATA1BEFORE, strlen(DATA1BEFORE));
                    strncat(DATA1, msg, strlen(msg));
                    strncat(DATA1, DATA1AFTER, strlen(DATA1AFTER));
                    Message("Posted crash:\n");
                    Report(DATA1);
                    http_post(lRetVal);
                    //fillRect(0,0,width(),height(),RED);
                    fillScreen(RED);
                    setCursor(0,height()/2);
                    setTextSize(1);
                    Outstr("CRASH");
                    gotmsg = 0;
                }else{
                    //strncat(send_message, &ch, 1);
                    //fillRect(0,0,width(),height(),BLACK);
                    fillScreen(YELLOW);
                }
                button = -1;
                prevButton = 0;
            }
            prevChar = ch;
            prevButton = button;

        }
    }
}
//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************

static int http_post(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, POSTHEADER);
    pcBufHeaders += strlen(POSTHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    int dataLength = strlen(DATA1);

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    //ADD OUR OWN DATA
    strcpy(pcBufHeaders, DATA1);
    pcBufHeaders += strlen(DATA1);

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("POST failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

    return 0;
}

static int http_get(int iTLSSockID){
    char acSendBuff[512];
    char acRecvbuff[1460];
    char cCLLength[200];
    char* pcBufHeaders;
    int lRetVal = 0;

    pcBufHeaders = acSendBuff;
    strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    strcpy(pcBufHeaders, "\r\n\r\n");

    /**int dataLength = strlen(DATA1);

    strcpy(pcBufHeaders, CTHEADER);
    pcBufHeaders += strlen(CTHEADER);
    strcpy(pcBufHeaders, CLHEADER1);

    pcBufHeaders += strlen(CLHEADER1);
    sprintf(cCLLength, "%d", dataLength);

    strcpy(pcBufHeaders, cCLLength);
    pcBufHeaders += strlen(cCLLength);
    strcpy(pcBufHeaders, CLHEADER2);
    pcBufHeaders += strlen(CLHEADER2);

    strcpy(pcBufHeaders, DATA1);
    pcBufHeaders += strlen(DATA1);**/

    int testDataLength = strlen(pcBufHeaders);

    UART_PRINT(acSendBuff);


    //
    // Send the packet to the server */
    //
    lRetVal = sl_Send(iTLSSockID, acSendBuff, strlen(acSendBuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("GET failed. Error Number: %i\n\r",lRetVal);
        sl_Close(iTLSSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
        return lRetVal;
    }
    lRetVal = sl_Recv(iTLSSockID, &acRecvbuff[0], sizeof(acRecvbuff), 0);
    if(lRetVal < 0) {
        UART_PRINT("Received failed. Error Number: %i\n\r",lRetVal);
        //sl_Close(iSSLSockID);
        //GPIO_IF_LedOn(MCU_RED_LED_GPIO);
           return lRetVal;
    }
    else {
        acRecvbuff[lRetVal+1] = '\0';
        UART_PRINT(acRecvbuff);
        UART_PRINT("\n\r\n\r");
    }

//START
    /**pcBufHeaders = acRecvbuff;
    //strcpy(pcBufHeaders, GETHEADER);
    pcBufHeaders += strlen(GETHEADER);
    //strcpy(pcBufHeaders, HOSTHEADER);
    pcBufHeaders += strlen(HOSTHEADER);
    //strcpy(pcBufHeaders, CHEADER);
    pcBufHeaders += strlen(CHEADER);
    //strcpy(pcBufHeaders, "\r\n\r\n");**/

    /**if(strstr(acRecvbuff, "bfv") != NULL){
        strcpy(getreqmsg,"bfv");
    }**/

    if(strstr(acRecvbuff, "crashed") != NULL){
        strcpy(getreqmsg,"crashed");
        gotmsg = 1;
    }

    //strcpy(getreqmsg, pcBufHeaders);


//END
    return 0;
}
