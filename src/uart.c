/*
 * Low level UART handler routines.  These originally were based on the ROM routines, but they
 * added little and mean we can't see exactly what is going on.
 *
 */

#include <LPC8xx.h>
#include "config.h"
#include "uart.h"
#include "flags.h"
#include "timers.h"
#include "command.h"

// ============================================================================================

static char uartrxbuff[UART_RX_LINELEN];       		// Incoming line buffer...
static char *uartRxPos; 	           				// ..and position in it (initialised in the unlock)
static BOOL buffUnlocked = FALSE;                 	// is the buffer locked for writing to?
static timerType tuart;						   		// Timer for this state machine
uint32_t uartExceptionStore = UART_NO_EXCEPTION;    // ... any exceptions generated by the process
static volatile char rxedChar;						// Character received from UART in interrupt layer
dringbuffer_create(txBuffer,UART_TX_BUFFSIZE);		// Create the transmission buffer

#define RXRDY (1<<0)
#define TXRDY (1<<2)
// ============================================================================================
void UART0_IRQHandler(void)

// UART Reception interrupt handler

{
	// Reception side
	if (UART ->STAT & RXRDY)
	{
		rxedChar=(UART->RXDATA&0xFF);
		flag_post(FLAG_UARTRX);
	}

	// Transmission side
	if (UART->STAT & TXRDY)
	{
		if (dringbuffer_elements(txBuffer))
			UART ->TXDATA = dringbuffer_getByte(txBuffer);
		else
			UART ->INTENCLR = TXRDY;		// Nothing to transmit, so stop interrupting
	}
}
// ============================================================================================
char *uartGets(void)

// Return the string received by the UART

{
	*uartRxPos = 0;
	return uartrxbuff;
}
// ============================================================================================
uartExceptionType uartException(void)

// Return latest exception received by the UART

{
	uint32_t uartExceptionReturn = uartExceptionStore;
	uartExceptionStore = UART_NO_EXCEPTION;
	return uartExceptionReturn;
}
// ============================================================================================
void uartUnlockbuffer(void)

// Unlock UART to allow characters to be received

{
	uartRxPos = uartrxbuff;
	buffUnlocked = TRUE;
	commandUartUnlocked();
}
// ============================================================================================
void uartInsertChar(char c)

// Insert character into the receive buffer (but not from the UART)

{
	if (uartRxPos - uartrxbuff < UART_RX_LINELEN - 2)
	{
		*uartRxPos++ = c;
#ifdef USE_ECHO
		// Only echo if there's room for the echo
		uartPutchar(c);
#endif
	}
}
// ============================================================================================
void uartTimeout(uint32_t timerNumber)

// Timer has ticked out, so unlock the buffer

{
	uartUnlockbuffer();
}
// ============================================================================================
void uartInit(void)

// Initialise the UART subsystem

{
	LPC_SWM ->PINASSIGN0 &= ~0xFFFF;
	LPC_SWM ->PINASSIGN0 |= ((UART_TX_PIN) | (UART_RX_PIN << 8));     // Put pins in correct place on chip

	LPC_SYSCON ->SYSAHBCLKCTRL |= (1 << 14);     // Enable UART clock

	LPC_SYSCON ->PRESETCTRL &= ~0x08;
	LPC_SYSCON ->PRESETCTRL |= 0x08;     // Peripheral reset control to UART, a "1" bring it out of reset.
	LPC_SYSCON ->UARTCLKDIV = 2;
	UART ->BRG = SystemCoreClock / 16 / UART_BAUDRATE - 1;
	LPC_SYSCON ->UARTFRGDIV = 0xFF;
	LPC_SYSCON ->UARTFRGMULT = (((SystemCoreClock / 16) * (LPC_SYSCON ->UARTFRGDIV + 1)) / (UART_BAUDRATE * (UART ->BRG + 1)))
			- (LPC_SYSCON ->UARTFRGDIV + 1);
	UART ->CFG = (1 << 0) | (1 << 2);     // 8 bit, 1 stop, enabled

	NVIC_EnableIRQ(UART0_IRQn);
	UART ->INTENSET = RXRDY;

	// We don't want to listen for the first few seconds as the BT chip chatters
	timerInit(&tuart);
	timerAdd(&tuart, TIMER_ORIGIN_UART, 0, TIMER_INIT_DELAY);
}
// ============================================================================================
void uartPutchar(char c)

/* Send single character
 *
 * \param c Character to be sent
 */
{
	// If the buffer is full then spin waiting for it to empty
	while (dringbuffer_full(txBuffer));

	dringbuffer_putByte(txBuffer,c);
	UART ->INTENSET |= TXRDY;
}
// ============================================================================================
void uartPrintfPutchar(void *x, char c)

// Printf putchar routine

{
	uartPutchar(c);
#ifdef UART_USECRLF
	if (c == '\n') uartPutchar('\r');
#endif
}
// ============================================================================================
inline void uartSend(char *data, uint32_t len)

// Send multiple characters to the uart

{
	while (len--)
		uartPutchar(*data++);
}
// ============================================================================================
void uartPutsn(const char *data)

// Write string to uart with no newline

{
	while (*data)
		uartPutchar(*data++);
}
// ============================================================================================
void uartPuts(const char *data)

// Write string to uart with newline

{
	uartPutsn(data);
#ifdef UART_USECRLF
	uartPutsn("\n\r");
#else
	uartPutsn("\n");
#endif
}
// ============================================================================================
void uartPutin(uint32_t i, uint32_t base)

// Write an integer out, no newline

{
#define MAXNUMLEN 18
	char construct[MAXNUMLEN];        // String under construction - max length set here
	char *psn = &construct[MAXNUMLEN - 1];

	// Make sure we're null terminated
	*psn-- = 0;

	do
	{
		*psn-- = "0123456789ABCDEF"[i % base];
		i /= base;
	} while ((i) && (psn != construct));

	uartPutsn(++psn);
}
// ============================================================================================
void uartPuti(uint32_t i, uint32_t base)

// Write an integer out with newline

{
	uartPutin(i, base);
	uartPuts("");
}
// ============================================================================================
BOOL uartIsLocked(void)

// Return if UART is currently available

{
	return !buffUnlocked;
}
// ============================================================================================
void uartEvent(void)

// Handle uart reception - buffer management is done here, but no printing, that is done by callbacks

{
	if (buffUnlocked)
	{
		switch (rxedChar)
		{
		case '\r':
			*uartRxPos = 0;
			buffUnlocked = FALSE;
			commandHandleException(UART_LINE_RXED);
			break;

		case 1 ... 7:
		case 9 ... 12:
		case 14 ... 31:     // CTRL codes
			commandHandleException(UART_CTRL_CODE | (rxedChar << 24));
			break;

		case 8:
		case 127:
			// We deal with the delete here, but any pretty-printing is done elsewhere
			if (uartRxPos > uartrxbuff)
			{
				uartRxPos--;
				commandHandleException(UART_DELCODE);
			}
			break;

		default:
			if (uartRxPos - uartrxbuff < UART_RX_LINELEN - 2)
			{
				*uartRxPos++ = rxedChar;
				commandHandleException(UART_CHARRXED | (rxedChar << 24));
			}
			break;
		}
	}
}
// ============================================================================================
