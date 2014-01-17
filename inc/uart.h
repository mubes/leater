/*
 * Low level UART handler routines.  These originally were based on the ROM routines, but they
 * added little and mean we can't see exactly what is going on.
 *
 */

#ifndef _UART_
#define _UART_
#include "LPC8xx.h"
#include "config.h"

// ------------------------------ Configuration Parameters ---------------------------------------------
#ifndef UART_BAUDRATE
#error "Need to define UART_BAUDRATE"
#endif
#ifndef UART_TX_PIN
#error "Need to define UART_TX_PIN"
#endif
#ifndef UART_RX_PIN
#error "Need to define UART_RX_PIN"
#endif


#define UART                    LPC_USART0   // The particular UART to be using
#define TIMER_INIT_DELAY        3000    // Initalisation after 3 seconds
#define UART_RX_LINELEN         80      // Maximum length of an incoming line
#define UART_TX_BUFFSIZE        512     // Maximum number of bufferable characters on transmit side

#define USE_ECHO 1
#define UART_USECRLF            TRUE    // Use CRLF pair
// ------------------------------------------------------------------------------------------------------

#define UART_BASE_BINARY  2
#define UART_BASE_OCTAL   8
#define UART_BASE_DECIMAL 10
#define UART_BASE_HEX     16    // If you want a base > 16, then extend uartPuti accordingly

typedef uint32_t uartExceptionType;
#define UART_NO_EXCEPTION   0
#define UART_LINE_RXED      1
#define UART_CTRL_CODE      2
#define UART_DELCODE        3
#define UART_CHARRXED       4

#define UART_EXCEPTION_MASK 0xFF

void uartInit(void);                        // Init the UART to specified baudrate
void uartPutchar(char c);                   // Send single character
void uartPrintfPutchar(void *x, char c);    // Send single character (printf compatible version)
void uartSend(char *data, uint32_t len);    // Send the UART data

void uartPutsn(const char *data);
void uartPuts(const char *data);            // Send a string with newline (analagous to puts)

void uartPutin(uint32_t i, uint32_t base);
void uartPuti(uint32_t i, uint32_t base);   // Send an integer in arbitary base

void uartTimeout(uint32_t timerNumber);
void uartEvent(
    void);                       // A character received in the interrupt level handled in the base level

char *uartGets(void);                       // Get string entered from serial port
BOOL uartIsLocked(void);                    // Check to see if UART is currently locked
void uartInsertChar(char c);                // Insert a character into the receive buffer
void uartUnlockbuffer(
    void);                // Unlock buffer and make it available for input characters
uartExceptionType uartException(void);      // Return UART Exception code

#ifdef DEBUG
#define DBGuartPuts(x) uartPuts(x)
#define DBGuartPutsn(x) uartPutsn(x)
#define DBGuartPutin(a,x) uartPutin(a,x)
#define DBGuartPuti(a,x) uartPuti(a,x)
#else
#define DBGuartPuts(x)
#define DBGuartPutsn(x)
#define DBGuartPutin(a,x)
#define DBGuartPuti(a,x)
#endif
#endif
