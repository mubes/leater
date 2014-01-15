/*
 * Timer management.  There are an unlimited number of timers (they are instances of timerType which are
 * managed as static data by the caller) and they are organised into a list.  The timers are 'tickless' in
 * that they don't take any CPU time until they actually mature.
 *
 * Timers on the LPC812 aren't super-accurate because the chip only supports intervals (avoiding using up the SCT)...that means that
 * when something matures there will always be some inaccuracy while correcting the remaining timeouts, but
 * the differences are _very_ small in comparison with the 1% accuracy of the internal oscillator.
 *
 */

#include <LPC8xx.h>

#include "config.h"
#include "dutils.h"
#include "flags.h"
#include "timers.h"
#include "printf.h"
#include "uart.h"
#include "bod.h"
#include "profile.h"
#include "heater.h"
#include "command.h"
#include "sensor.h"
#include "ledflash.h"

// WKT timer runs at 750KHz, and we want 1mS resolution timers
#define TICKS_PER_MS 750
#define TICKS_PER_WEEK (1000*60*60*24*7)

#define WKT_ALARM (1<<1)		// Counter alarm triggered
#define WKT_CLEAR (2<<1)		// Clear the counter and stop it counting

#define MAX_TIMEOUT 0xFFFFFFFF

// Approximate of milliseconds since system boot
static volatile uint32_t _ticks;

// Number of weeks since system boot
static int32_t _weeks;

// Head of the timer linked list
static timerType *timerHead;

// Time to the next timer to mature
static volatile uint32_t _nextTO;

// ============================================================================================
void WKT_IRQHandler(void)

// Interrupt handler for WKT tickout

{
  LPC_WKT->CTRL=WKT_ALARM|WKT_CLEAR;	// Acknowledge the interrupt and stop the counter

  // While we wait for the next timer to be started we run an interim timer so the time gets accounted for...
  LPC_WKT->COUNT=MAX_TIMEOUT;			// ... get the timer running again as quickly as possible

  // ...now account for this time
  _ticks+=_nextTO/TICKS_PER_MS;	// Soak up the ticks from this timer maturation
  _nextTO=MAX_TIMEOUT;

  flag_post(FLAG_TICK);			// Let the base layer know something has to happen
}
// ============================================================================================
void _setTimeout(void)

// Updates timer to next timeout period
// This routine is called with interrupts off (i.e. in a critical section)

{
	uint32_t newTO,currentReading;

	// We are changing the timeout, so soak up whatever had been used - get the reading
	while ((currentReading=LPC_WKT->COUNT)!=LPC_WKT->COUNT);

	// ...and stop the counter
	LPC_WKT->CTRL=WKT_CLEAR;

	if (currentReading<=_nextTO)
		_ticks+=(_nextTO-currentReading)/TICKS_PER_MS;
	else
		// This is just in case we wrapped around
		_ticks+=_nextTO/TICKS_PER_MS;

	// Now find the time for the next one to mature - be aware the interval timer is still running here
	if (timerHead)
	{
		if (_ticks>=timerHead->timeout )
		{
			// There's still a timer here to process
			newTO=MAX_TIMEOUT;
			flag_post(FLAG_TICK);
		}
		else
		{
			newTO=(timerHead->timeout-_ticks)*TICKS_PER_MS;
		}
	}
	else
		newTO=MAX_TIMEOUT;

	// Whatever happened, this is our new timeout value
	_nextTO=newTO;

	LPC_WKT->COUNT=_nextTO;
}
// ============================================================================================
void _removeFromList(timerType *t)

// Remove a timer from the list, updating as necessary
// does not adjust the next timer to expire header

{
	if (t->prev)
		t->prev->next = t->next;
	else
		// We were next to expire
		timerHead = t->next;

	// Deal with the backlink for all cases
	if (t->next) t->next->prev = t->prev;

	// Flag this timer to allow it to be restarted
	t->origin = TIMER_ORIGIN_ILLEGAL;
}
// ============================================================================================
uint32_t _getTicks(void)

{

	uint32_t val;
	denter_critical();

	// Make sure we get a stable reading from the counter - no protection against clock edges is provided!
	while ((val=LPC_WKT->COUNT) != LPC_WKT->COUNT);
	val=_ticks+((_nextTO-val)/TICKS_PER_MS);

	dleave_critical();
  return val;
}
// ============================================================================================
void _add(timerType *newTimer, timerOriginType originSet, uint32_t numberSet)

// Add a timer to the timer list (i.e. it'll mature at some point in the future)

