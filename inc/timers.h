/*
 * Timer management.  There are an unlimited number of timers (they are instances of timerType which are
 * managed as static data by the caller) and they are organised into a list.  The timers are 'tickless' in
 * that they don't take any CPU time until they actually mature.
 *
 * Timers on the LPC812 aren't super-accurate because the chip only supports intervals...that means that
 * when something matures there will always be some inaccuracy while correcting the remaining timeouts, but
 * the differences are _very_ small in comparison with the 1% accuracy of the internal oscillator.
 *
 */

#ifndef TIMERS_H_
#define TIMERS_H_

#include <LPC8xx.h>

#include "config.h"
#include "flags.h"
#include "statemachine.h"

// Origins (and callbacks) for timer events
typedef enum {TIMER_ORIGIN_ILLEGAL, TIMER_ORIGIN_STATEMACHINE, TIMER_ORIGIN_UART, \
			  TIMER_ORIGIN_BOD, TIMER_ORIGIN_PROFILE, TIMER_ORIGIN_LEDFLASH, TIMER_ORIGIN_HEATER, \
			  TIMER_ORIGIN_COMMAND, TIMER_ORIGIN_SENSOR } timerOriginType;

typedef struct timerStruct
{
	timerOriginType origin;
	uint32_t number;
	uint32_t timeout;
	struct timerStruct *next;
	struct timerStruct *prev;
} timerType;

// ============================================================================================
#ifdef DEBUG
void timerDump(void);						// Debug - output all running timers
#endif
void timerAdd(timerType *newTimer, timerOriginType origin, uint32_t numberSet, uint32_t timeoutSet);  // Add a timer to runlist
void timerAddInc(timerType *newTimer, timerOriginType originSet, uint32_t numberSet, uint32_t timeoutIncSet); // Add increment timer to runlist
void timerDel(timerType *t);				// Delete a running timer
BOOL timerRunning(timerType *t);			// Return if this timer is active or not
void timerDispatch(void);					// Called when a timer has matured
uint32_t timerSecs(void);					// Get number of second ticks since system started

void timersInit(void);						// Initalise timer subsystem
void timerInit(timerType *t);				// Initialise an individual timer
// ============================================================================================

#endif /* TIMERS_H_ */
