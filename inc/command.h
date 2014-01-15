/*
 * Handler for all command interaction. Parses commands from the uart (or other source)
 * and dispatches events to the correct module via the various COMMAND(x) routines.
 */

#ifndef COMMAND_H_
#define COMMAND_H_

#include <LPC8xx.h>
#include "config.h"
#include "uart.h"

// ============================================================================================
void commandReportLine(char *fmt, ...);           	// Report line when logging is enabled and keep printing tidy
void commandprintf(char *fmt, ...);					// Handle simple print
void commandRefreshPrompt(void);					// Signal to refresh the prompt because something in it has changed

void commandUartUnlocked(void);					  	// Callback that input is now being accepted
void commandHandleException(uartExceptionType e);	// Callback that something interesting happend on the UART

void commandInit(void);								// Initialise this module
// ============================================================================================

#endif /* COMMAND_H_ */