{
	timerType *c = timerHead;
	timerType *p = 0;

	ASSERT(newTimer->origin == TIMER_ORIGIN_ILLEGAL);

	newTimer->origin = originSet;
	newTimer->number = numberSet;

	denter_critical();
	// Traverse list looking for next largest timer
	while ((c) && ((newTimer->timeout > c->timeout)))
	{
		p = c;
		c = c->next;
	}

	if (!p)
	{
		// We are at head
		newTimer->prev = 0;
		newTimer->next = timerHead;
		timerHead = newTimer;
		_setTimeout();
	}
	else
	{
		// We are somewhere in the list
		newTimer->prev = p;
		newTimer->next = p->next;
		p->next = newTimer;
	}

	if (newTimer->next)
	{
		// Deal with backlinks
		newTimer->next->prev = newTimer;
	}
	dleave_critical();
}
// ============================================================================================
// ============================================================================================
// ============================================================================================
// Externally available routines
// ============================================================================================
// ============================================================================================
void timersInit(void)

// Initialise the timer system

{
	LPC_SYSCON ->SYSAHBCLKCTRL |= (1 << 9);     // Enable register access to the WKT
	LPC_SYSCON ->PRESETCTRL |= (1 << 9);     	// Clear WKT reset
	LPC_WKT ->CTRL = WKT_CLEAR|WKT_ALARM; 				// Clear counter
	NVIC_EnableIRQ(WKT_IRQn);

	LPC_WKT->COUNT=MAX_TIMEOUT;			// ... get the timer running again as quickly as possible
	_nextTO=MAX_TIMEOUT;
}
// ============================================================================================
BOOL timerRunning(timerType *t)

// Return if this timer is active or not

{
	if (t->origin == TIMER_ORIGIN_ILLEGAL) return FALSE;
	else
		return TRUE;
}
// ============================================================================================
void timerDel(timerType *t)

// Remove a timer from running list and make it available to re-run

{
	ASSERT(t->origin != TIMER_ORIGIN_ILLEGAL);

	denter_critical();
	if (t->origin != TIMER_ORIGIN_ILLEGAL)
	{
		_removeFromList(t);
		_setTimeout();
	}

	dleave_critical();
}
// ============================================================================================
void timerInit(timerType *t)

// Initalise an individual timer

{
	t->origin = TIMER_ORIGIN_ILLEGAL;
}
// ============================================================================================
void timerAdd(timerType *newTimer, timerOriginType originSet, uint32_t numberSet, uint32_t timeoutSet)

// Add a timer to the timer list with an absolute increment

{
	newTimer->timeout = _getTicks()+timeoutSet;
	_add(newTimer,originSet,numberSet);
}
// ============================================================================================
void timerAddInc(timerType *newTimer, timerOriginType originSet, uint32_t numberSet, uint32_t timeoutIncSet)

// Add a timer to the timer list with an increment since last maturation

{
  uint32_t diff=_getTicks()-newTimer->timeout;
  newTimer->timeout=_getTicks()+((diff > timeoutIncSet)?1:(timeoutIncSet-diff));
  _add(newTimer,originSet,numberSet);
}
// ============================================================================================
#ifdef DEBUG
void timerDump(void)

// Dump all of the timers (debug function)

{
	timerType *t = timerHead;
	printf("\n");
	while (t)
	{
		printf("Origin %d, Timeout %d : ", t->origin, t->timeout-_getTicks());
		t = t->next;
	}
	printf("\n");
}
#endif
// ============================================================================================
uint32_t timerSecs(void)

// Get number of second ticks since system started

{
	return ((_weeks*TICKS_PER_WEEK)/mS) + (_getTicks()/mS);
}
// ============================================================================================
void timerDispatch(void)

// Dispatch a timer that has matured

{
  // At least one timer has matured - handle it, taking into account any time since timers were triggered
  while ((timerHead) && (timerHead->timeout<=_getTicks()))
    {
      denter_critical();

    // This really _should_ be _ticks and not _getTicks because we only want to do this when the value in _ticks is itself > 1 week
    if (_ticks>TICKS_PER_WEEK)
	{
	  // We've got over one week on the timers, so roll them back
	  timerType *c = timerHead;

	  while (c)
	    {
	      c->timeout-=TICKS_PER_WEEK;
	      c=c->next;
	    }
	  _ticks-=TICKS_PER_WEEK;
	  _weeks++;
	}

      uint32_t maturedTimerNumber = timerHead->number;
      timerOriginType origin = timerHead->origin;
      _removeFromList(timerHead);
      _setTimeout();
      dleave_critical();

      switch (origin)
	{
	case TIMER_ORIGIN_STATEMACHINE:
	  stateTimeout(maturedTimerNumber);
			break;

		case TIMER_ORIGIN_UART:
			uartTimeout(maturedTimerNumber);
			break;

		case TIMER_ORIGIN_BOD:
			bodTimeout();
			break;

		case TIMER_ORIGIN_PROFILE:
			profileTimeout();
			break;

		case TIMER_ORIGIN_LEDFLASH:
			ledTimeout();
			break;

		case TIMER_ORIGIN_HEATER:
			heaterTimeout();
			break;

		case TIMER_ORIGIN_COMMAND:
			commandRefreshPrompt();
			break;

		case TIMER_ORIGIN_SENSOR:
			sensorTimeout();
			break;

		default:
			ASSERT(FALSE);
			;
		}
	}
}
// ============================================================================================
